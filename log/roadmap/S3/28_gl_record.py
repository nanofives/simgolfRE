r"""S3.28: the kind-5 passes (Terrain::lowerEdgeCorner's draw half, renderTileAtFiveOffsets 0x10038900) of a pinned scenario, the
original's code with hooks off; a copy of log/roadmap/S3/27_gl_record.py driving log/roadmap/S3/28_gl_rec.js.

    SIMGOLF_ALLOW_MULTI=1 C3FT_AUDIO=none ~/bin/py -3.12 log/roadmap/S3/28_gl_record.py --scenario cov_r_s2_screens --instance s3_w8 \
        --label s328_s2 --inject-at course

Every organic kind-5 pass (a lowerEdgeCorner call, also those that draw nothing) is a snapshot k<NN>_lowerEdge_o<ord> whose meta.txt
has entry_a0..a3 = lowerEdgeCorner's (tile, corner, pivot, angle). --inject-at <checkpoint>: at the next outermost render /
localRender after that checkpoint, renderTileAtFiveOffsets is called directly on the game thread for INJECT (tiles on the edges
of the land and one inner tile, each at the four view angles, the pivot two tiles inward so the tile is on screen); those
snapshots are i<seq>_<checkpoint>_rtafo with entry_direct 1 and entry_a0..a2 = (tile, pivot, angle). The back buffer is written
back after the injection (27_gl_rec.js runInject), and renderTileAtFiveOffsets changes no Terrain state but the view quadrant,
which the interrupted render sets again, so the run stays comparable with the pinned reference.

What follows is S3.27's description of the driver this was copied from:

re/tools/gl_record.py's --scenario mode with log/roadmap/S3/27_gl_rec.js (a copy of re/frida/js/gl_rec.js with an ordinal per
entry kind and a keep rule) in place of gl_rec.js: every kept pass (default: Terrain::render ordinals 0..2 and every localRender /
stripRender / pathUpdateRender begun after render 1 and before render 2) arrives as a snapshot in gl_corpus.py's layout under
log/p5/gl/corpus/<label>/snaps/k<NN>_<kind>_o<ord>/ (meta.txt also has entry_ord, entry_rord), the state stream in stream.gl.txt.
The run is log/c3/alt/refs/<scenario>/<label>; check it with re/tools/scenario_refs.py check --scenario <s> --run <label>.
"""
import argparse
import json
import pathlib
import sys
import time

ROOT = pathlib.Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "re" / "tools"))
import gl_record as GR   # noqa: E402  (imports neither game nor scenario)

