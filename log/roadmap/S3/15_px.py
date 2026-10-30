"""S3.15: run framegate's run_scenario on one scenario / checkpoint list and print every differing pixel in the HUD top-bar
regions (hud_title, hud_meters, hud_message, toolbar) with the port's and the reference's RGB555 values.

    cd port/build/S3_15 && SG_PORTSIM_DLL=... ~/bin/py -3.12 ../../../log/roadmap/S3/15_px.py <scenario> <chk,chk> [--negctl n=m]
           [--save <dir>]   (port / ref .npy saved for later analysis)
"""
import argparse
import pathlib
import sys

import numpy as np

sys.path.insert(0, str(pathlib.Path.cwd()))
from port.test import framegate as FG  # noqa: E402
from port.test import gfx as GX  # noqa: E402

REG = dict(FG.INGAME_REGIONS)
TOP = ("hud_title", "hud_meters", "hud_message", "toolbar")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("scenario")
    ap.add_argument("chks")
    ap.add_argument("--negctl")
    ap.add_argument("--save", type=pathlib.Path)
    ap.add_argument("--regions", default=",".join(TOP))
    a = ap.parse_args()
    neg = None
    if a.negctl:
        n, m = a.negctl.split("=")
        neg = (n, int(m, 0))
    chks = a.chks.split(",")
    caught = {}
    orig = FG.region_numbers

    def keep(port, ref, kind):
        caught.setdefault("imgs", []).append((port.copy(), ref.copy()))
        return orig(port, ref, kind)

    FG.region_numbers = keep
    rows = FG.run_scenario(a.scenario, chks, negctl=neg, probe=False)
    regs = a.regions.split(",")
    for i, r in enumerate(rows):
        print(f"== {r['scenario']} {r['checkpoint']} {r['status']} {r.get('regions', r.get('detail'))}")
        if i >= len(caught.get("imgs", [])):
            continue
        port, ref = caught["imgs"][i]
        if a.save:
            a.save.mkdir(parents=True, exist_ok=True)
            np.save(a.save / f"{r['scenario']}.{r['checkpoint']}.port.npy", port)
            np.save(a.save / f"{r['scenario']}.{r['checkpoint']}.ref.npy", ref)
        p5, f5 = port.astype(int) >> 3, ref.astype(int) >> 3
        for n in regs:
            x0, y0, w, h = REG[n]
            sub = (np.abs(p5[y0:y0 + h, x0:x0 + w] - f5[y0:y0 + h, x0:x0 + w]).max(axis=2) > 0)
            ys, xs = np.nonzero(sub)
            print(f"  {n}: {len(xs)} px")
            for y, x in zip(ys + y0, xs + x0):
                print(f"    ({x:3d},{y:3d}) 0x{x:03x},0x{y:03x} port {tuple(p5[y, x])} ref {tuple(f5[y, x])}")


if __name__ == "__main__":
    main()
