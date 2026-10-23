r"""Capture a corpus of Terrain.dll draw passes for replay (Phase 5 step 3, re/PHASE5_TERRAIN_PLAN.md section 7).

    ~/bin/py -3.12 re/tools/gl_corpus.py --label c1

One boot of the ORIGINAL Terrain.dll code (SIMGOLF_HOOKS_OFF=all) driven through the sandbox: a built hole, cart paths,
terrain edit, the buildings-palette terrain tools, then every camera rotation (PgUp) at every zoom level (x / z). After each
step the last completed pass (first glPushMatrix .. glFlush, see gl_rec.js multi mode) is saved as
log/p5/gl/corpus/<label>/snaps/<step>/{meta.txt, pass.gl.txt, vtx.bin, nrm.bin, dib_pre.bin, dib_post.bin}; the shared state stream
(stream.gl.txt: every non-pass call after the process start) and the texture bytes (tex/<hash>.bin) sit in the corpus root.
Replay one with: glreplay.exe <root> <step>.   `--only a,b` runs just the named steps (plus the sandbox start).
"""
import argparse
import pathlib
import sys
import threading
import time

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "re" / "tools"))
import game  # noqa: E402
import scenario  # noqa: E402

OUT = ROOT / "log" / "p5" / "gl" / "corpus"
NL = chr(10)
VK_PGUP, VK_X, VK_Z, VK_E, VK_ESC, VK_RIGHT = 0x21, 0x58, 0x5A, 0x45, 0x1B, 0x27
NO_RETRY = {"paths_prep", "golf_tools_prep", "edit_off", "terrain_tools_off"}   # steps that only move UI state


class Collector:
    def __init__(self):
        self.tex = {}
        self.reset()
        self.chunks = {}
        self.done = []
        self.info = {}
        self.ev = threading.Event()

    def reset(self):
        self.passlines, self.meta, self.data = {}, {}, {}

    def on_msg(self, m, d):
        p = m.get("payload") if m.get("type") == "send" else None
        if not isinstance(p, dict) or "glrec" not in p:
            return
        k = p["glrec"]
        if k == "tex":
            self.tex[p["hash"]] = bytes(d)
        elif k == "passlines":
            self.passlines[p["i"]] = p["lines"]
        elif k == "snapmeta":
            self.meta.update(p)
        elif k == "snapdata":
            self.data[p["kind"]] = bytes(d)
        elif k == "snapend":
            self.ev.set()
        elif k == "chunk":
            self.chunks[p["i"]] = p["lines"]
        elif k == "done":
            self.done.append(p)
        else:
            self.info.update(p)


