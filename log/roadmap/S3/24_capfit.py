"""S3.24: the end cap in a canonical frame (major and minor coordinates both increasing from a to b, origin at b).

For every visible-by-model pixel past the side-edge corner region at the b end (columns u in [-4, 4] around b), prints the
pixel's canonical (u, v), its margin to the exact cap line (t - L - 0.5 <= 0 inside) and to the side edge, and whether the real GL
drew it; then a summary of the not-drawn ones by margin.

    ~/bin/py -3.12 log/roadmap/S3/24_capfit.py <npz>...
"""
import collections
import importlib.util
import pathlib
import sys

import numpy as np

HERE = pathlib.Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("p24", HERE / "24_probe.py")
P = importlib.util.module_from_spec(spec)
spec.loader.exec_module(P)


def main():
    rows = []
    for p in sys.argv[1:]:
        for i, r in enumerate(P.load(p)):
            if r["width"] < 2:
                continue
            cov, g = P.coverage(r, "span_dir")
            if g is None:
                continue
            m = P.blend(r, cov)
            vis = m != r["pre"]
            rd = r["real"] != r["pre"]
            dx, dy = g["bx"] - g["ax"], g["by"] - g["ay"]
            xmaj = abs(dx) >= abs(dy)
            sM = np.sign(dx if xmaj else dy) or 1.0
            sm = np.sign(dy if xmaj else dx) or 1.0
            X, Y = g["X"] + 0.5, g["Y"] + 0.5
            Mc, mc = (X, Y) if xmaj else (Y, X)
            bM, bm = (g["bx"], g["by"]) if xmaj else (g["by"], g["bx"])
            u = (Mc - bM) * sM
            v = (mc - bm) * sm
            tl = g["t"] - g["L"]
            sel = vis & (np.abs(u) <= 4.5) & (tl > -2)
            slope = abs((dy / dx) if xmaj else (dx / dy))
            for yy, xx in zip(*np.nonzero(sel)):
                rows.append((p[-12:], i, r["width"], round(slope, 3), float(u[yy, xx]), float(v[yy, xx]), float(tl[yy, xx]),
                             float(g["half"] + 0.5 - abs(g["n"][yy, xx])), int(rd[yy, xx]), float(cov[yy, xx])))
    nd = [x for x in rows if not x[8]]
    print(len(rows), "end pixels visible by the model;", len(nd), "not drawn")
    c = collections.Counter((x[4], x[5] > 0) for x in nd)
    print("not drawn by (u, v>0):", sorted(c.items()))
    # for the not-drawn (u, v) cells, how many drawn pixels share that (u, v, slope class)?
    for x in nd[:60]:
        same = [y for y in rows if y[8] and abs(y[4] - x[4]) < 1e-6 and abs(y[5] - x[5]) < 0.05 and y[2] == x[2]]
        print(f"ND w {x[2]:2d} slope {x[3]:.3f} u {x[4]:+.2f} v {x[5]:+.3f} t-L {x[6]:+.3f} side {x[7]:+.3f} cov {x[9]:.3f}"
              f"   drawn twins (same u, v, w): {len(same)}" + (f" e.g. slope {same[0][3]} t-L {same[0][6]:+.3f}" if same else ""))


if __name__ == "__main__":
    main()
