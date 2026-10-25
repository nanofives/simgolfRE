r"""Record the GL call stream Terrain.dll issues for rendered frames (Phase 5 step 3, re/PHASE5_TERRAIN_PLAN.md 2.3).

    ~/bin/py -3.12 re/tools/gl_record.py --label sandbox_a [--flushes 1] [--move]
    ~/bin/py -3.12 re/tools/gl_record.py --scenario cov_b_holes18 --instance s3_1 --inject --label g1   # pinned, injected passes

The --scenario mode (roadmap S3.1) is described in run_pinned: a pinned scenario, the original Terrain.dll code, snapshots of the
draw-list passes (stripRender, pathUpdateRender injected on the game thread) in log/p5/gl/corpus/<label>/.

Boots the game (hooks as deployed), goes to the sandbox, arms re/frida/js/gl_rec.js for N glFlush calls (one per
rendered terrain strip pass / frame), optionally nudges the camera so the terrain re-renders, and writes
log/p5/gl/<label>.gl.txt (one call per line) plus <label>.png (the window right after). Run it twice with the same
label prefix and `cmp` the two files to see whether the stream is reproducible.
"""
import argparse
import hashlib
import json
import pathlib
import sys
import threading
import time

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "re" / "tools"))
# game.py fixes its install dir from SIMGOLF_GAME_DIR when imported, and scenario_ab sets it from --instance when IT is imported:
# in --scenario mode nothing may import game before scenario_refs has set the instance up (U-1078, re/PHASE5_TERRAIN_PLAN.md
# section 17). The walk modes import them in main().
game = scenario = None

OUT = ROOT / "log" / "p5" / "gl"
CORPUS = OUT / "corpus"
NL = chr(10)
F0, F90, F180, FM90 = 0, 0x42B40000, 0x43340000, 0xC2B40000   # angle bit patterns (the game passes -90 for the fourth view)
# --scenario --inject: the passes injected at the next outermost render / localRender after each chosen checkpoint (gl_rec.js
# glrecInject; 'P' = that call's pivot, 'A' = its angle): pathUpdateRender at the game's angle and the three others, then
# stripRender in every direction 1..8 at the game's angle, plus one direction outside 1..8 (draws nothing).
INJECT = ([["pathUpdateRender", "P", "A"]] + [["pathUpdateRender", "P", a] for a in (F0, F90, F180, FM90)]
          + [["stripRender", "P", d, "A"] for d in range(1, 9)] + [["stripRender", "P", 9, "A"]])
# --set wide: also every direction at the four angles, and with the zoom shift forced to 1 and to 2 (gl_rec.js ['zoom', z])
INJECT_WIDE = ([["pathUpdateRender", "P", "A"]] + [["pathUpdateRender", "P", a] for a in (F0, F90, F180, FM90)]
               + [["stripRender", "P", d, a] for a in (F0, F90, F180, FM90) for d in range(1, 9)]
               + [e for z in (1, 2) for e in ([["zoom", z]] + [["stripRender", "P", d, "A"] for d in range(1, 9)]
                                              + [["pathUpdateRender", "P", "A"]])])
# --set edge: the tiles fuzzed (gl_rec.js ['fuzz', seed, p]: path flags on half the tiles, walls, hidden and type-7 tiles), then
# passes whose pivot lies near the x = 0 / y = 0 edges, where pathUpdateRender's clipping quirks (left edge x + 1) and the strip
# windows' grid tests act
EDGE = [[3, 25], [0, 10], [8, 40], [25, 2], [46, 47]]
INJECT_EDGE = ([["fuzz", 0x5331, [0.5, 0.3, 0.05, 0.05]]]
               + [["pathUpdateRender", ["T", x, y], a] for x, y in EDGE for a in ("A", F90)]
               + [["stripRender", ["T", x, y], d, "A"] for x, y in EDGE[:3] for d in (2, 4, 6, 8)]
               + [["localRender", ["T", x, y], ["T", x, y], "A"] for x, y in EDGE])
