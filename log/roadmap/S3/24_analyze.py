"""S3.24: list the pixels where the Python model (a rule of 24_probe.py) differs from the real GL, with their geometry.

    ~/bin/py -3.12 log/roadmap/S3/24_analyze.py <npz> [--rule R] [--max N]
"""
import argparse
import importlib.util
import pathlib

import numpy as np

HERE = pathlib.Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("p24", HERE / "24_probe.py")
P = importlib.util.module_from_spec(spec)
spec.loader.exec_module(P)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("npz", nargs="+")
    ap.add_argument("--rule", default="s38")
    ap.add_argument("--max", type=int, default=200)
    a = ap.parse_args()
    recs = []
    for p in a.npz:
        recs += P.load(p)
    rows = []
    for i, r in enumerate(recs):
        cov, g = P.coverage(r, a.rule)
        m = P.blend(r, cov)
        d = P.diff5(m, r["real"])
        if not (d > 0).any():
            continue
        x0, y0 = r["box"][:2]
        ang = np.degrees(np.arctan2(g["by"] - g["ay"], g["bx"] - g["ax"]))
        for yy, xx in zip(*np.nonzero(d)):
            drawn = r["real"][yy, xx] != r["pre"][yy, xx]
            rows.append((i, r["width"], r["alpha"], round(float(ang), 1), round(g["L"], 2), xx + x0, yy + y0, round(float(g["t"][yy, xx]), 3),
                         round(float(g["n"][yy, xx]), 3), round(float(cov[yy, xx]), 4), int(d[yy, xx]), int(drawn),
                         int(m[yy, xx] != r["pre"][yy, xx]), hex(int(r["pre"][yy, xx])), hex(int(m[yy, xx])), hex(int(r["real"][yy, xx]))))
    print("rec w a ang L x y t n cov d real_drawn model_drawn pre model real")
    for row in rows[:a.max]:
        print(*row)
    rows = np.array([r[:13] for r in rows], dtype=float)
    if len(rows):
        print("total", len(rows), "real-not-drawn", int(((rows[:, 11] == 0)).sum()), "model-not-drawn", int((rows[:, 12] == 0).sum()))


if __name__ == "__main__":
    main()
