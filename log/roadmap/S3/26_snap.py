"""S3.26: one framegate boot of a pinned scenario (host GL by default, so the terrain is exact) that also keeps, per checkpoint,
the port's GameState at the snapshot's instant (the development switch SG_GFX_SNAPSTATE=1 of gfx.cpp) and the port's frame.

    ~/bin/py -3.12 log/roadmap/S3/26_snap.py <scenario> [<checkpoint> ...] [--raster] [--negctl name=mode] [--out <dir>]

Run with cwd = the S3_26 tree and SG_PORTSIM_DLL set (26_run.sh does both). Writes <out>/<scenario>.<cp>.state.bin,
<out>/<scenario>.<cp>.port.png / .diff.png and prints framegate's rows (status, total / beyond px, regions). Heavy (a boot):
under w8_lock.py."""
import argparse
import os
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path.cwd()))
os.environ["SG_GFX_SNAPSTATE"] = "1"
from port.test import framegate as FG  # noqa: E402

ROOT = pathlib.Path(__file__).resolve().parents[3]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("scenario")
    ap.add_argument("checkpoints", nargs="*")
    ap.add_argument("--raster", action="store_true")
    ap.add_argument("--negctl")
    ap.add_argument("--out", default=str(ROOT / "log" / "roadmap" / "S3" / "26_img"))
    a = ap.parse_args()
    out = pathlib.Path(a.out)
    out.mkdir(parents=True, exist_ok=True)
    chks = FG.scenario_checkpoints(a.scenario)
    real_run = FG.B.run
    states = {}

    def run(scenario, lib, **kw):
        after = kw.get("after")

        def after2(st, res):
            for i in range(len(chks)):
                b = st.files_read(f"$gfx/state_{i}.bin")
                if b:
                    states[i] = b
            if after:
                after(st, res)
        kw["after"] = after2
        return real_run(scenario, lib, **kw)

    FG.B.run = run
    neg = None
    if a.negctl:
        n, m = a.negctl.split("=")
        neg = (n, int(m))
    rows = FG.run_scenario(a.scenario, None, negctl=neg, images=out, host_gl=False if a.raster else True)
    for r in rows:
        if a.checkpoints and r["checkpoint"] not in a.checkpoints:
            continue
        print(r["scenario"], r["checkpoint"], r.get("status"), r.get("total_px"), r.get("beyond_px"), r.get("regions", ""),
              r.get("detail", "")[:160])
    for i, b in states.items():
        (out / f"{a.scenario}.snap{i}.state.bin").write_bytes(b)
    print("states", sorted(states))


if __name__ == "__main__":
    main()
