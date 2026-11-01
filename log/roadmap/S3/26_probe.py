"""S3.26: one port boot of a pinned scenario up to the frame of a checkpoint, then the GameState words at given VAs.

    ~/bin/py -3.12 log/roadmap/S3/26_probe.py <scenario> <checkpoint> [--extra k] [--gfx] [--dump <file>] <va[:n]> ...

Run from the S3_26 tree (cwd port/build/S3_26, SG_PORTSIM_DLL set). The boot stops after the frame records up to the checkpoint's
clock ordinal plus `extra` (framegate's max_records rule; extra 0 = the run ends at the next frame head after the checkpoint's
frame). --gfx attaches the graphics backend (no LiveTerrain), so drops with a body run as with framegate. --dump writes the whole
GameState. Heavy: run it under w8_lock.py."""
import argparse
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path.cwd()))
from port.test import boot as B  # noqa: E402
from port.test import framegate as FG  # noqa: E402
from port.test import lib as L  # noqa: E402
from port.test import trace_format as TF  # noqa: E402

DATA = 0x4C1000


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("scenario")
    ap.add_argument("checkpoint")
    ap.add_argument("vas", nargs="*")
    ap.add_argument("--extra", type=int, default=0)
    ap.add_argument("--gfx", action="store_true")
    ap.add_argument("--dump")
    a = ap.parse_args()
    import json
    tr = TF.read(B.TRACES / f"{a.scenario}.trace")
    tgt = {}
    k = 0
    for ordinal, _ra, _slot, kind, value in TF.clock_calls(tr):
        if kind == 1:
            tgt[k] = ordinal
            k += 1
    summ = json.loads((FG.GX.REFS / a.scenario / "det_b" / "summary.json").read_text())
    o = tgt[summ["checkpoints"][a.checkpoint]["status"]["tgt"]]
    frames_pos = [r.f["pos"] for r in tr.records if r.type == TF.FRAME]
    n = sum(1 for p in frames_pos if p <= o) + a.extra
    del tr
    lib = L.load()
    out = {}

    def before(st):
        if a.gfx:
            st.call("gfx.attach", [])

    def after(st, _res):
        for spec in a.vas:
            va, _, cnt = spec.partition(":")
            va = int(va, 0)
            cnt = int(cnt or "4", 0)
            out[spec] = st.read(va - DATA, cnt)
        if a.dump:
            pathlib.Path(a.dump).write_bytes(st.image())
        if a.gfx:
            st.call("gfx.detach", [])

    res = B.run(a.scenario, lib, max_records=n, before=before, after=after, to_end=False)
    print(f"{a.scenario} {a.checkpoint}: ordinal {o}, frame records {n}, boot {res.status} {res.records}/{res.expected}")
    for spec, b in out.items():
        print(spec, b.hex(" "))


if __name__ == "__main__":
    main()
