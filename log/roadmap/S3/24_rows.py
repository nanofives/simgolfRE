"""S3.24: the 31 overlay-line census rows (log/roadmap/S3/24_rows.tsv, group 1 of 22_causes.tsv) under a line rule.

One framegate boot per scenario (host_gl = the terrain exact, as 22_rows.py; --raster for the default GlRaster terrain), the rows'
checkpoints only; the port's checkpoint frames are kept as log/roadmap/S3/24_data/frames_<tag>/<scenario>.npz (5-bit RGB) and
one line per row printed: px, beyond, pass (px == 0).

    cd port/build/S3_24 && SG_PORTSIM_DLL=<dll> ~/bin/py -3.12 ../../../log/roadmap/S3/24_rows.py --rule 1 --tag r1 [--raster]
        [--scenarios a,b] [--dump]          (--dump: SG_OVERLAY_DUMP=24_data/dump_<scenario>.bin per scenario)
"""
import argparse
import csv
import os
import pathlib
import sys

import numpy as np

sys.path.insert(0, str(pathlib.Path.cwd()))
from port.test import framegate as FG  # noqa: E402
from port.test import gfx as GX  # noqa: E402
from port.test import lib as L  # noqa: E402

ROOT = pathlib.Path(__file__).resolve().parents[3]
DATA = ROOT / "log" / "roadmap" / "S3" / "24_data"


def rgb5(a):
    a = np.asarray(a)
    return (a.astype(int) >> 3) if a.ndim == 3 else np.stack([(a >> 10) & 31, (a >> 5) & 31, a & 31], -1).astype(int)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--rule", type=int, default=-1, help="overlay.setRule for the run (-1: the build's default)")
    ap.add_argument("--tag", required=True)
    ap.add_argument("--raster", action="store_true")
    ap.add_argument("--scenarios")
    ap.add_argument("--dump", action="store_true")
    ap.add_argument("--negctl", type=int, default=0, help="overlay.rasterLine control mode for the run (10 = the overlay draws nothing)")
    ap.add_argument("--rows", type=pathlib.Path, default=ROOT / "log" / "roadmap" / "S3" / "24_rows.tsv")
    a = ap.parse_args()
    by = {}
    for r in csv.reader(open(a.rows, encoding="utf-8"), delimiter="\t"):
        by.setdefault(r[0], []).append(r[1])
    names = a.scenarios.split(",") if a.scenarios else sorted(by)
    out = DATA / f"frames_{a.tag}"
    out.mkdir(parents=True, exist_ok=True)
    lib = L.load()
    if a.rule >= 0:
        with lib.state() as st:
            prev = st.call("overlay.setRule", [a.rule]).eax
        print(f"rule {a.rule} (was {prev})", flush=True)
    else:
        print("rule: the build's default", flush=True)
    tot_pass = 0
    for s in names:
        if a.dump:
            d = DATA / f"dump_{s}.bin"
            if d.exists():
                d.unlink()
            os.environ["SG_OVERLAY_DUMP"] = str(d)
        order, orig = [], FG.region_numbers

        def rn(p_, r_, kd, _o=orig, _l=order):
            _l.append(p_.copy())
            return _o(p_, r_, kd)
        FG.region_numbers = rn
        try:
            rows = FG.run_scenario(s, by[s], lib=lib, probe=False, host_gl=not a.raster,
                                   negctl=("overlay.rasterLine", a.negctl) if a.negctl else None)
        finally:
            FG.region_numbers = orig
            os.environ.pop("SG_OVERLAY_DUMP", None)
        it, sink = iter(order), {}
        for r in rows:
            if r.get("regions") and r.get("status") in ("PASS", "FAIL", "UNSUPPORTED"):
                sink[r["checkpoint"]] = next(it)
        frames = {}
        for c in by[s]:
            img = sink.get(c)
            if img is None:
                print(f"{s}\t{c}\tno snapshot", flush=True)
                continue
            P_, R_ = rgb5(img), rgb5(GX.reference(s, c))
            frames[c] = P_.astype(np.uint8)
            dd = np.abs(P_ - R_).max(2)
            px, beyond = int((dd > 0).sum()), int((dd > 1).sum())
            tot_pass += px == 0
            print(f"{s}\t{c}\t{px}\t{beyond}\t{'PASS' if px == 0 else 'FAIL'}", flush=True)
        np.savez_compressed(out / f"{s}.npz", **frames)
    print("rows PASS", tot_pass, flush=True)


if __name__ == "__main__":
    main()