# --set edge7: the same with most tiles made type 7 (the border ring is type 0x14 = hidden in every recorded game state, so
# pathUpdateRender's left clip at x + 1 can only show on a non-hidden column 0: S3.1 control 12)
INJECT_EDGE7 = [["fuzz", 0x5337, [0.6, 0.3, 0.0, 0.6]]] + INJECT_EDGE[1:]
# --zoom-early z: queued from process start, run at the first outermost render / localRender whose Terrain object has zoom shift z
INJECT_ZOOM = ([["pathUpdateRender", "P", a] for a in ("A", F0, F90, F180, FM90)]
               + [["stripRender", "P", d, a] for a in (F0, F90, F180, FM90) for d in range(1, 9)])


class SnapCollector:
    """gl_rec.js snapshot messages (multi mode) -> one dict per snapshot, in arrival order; plus the texture bytes and the stream."""

    def __init__(self):
        self.tex, self.snaps, self.chunks, self.done, self.info = {}, [], {}, [], {}
        self.cur = None
        self.lock = threading.Lock()

    def on_msg(self, m, d):
        p = m.get("payload") if m.get("type") == "send" else None
        if not isinstance(p, dict) or "glrec" not in p:
            return False
        k = p["glrec"]
        with self.lock:
            if k == "tex":
                self.tex[p["hash"]] = bytes(d)
            elif k == "passlines":
                if self.cur is None:
                    self.cur = {"passlines": {}, "meta": {}, "data": {}}
                self.cur["passlines"][p["i"]] = p["lines"]
            elif k == "snapmeta":
                if self.cur is None:
                    self.cur = {"passlines": {}, "meta": {}, "data": {}}
                self.cur["meta"].update(p)
            elif k == "snapdata":
                self.cur["data"][p["kind"]] = bytes(d)
            elif k == "snapend":
                self.snaps.append(self.cur)
                self.cur = None
            elif k == "chunk":
                self.chunks[p["i"]] = p["lines"]
            elif k == "done":
                self.done.append(p)
            else:
                self.info.update(p)
        return True


def save_corpus_snap(root: pathlib.Path, name: str, s: dict) -> int:
    """One snapshot in re/tools/gl_corpus.py's layout (port/test/glcorpus.py reads it): pass.gl.txt, the back buffer before / after,
    the arrays, the Terrain object and the DLL data image, meta.txt with the entry call."""
    d = root / "snaps" / name
    d.mkdir(parents=True, exist_ok=True)
    lines = [l for i in sorted(s["passlines"]) for l in s["passlines"][i]]
    (d / "pass.gl.txt").write_text(NL.join(lines) + NL)
    for kind, fn in (("pre", "dib_pre.bin"), ("post", "dib_post.bin"), ("vtx", "vtx.bin"), ("nrm", "nrm.bin"),
                     ("tobj", "tobj.bin"), ("tdata", "tdata.bin")):
        if kind in s["data"]:
            (d / fn).write_bytes(s["data"][kind])
    c = s["meta"]
    m = c.get("meta", {})
    with open(d / "meta.txt", "w") as f:
        for k in ("w", "h", "wb", "bpp", "biHeight", "comp", "m0", "m1", "m2"):
            f.write(f"{k} {m.get(k, 0)}" + NL)
        for k in ("stateIdx", "maxIdx"):
            f.write(f"{k} {c.get(k, -1)}" + NL)
        e = c.get("entry") or {}
        for k in ("kind", "ecx", "a0", "a1", "a2", "inj"):
            f.write(f"entry_{k} {e.get(k, 0)}" + NL)
        f.write(f"tobjAddr {c.get('tobjAddr', 0)}" + NL)
        f.write(f"passLines {len(lines)}" + NL)
    return len(lines)


INJECT_SETS = {"basic": INJECT, "wide": INJECT_WIDE, "edge": INJECT_EDGE, "edge7": INJECT_EDGE7}
KIND_NAME = {1: "strip", 2: "render", 3: "local", 4: "pathupd"}


