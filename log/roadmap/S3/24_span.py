"""S3.24: the real GL's footprint per minor-axis column, per copy (no cross-copy intervals).

For each record of a white-on-black phase set: major axis = the larger of |dx|, |dy| of the snapped ends; for each pixel the model
draws, o = its centre's minor coordinate minus the line centre's minor coordinate at the pixel centre's major coordinate, and
`vis` = the model's value would be nonzero under that pixel's dither (31 * s * cov + d >= 1). A visible-by-model pixel that the
real GL leaves 0 is NOT DRAWN; a pixel the real GL changes is DRAWN. Prints, per (width, slope bucket), the largest |o| drawn and
the smallest |o| not drawn, away from the end caps (t in [1, L - 1]).

    ~/bin/py -3.12 log/roadmap/S3/24_span.py <npz>...
"""
import importlib.util
import pathlib
import sys

import numpy as np

HERE = pathlib.Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("p24", HERE / "24_probe.py")
P = importlib.util.module_from_spec(spec)
spec.loader.exec_module(P)


def main():
    recs = []
    for p in sys.argv[1:]:
        recs += P.load(p)
    agg = {}
    for r in recs:
        if r["alpha"] != 10 or r["colour"] != 0x7FFF or r["pre"].any():
            continue
        cov, g = P.coverage(r, "s38")
        if g is None:
            continue
        dx, dy = g["bx"] - g["ax"], g["by"] - g["ay"]
        xmaj = abs(dx) >= abs(dy)
        x0, y0 = r["box"][:2]
        X, Y = g["X"] + 0.5, g["Y"] + 0.5
        if xmaj:
            yc = g["ay"] + (X - g["ax"]) * dy / dx
            o = Y - yc
            slope = abs(dy / dx)
        else:
            xc = g["ax"] + (Y - g["ay"]) * dx / dy
            o = X - xc
            slope = abs(dx / dy)
        s = P.chan(0x7FFF)[0]
        d = (P.K[g["Y"].astype(int) & 3, g["X"].astype(int) & 3] + 0.5) / 16.0
        vis = (np.floor(31 * s * cov + d) >= 1)
        q = (r["real"].astype(int) >> 10) & 31
        drawn = r["real"] != 0
        mid = (g["t"] >= 1.0) & (g["t"] <= g["L"] - 1.0)
        nd = vis & ~drawn & mid
        dr = drawn & mid
        key = (r["width"], round(float(np.degrees(np.arctan(slope)))))
        a = agg.setdefault(key, [0.0, 99.0, 0, 0])
        if dr.any():
            a[0] = max(a[0], float(np.abs(o[dr]).max()))
        if nd.any():
            a[1] = min(a[1], float(np.abs(o[nd]).min()))
        a[2] += int(nd.sum())
        a[3] += int(dr.sum())
    print("width deg max|o|drawn min|o|notdrawn n_notdrawn n_drawn  (w/2+0.5)/cos")
    for (w, deg), (mx, mn, nnd, ndr) in sorted(agg.items()):
        lim = (w / 2 + 0.5) / np.cos(np.radians(deg))
        print(f"{w:3d} {deg:3d} {mx:7.3f} {mn:7.3f} {nnd:5d} {ndr:6d}   {lim:.3f}")


if __name__ == "__main__":
    main()