NL = chr(10)
JS = pathlib.Path(__file__).resolve().parent / "28_gl_rec.js"
F0, F90, F180, FM90 = 0, 0x42B40000, 0x43340000, 0xC2B40000   # the angle bit patterns (the game passes -90 for the fourth view)
# (tile, pivot) pairs. cov_r_s2_screens holds its land at x, y = 12..37 at `course` (every other tile type 0x14 = hidden, read from
# the recorded Terrain object): (37, 25) / (12, 25) / (25, 37) / (25, 12) lie on its edges, so each draws at exactly the two angles
# whose neighbour set reaches the hidden side (0 and 90 / 180 and -90 / 90 and 180 / 0 and -90) and not at the other two, which
# tells the four neighbour sets apart; (25, 25) is inner (draws at none). The pivot two tiles inward keeps the tile on screen.
PAIRS = [((37, 25), (35, 25)), ((12, 25), (14, 25)), ((25, 37), (25, 35)), ((25, 12), (25, 14)), ((25, 25), (25, 25))]
INJECT = [["renderTileAtFiveOffsets", ["T", t[0], t[1]], ["T", p[0], p[1]], a] for a in (F0, F90, F180, FM90) for t, p in PAIRS]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--scenario", required=True)
    ap.add_argument("--instance", default=None)
    ap.add_argument("--label", required=True)
    ap.add_argument("--keep", default=None, help="JSON keep rule for 28_gl_rec.js glrecKeep (default: the JS's)")
    ap.add_argument("--inject-at", default=None, help="checkpoint after which INJECT runs (renderTileAtFiveOffsets called directly)")
    a = ap.parse_args()
    if "game" in sys.modules:
        raise RuntimeError("game.py imported before scenario_refs (U-1078)")
    import scenario_refs as R
    root = GR.CORPUS / a.label
    if root.exists():
        raise SystemExit(f"{root} exists")
    (root / "snaps").mkdir(parents=True)
    c = GR.SnapCollector()
    js = JS.read_text() + NL + "rpc.exports.glrecSnap(true, true, true);" + NL
    if a.keep:
        js += f"rpc.exports.glrecKeep({a.keep});" + NL
    orig_setup = R.setup
    cps = []
    queued = []

    def fetch(rec, label):
        n0 = len(c.snaps)
        k = rec.glrec_send_injected()
        t0 = time.time()
        while len(c.snaps) < n0 + k and time.time() - t0 < 180:
            time.sleep(0.05)
        print(f"  [gl28] {label}: {k} injected passes fetched", flush=True)

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
                fetch(self._rec(), name)
                if a.inject_at and name == a.inject_at:
                    self._rec().glrec_inject(INJECT)
                    queued.append(name)
                st = self._rec().glrec_kept()
                cps.append({"checkpoint": name, "kept": st["kept"], "ords": st["ords"], "snaps_received": len(c.snaps)})
                print(f"  [gl27] {name}: kept {st['kept']} ords {st['ords']} received {len(c.snaps)}", flush=True)
                return super().take_dump(sc, name)

            def post(self, sc, **kw):
                if "actions" in kw and not kw.get("next"):
                    rec = self._rec()
                    st = rec.glrec_kept()
                    t0 = time.time()
                    while len(c.snaps) < st["kept"] and time.time() - t0 < 180:
                        time.sleep(0.05)
                    fetch(rec, "end")
                    c.info["injected"] = rec.glrec_injected()
                    c.info["kept"] = st
                    rec.glrec_dump()
                    t0 = time.time()
                    while not c.done and time.time() - t0 < 180:
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
    rows = []
    for i, s in enumerate(c.snaps):
        e = s["meta"].get("entry") or {}
        direct = e.get("name") == "renderTileAtFiveOffsets"
        if e.get("inj"):
            name = f"i{e['inj']:03d}_{queued[0] if queued else 'x'}_rtafo"
        else:
            kn = {**GR.KIND_NAME, 5: "lowerEdge"}.get(e.get("kind", 0), e.get("kind"))
            name = f"k{i:03d}_{kn}_o{e.get('ord', -1)}"
        n = GR.save_corpus_snap(root, name, s)
        d = root / "snaps" / name
        for f in ("vtx.bin", "nrm.bin"):   # a pass that drew nothing reads no array element: empty arrays
            if not (d / f).exists():
                (d / f).write_bytes(b"")
        with open(d / "meta.txt", "a") as f:
            f.write(f"entry_ord {e.get('ord', -1)}" + NL + f"entry_rord {e.get('rord', -1)}" + NL)
            f.write(f"entry_a3 {e.get('a3', 0)}" + NL + f"entry_direct {1 if direct else 0}" + NL)
            f.write(f"streamN {s['meta'].get('streamN', -1)}" + NL)
        rows.append((name, n, e.get("kind"), e.get("ord"), e.get("rord"), hex(e.get("a0", 0)), hex(e.get("a1", 0)), hex(e.get("a2", 0)),
                     hex(e.get("a3", 0)), int(direct), e.get("inj", 0)))
    lines = [l for i in sorted(c.chunks) for l in c.chunks[i]]
    (root / "stream.gl.txt").write_text(NL.join(lines) + NL)
    (root / "tex").mkdir(exist_ok=True)
    for h, b in c.tex.items():
        (root / "tex" / f"{h}.bin").write_bytes(b)
    with open(root / "index.tsv", "w") as f:
        f.write("step\tpass_lines\tentry_kind\tord\trord\ta0\ta1\ta2\ta3\tdirect\tinj" + NL)
        for r in rows:
            f.write("\t".join(str(x) for x in r) + NL)
    (root / "run.json").write_text(json.dumps({"scenario": a.scenario, "run": str(R.REFS / a.scenario / a.label), "rc": rc,
                                               "kept": c.info.get("kept"), "injected": c.info.get("injected"), "inject_at": a.inject_at,
                                               "checkpoints": cps, "textures": len(c.tex),
                                               "stream_lines": len(lines), "snaps": len(rows)}, indent=1))
    print(f"{len(rows)} snapshots, {len(lines)} stream lines, {len(c.tex)} textures -> {root} (scenario_refs rc {rc})")
    return 0 if rows and lines and rc == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