def run_pinned(a) -> int:
    """--scenario: a pinned scenario replayed by re/tools/scenario_refs.py under the virtual clock (OFF arm: every shim hook off, so
    the ORIGINAL Terrain.dll code draws), with gl_rec.js appended to its agent in snapshot multi mode from process start. With
    --inject, INJECT is queued at each checkpoint (all, or those named by --at) and runs at the next outermost render / localRender
    on the game thread; the injected passes are fetched at the following checkpoints. At the scenario's last reply the last
    organic pass of every entry kind, the injected passes still waiting and the state stream are fetched. Output
    log/p5/gl/corpus/<label>/ (gl_corpus.py's layout, index.tsv, inject.json); the run itself is log/c3/alt/refs/<scenario>/<label>
    and its checkpoints compare with the pinned reference (scenario_refs.py check)."""
    if "game" in sys.modules:
        raise RuntimeError("game.py was imported before scenario_refs: the game would boot in the wrong install dir (U-1078)")
    import scenario_refs as R
    root = CORPUS / a.label
    if root.exists():
        raise SystemExit(f"{root} exists")
    (root / "snaps").mkdir(parents=True)
    c = SnapCollector()
    at = set(a.at.split(",")) if a.at else None
    queued = []           # (checkpoint, first injected sequence number)
    js = (ROOT / "re" / "frida" / "js" / "gl_rec.js").read_text()
    # snapshot multi mode with the Terrain object from the first call on (the textures are uploaded after the main menu)
    js += NL + "rpc.exports.glrecSnap(true, true, true);" + NL
    if a.zoom_early is not None:
        js += f"rpc.exports.glrecInject({json.dumps(INJECT_ZOOM)}, {{zoom: {a.zoom_early}}});" + NL
        queued.append((f"zoom{a.zoom_early}early", 1))
    names, rows = [], []
    orig_setup = R.setup

    def fetch(rec, label):
        n0 = len(c.snaps)
        k = rec.glrec_send_injected()
        t0 = time.time()
        while len(c.snaps) < n0 + k and time.time() - t0 < 120:
            time.sleep(0.05)
        print(f"  [gl] {label}: {k} injected passes fetched", flush=True)

    def install(S):
        Base = S.Run

        class GlRun(Base):
            def agent_source(self):
                p = super().agent_source()
                p.write_text(p.read_text() + NL + js)
                return p

            def _on_message(self, m, data):
                if c.on_msg(m, data):
                    return
                return super()._on_message(m, data)

            def _rec(self):
                return self.g.scripts[-1].exports_sync

            def take_dump(self, sc, name):
                rec = self._rec()
                fetch(rec, name)
                if a.inject and (at is None or name in at):
                    st = rec.glrec_injected()
                    if st["pending"]:
                        print(f"  [gl] {name}: the previous injection never ran (no render / localRender since)", flush=True)
                    rec.glrec_inject(INJECT_SETS[a.set])
                    queued.append((name, max((e["seq"] for e in st["log"]), default=0) + 1))
                return super().take_dump(sc, name)

            def post(self, sc, **kw):
                if "actions" in kw and not kw.get("next"):
                    rec = self._rec()
                    fetch(rec, "end")
                    for kind in (1, 2, 3, 4):
                        n0 = len(c.snaps)
                        if rec.glrec_send_snap(kind):
                            t0 = time.time()
                            while len(c.snaps) <= n0 and time.time() - t0 < 60:
                                time.sleep(0.05)
                    c.info["injected"] = rec.glrec_injected()
                    rec.glrec_dump()
                    t0 = time.time()
                    while not c.done and time.time() - t0 < 120:
                        time.sleep(0.05)
                return super().post(sc, **kw)

        S.Run = GlRun

    def setup(scen, inst):
        S = orig_setup(scen, inst)
        install(S)
        return S

    R.setup = setup
    ra = argparse.Namespace(instance=a.instance or R.DEFAULT_INSTANCES[0], scenario=a.scenario, label=a.label, schedule=None,
                            record=False, on=None, env=None, stop_after=0, clock_log=0, msg_log=0, trace=None, watch=None)
    rc = R.cmd_run(ra)
    log = {e["seq"]: e for e in (c.info.get("injected") or {}).get("log", [])}
    cp_of = lambda seq: max((q for q in queued if q[1] <= seq), key=lambda q: q[1], default=("?", 0))[0]
    for s in c.snaps:
        e = s["meta"].get("entry") or {}
        kind, inj = e.get("kind", 0), e.get("inj", 0)
        if inj:
            le = log.get(inj, {})
            name = f"i{inj:03d}_{cp_of(inj)}_{KIND_NAME.get(kind, kind)}"
            args = ",".join(le.get("args", []))
        else:
            name = f"organic_{KIND_NAME.get(kind, kind)}"
            args = ",".join(hex(e.get(k, 0)) for k in ("a0", "a1", "a2"))
        n = save_corpus_snap(root, name, s)
        names.append(name)
        rows.append((name, "ok", n, kind, inj, args))
    lines = [l for i in sorted(c.chunks) for l in c.chunks[i]]
    (root / "stream.gl.txt").write_text(NL.join(lines) + NL)
    (root / "tex").mkdir(exist_ok=True)
    for h, b in c.tex.items():
        (root / "tex" / f"{h}.bin").write_bytes(b)
    with open(root / "index.tsv", "w") as f:
        f.write("step\tstatus\tpass_lines\tentry_kind\tinj\targs" + NL)
        for r in rows:
            f.write("\t".join(str(x) for x in r) + NL)
    (root / "inject.json").write_text(json.dumps({"scenario": a.scenario, "queued": queued, "inject": INJECT_SETS[a.set],
                                                  "injected": c.info.get("injected"), "run": str(R.REFS / a.scenario / a.label),
                                                  "rc": rc, "textures": len(c.tex), "stream_lines": len(lines)}, indent=1))
    print(f"{len(names)} snapshots, {len(lines)} stream lines, {len(c.tex)} textures -> {root} (scenario_refs rc {rc})")
    return 0 if names and lines and rc == 0 else 1