def save_snap(root, name, c):
    d = root / "snaps" / name
    d.mkdir(parents=True, exist_ok=True)
    lines = [l for i in sorted(c.passlines) for l in c.passlines[i]]
    (d / "pass.gl.txt").write_text(NL.join(lines) + NL)
    for kind, fn in (("pre", "dib_pre.bin"), ("post", "dib_post.bin"), ("vtx", "vtx.bin"), ("nrm", "nrm.bin"),
                     ("tobj", "tobj.bin"), ("tdata", "tdata.bin")):
        if kind in c.data:
            (d / fn).write_bytes(c.data[kind])
    m = c.meta.get("meta", {})
    with open(d / "meta.txt", "w") as f:
        for k in ("w", "h", "wb", "bpp", "biHeight", "comp", "m0", "m1", "m2"):
            f.write(f"{k} {m.get(k, 0)}" + NL)
        for k in ("stateIdx", "maxIdx"):
            f.write(f"{k} {c.meta.get(k, -1)}" + NL)
        # the exported call that started the pass (1 stripRender, 2 render, 3 localRender, 4 pathUpdateRender): this, args as raw dwords
        e = c.meta.get("entry") or {}
        for k in ("kind", "ecx", "a0", "a1", "a2"):
            f.write(f"entry_{k} {e.get(k, 0)}" + NL)
        f.write(f"tobjAddr {c.meta.get('tobjAddr', 0)}" + NL)
        f.write(f"passLines {len(lines)}" + NL)
    return len(lines), c.meta.get("maxIdx", -1)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--label", required=True)
    ap.add_argument("--only", default="", help="comma list of step names to run")
    ap.add_argument("--settle", type=float, default=1.6)
    a = ap.parse_args()
    only = {s for s in a.only.split(",") if s}
    root = OUT / a.label
    root.mkdir(parents=True, exist_ok=True)
    c = Collector()
    results = []
    g = game.Game(extra_js=[str(ROOT / "re" / "frida" / "js" / "gl_rec.js")],
                  env={"SIMGOLF_SKIP_INTRO": "1", "SIMGOLF_HOOKS_OFF": "all"})
    g.on_message = c.on_msg
    with game.preserve_saves(), g:   # leave original/saved games as found (the port boots read it as the install)
        t0 = time.time()
        while not g.hwnd() and time.time() - t0 < 30:
            time.sleep(0.25)
        rec = g.scripts[-1].exports_sync
        while "hooks" not in c.info and time.time() - t0 < 45:
            time.sleep(0.1)
        rec.glrec_snap(True, True, True)
        scenario.imgcmp.wait_for(g, scenario.imgcmp.load(scenario.GOLDEN / "main_menu.png"), timeout=60)
        scenario.to_sandbox(g)
        g.key(VK_ESC)
        time.sleep(1.5)

        def send(kind):
            c.reset()
            c.ev.clear()
            ok = rec.glrec_send_snap(kind)
            if ok:
                c.ev.wait(5)
            return ok

        def take(name, kinds=((2, ""), (3, "_local"), (4, "_pathupd"), (1, "_strip"))):
            # the last completed pass of each entry point since the previous snapshot: 2 Terrain::render (the full views, the
            # step's own name), 3 localRender, 4 pathUpdateRender, 1 stripRender (suffixed)
            got = False
            for kind, suffix in kinds:
                ok = send(kind)
                if not ok and kind == 2 and name not in NO_RETRY:   # a view change draws on the next camera move: nudge once
                    g.key(VK_RIGHT, hold=0.1); time.sleep(0.8); g.key(0x25, hold=0.1); time.sleep(a.settle)
                    ok = send(kind)
                if ok:
                    n, mx = save_snap(root, name + suffix, c)
                    results.append((name + suffix, "ok", n, mx))
                    print(f"  {name + suffix}: pass {n} lines, {mx + 1} vertices", flush=True)
                    got = True
            if not got:
                results.append((name, "no pass", 0, -1))
                print(f"  {name}: NO PASS since the last snapshot", flush=True)

        def step(name, fn, kinds=((2, ""), (3, "_local"), (4, "_pathupd"), (1, "_strip"))):
            if only and name not in only:
                return
            try:
                fn()
                time.sleep(a.settle)
            except Exception as e:   # a failed input must not lose the boot
                results.append((name, f"error {e!r}", 0, -1))
                print(f"  {name}: ERROR {e!r}", flush=True)
                return
            take(name, kinds)

        step("sandbox", lambda: g.key(VK_RIGHT, hold=0.1))
        step("hole", lambda: scenario.build_first_hole(g))

        def close_palette():
            g.key(VK_ESC); time.sleep(1.0)
            g.click(*scenario.GOLF_ICON); time.sleep(1.2)

        def golf_tools():
            # every Build Course palette slot placed once at a spot around the camera (types of terrain, bunkers, greens, tees ...)
            g.click(*scenario.GOLF_ICON); time.sleep(1.5)
            spots = [(300, 250), (360, 300), (420, 250), (480, 300), (540, 250), (600, 300), (330, 350), (400, 350), (470, 350)]
            for k, slot in enumerate(scenario.COV_B_GOLF_TOP + scenario.COV_B_GOLF_BOTTOM):
                g.click(*slot); time.sleep(0.8)
                x, y = spots[k % len(spots)]
                g.click(x, y); time.sleep(1.0)
                g.click(x, y + 60, right=True); time.sleep(0.6)
            g.click(*scenario.GOLF_ICON); time.sleep(1.0)

        def paths():
            # page 2 of the golf palette: the cart path tool dragged along a connected polyline of several tiles
            g.click(*scenario.GOLF_ICON); time.sleep(1.5)
            g.click(235, 527); time.sleep(1.5)
            g.click(*scenario.COV_B_PAGE2_ITEMS[0]); time.sleep(0.8)
            pts = [(360, 330), (430, 360), (500, 330), (560, 290), (520, 250), (450, 230)]
            for (x0, y0), (x1, y1) in zip(pts, pts[1:]):
                g.drag(x0, y0, x1, y1, steps=10, step_time=0.08)
                time.sleep(0.5)
            g.click(330, 300, right=True); time.sleep(0.8)
            for k, item in enumerate(scenario.COV_B_PAGE2_ITEMS[1:]):
                g.click(*item); time.sleep(0.8)
                x, y = [(380, 260), (560, 240)][k]
                g.drag(x, y, x + 60, y + 30)
                time.sleep(0.8)
                g.click(x - 40, y, right=True); time.sleep(0.8)

        step("golf_tools_prep", close_palette)
        step("golf_tools", golf_tools)
        step("paths_prep", close_palette)
        step("paths", paths)

        def edit():
            g.key(VK_E); time.sleep(1.5)
            for (x, y) in ((400, 300), (460, 260)):
                g.move(x, y); time.sleep(0.8)
                for _ in range(3):
                    g.key(0xBB, char="="); time.sleep(0.5)
            g.move(340, 340); time.sleep(0.6)
            for _ in range(2):
                g.key(0xBD, char="-"); time.sleep(0.5)

        step("edit_raise", edit)
        step("edit_off", lambda: (g.key(VK_E), time.sleep(1.0), g.key(VK_ESC)))

        def terrain_tools():
            g.click(118, 495); time.sleep(1.5)
            g.click(235, 527); time.sleep(1.5)
            for k, tool in enumerate(scenario.COV_B_TERRAIN_TOOLS):
                g.click(*tool); time.sleep(1.0)
                x, y = [(380, 260), (450, 300), (560, 240), (330, 330)][k]
                g.move(x, y); time.sleep(0.6)
                g.drag(x, y, x + 40, y + 20)
                time.sleep(0.8)
                g.click(x - 40, y + 20, right=True); time.sleep(0.8)

        step("terrain_tools", terrain_tools)
        step("terrain_tools_off", lambda: (g.key(VK_ESC), time.sleep(0.8), g.click(118, 495)))

        for zoom in range(3):
            if zoom:
                step(f"zoom{zoom}", lambda: g.key(VK_X))
            for rot in range(4):
                if rot or zoom == 0:
                    step(f"z{zoom}_r{rot}", lambda: g.key(VK_PGUP))
                else:
                    step(f"z{zoom}_r{rot}", lambda: time.sleep(0.1))
        for _ in range(2):
            step(f"zoomback{_}", lambda: g.key(VK_Z))

        # State fuzzing of the original (gl_rec.js glrecFuzz): random path / wall / hidden / type-7 / rotation fields on the
        # live tiles so the rarely taken branches of the render path run; the real state is restored after each pass.
        def fuzz(seed, probs):
            def go():
                # queued; applied by the game thread at the next Terrain::render entry and undone after that pass's glFlush
                base = rec.glrec_fuzz_done()
                rec.glrec_fuzz(seed, probs)
                t0 = time.time()
                while rec.glrec_fuzz_done() == base and time.time() - t0 < 12:
                    g.key(VK_RIGHT, hold=0.1); time.sleep(0.6); g.key(0x25, hold=0.1); time.sleep(0.6)
                print(f"    fuzz seed {seed}: ran {rec.glrec_fuzz_done() - base} mutated pass(es)", flush=True)
            return go

        PROBS = [0.25, 0.06, 0.04, 0.12]
        for seed in range(1, 7):
            if seed in (3, 5):
                step(f"view{seed}", lambda: g.key(VK_PGUP))
            step(f"fuzz{seed}_z0", fuzz(seed, PROBS), kinds=((22, ""),))
        step("view_zoom1", lambda: g.key(VK_X))
        for seed in range(7, 10):
            step(f"fuzz{seed}_z1", fuzz(seed, PROBS), kinds=((22, ""),))
        rec.glrec_dump()
        t0 = time.time()
        while not c.done and time.time() - t0 < 60:
            time.sleep(0.2)
        img = g.shot()
        img.save(root / "last.png")
    lines = [l for i in sorted(c.chunks) for l in c.chunks[i]]
    (root / "stream.gl.txt").write_text(NL.join(lines) + NL)
    (root / "tex").mkdir(exist_ok=True)
    for h, b in c.tex.items():
        (root / "tex" / f"{h}.bin").write_bytes(b)
    with open(root / "index.tsv", "w") as f:
        f.write("step" + chr(9) + "status" + chr(9) + "pass_lines" + chr(9) + "vertices" + NL)
        for name, st, n, mx in results:
            f.write(chr(9).join([name, st, str(n), str(mx + 1)]) + NL)
    ok = [r for r in results if r[1] == "ok"]
    print(f"stream {len(lines)} state lines, {len(c.tex)} textures, {len(ok)}/{len(results)} passes captured -> {root}")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