def write_snap(d, lines, tex, snapd, snapm):
    """Replay inputs for tools/glexact/glreplay.c: stream.gl.txt, tex/<hash>.bin, vtx.bin, nrm.bin, dib_pre.bin, dib_post.bin, meta.txt."""
    (d / "tex").mkdir(parents=True, exist_ok=True)
    (d / "stream.gl.txt").write_text(chr(10).join(lines) + chr(10))
    for h, (p, data) in tex.items():
        (d / "tex" / f"{h}.bin").write_bytes(bytes(data))
    for k, fn in (("pre", "dib_pre.bin"), ("post", "dib_post.bin"), ("vtx", "vtx.bin"), ("nrm", "nrm.bin")):
        if k in snapd:
            (d / fn).write_bytes(bytes(snapd[k]))
    m = snapm.get("meta", {})
    with open(d / "meta.txt", "w") as f:
        for k in ("w", "h", "wb", "bpp", "biHeight", "comp", "m0", "m1", "m2"):
            f.write(f"{k} {m.get(k, 0)}" + chr(10))
        for k in ("startIdx", "endIdx", "maxIdx"):
            f.write(f"{k} {snapm.get(k, -1)}" + chr(10))
    print("snapshot:", {k: snapm.get(k) for k in ("startIdx", "endIdx", "maxIdx")}, m, "textures", len(tex), "->", d)


def main():
    global game, scenario
    ap = argparse.ArgumentParser()
    ap.add_argument("--label", required=True)
    ap.add_argument("--scenario", help="a pinned scenario replayed by scenario_refs.py (OFF arm) instead of the sandbox walk")
    ap.add_argument("--instance", default=None, help="--scenario: the install copy under instances/ (pinned winmm.dll)")
    ap.add_argument("--inject", action="store_true", help="--scenario: inject INJECT's passes after the checkpoints")
    ap.add_argument("--at", default="", help="--scenario --inject: comma list of checkpoint names (default: every checkpoint)")
    ap.add_argument("--set", choices=["basic", "wide", "edge", "edge7"], default="basic", help="--scenario --inject: INJECT, INJECT_WIDE or INJECT_EDGE")
    ap.add_argument("--zoom-early", type=int, default=None, help="--scenario: INJECT_ZOOM at the first pass with this zoom shift")
    ap.add_argument("--flushes", type=int, default=1)
    ap.add_argument("--move", action="store_true", help="press an arrow key to force a terrain re-render")
    ap.add_argument("--timeout", type=float, default=30)
    ap.add_argument("--hooks-off", action="store_true", help="run the ORIGINAL Terrain.dll code (SIMGOLF_HOOKS_OFF=all)")
    ap.add_argument("--init", action="store_true", help="record from process start through the sandbox load (no frame wait), marks at menu / sandbox")
    ap.add_argument("--snap", action="store_true", help="with --init: also capture the replay inputs of the last draw pass (textures, arrays, DIB before / after) into log/p5/gl/<label>/")
    ap.add_argument("--hole", action="store_true", help="build the first hole before recording")
    a = ap.parse_args()
    if a.scenario:
        return run_pinned(a)
    import game  # noqa: E402,F811
    import scenario  # noqa: E402,F811
    OUT.mkdir(parents=True, exist_ok=True)
    chunks, done, info = {}, [], {}

    tex, snapd, snapm = {}, {}, {}

    def on_msg(m, _d):
        p = m.get("payload") if m.get("type") == "send" else None
        if not isinstance(p, dict) or "glrec" not in p:
            return
        if p["glrec"] == "tex":
            tex[p["hash"]] = (p, _d)
        elif p["glrec"] == "snapmeta":
            snapm.update(p)
        elif p["glrec"] == "snapdata":
            snapd[p["kind"]] = _d
        elif p["glrec"] == "chunk":
            chunks[p["i"]] = p["lines"]
        elif p["glrec"] == "done":
            done.append(p)
        else:
            info.update(p)

    g = game.Game(extra_js=[str(ROOT / "re" / "frida" / "js" / "gl_rec.js")], env={"SIMGOLF_SKIP_INTRO": "1", **({"SIMGOLF_HOOKS_OFF": "all"} if a.hooks_off else {})})
    g.on_message = on_msg
    with g:
        t0 = time.time()
        while not g.hwnd() and time.time() - t0 < 30:
            time.sleep(0.25)
        if a.init:
            rec0 = g.scripts[-1].exports_sync
            if a.snap:
                t1 = time.time()
                while "hooks" not in info and time.time() - t1 < 15:
                    time.sleep(0.1)
                rec0.glrec_snap(True)
            imgcmp = scenario.imgcmp
            imgcmp.wait_for(g, imgcmp.load(scenario.GOLDEN / "main_menu.png"), timeout=60)
            rec0.glrec_mark("main_menu")
            scenario.to_sandbox(g)
            rec0.glrec_mark("sandbox")
            time.sleep(1.5)
            if a.snap:
                g.key(0x27, hold=0.1)       # nudge the camera so a fresh terrain pass is drawn
                time.sleep(1.5)
                ok = rec0.glrec_send_snap()
                print("snapshot sent:", ok, flush=True)
            rec0.glrec_dump()
            t0 = time.time()
            while not done and time.time() - t0 < 30:
                time.sleep(0.2)
            st = rec0.glrec_state()
            img = g.shot()
            img.save(OUT / f"{a.label}.png")
        else:
            scenario.to_sandbox(g)
        if not a.init:
            if a.hole:
                scenario.build_first_hole(g)
            rec = g.scripts[-1].exports_sync
            t0 = time.time()
            while "hooks" not in info and time.time() - t0 < 15:
                time.sleep(0.2)
            print("recorder:", info, flush=True)
            rec.glrec_arm(a.flushes)
            t0 = time.time()
            while not done and time.time() - t0 < a.timeout:
                if a.move:
                    g.key(0x27, hold=0.1)   # VK_RIGHT: camera moves, terrain re-renders
                time.sleep(0.5)
            st = rec.glrec_state()
            print('seen (all callers):', dict(sorted(rec.glrec_seen().items(), key=lambda kv: -kv[1])[:12]))
            img = g.shot()
            img.save(OUT / f"{a.label}.png")
    lines = [l for i in sorted(chunks) for l in chunks[i]]
    (OUT / f"{a.label}.gl.txt").write_text("\n".join(lines) + "\n")
    if a.snap:
        write_snap(OUT / a.label, lines, tex, snapd, snapm)
    names = {}
    for l in lines:
        n = l.split(" ", 1)[0]
        names[n] = names.get(n, 0) + 1
    print("done:", done, "state:", st)
    print("calls:", len(lines), "sha256:", hashlib.sha256("\n".join(lines).encode()).hexdigest()[:16])
    for n, c in sorted(names.items(), key=lambda kv: -kv[1]):
        print(f"  {c:8d} {n}")
    return 0 if done else 1


if __name__ == "__main__":
    sys.exit(main())
