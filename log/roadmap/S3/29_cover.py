"""S3.29 (U-1309): coverage of Windows' generic GL as an incremental edge walk, fitted on glprobe's `tie` scene (flat triangles whose
vertices sit on pixel centres, so edges pass exactly through centres; real frame port/corpus/gl/probe/tie/out.bin) and on the S3.29
probes. The exact-edge rule (GlRaster's, S3.1b) leaves 48 coverage differences on `tie`.

Model: rows from the top vertex (largest y) down, row centres yc = iy + 0.5 with ybot <= yc < ytop (rows owned as GlRaster does);
each edge's x at the row is walked: x(first row) = x_top + dxdy * (yc0 - y_top) held to 2^-F (`init`: floor / round / trunc / exact),
then x += dxdy held to 2^-F per row (`step`: floor / round / trunc / exact, the error accumulating along the edge); columns with
xl < x + 0.5 <= xr (`incl` variants for the tie on each side).

    cd <tree> && ~/bin/py -3.12 ../../../log/roadmap/S3/29_cover.py [scene]
"""
import importlib
import itertools
import math
import pathlib
import sys

import numpy as np

sys.path.insert(0, str(pathlib.Path.cwd()))
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
TF = importlib.import_module("29_texfit")


def walk_rows(t, F, init, step, rows_rule="lo_incl", incl=("open", "closed"), walk="top"):
    pts = [v["p"] for v in t]
    o = sorted(range(3), key=lambda i: (pts[i][1], i))
    bot, mid, top = [pts[i] for i in o]
    if top[1] == bot[1]:
        return {}
    Q = 2.0 ** F

    def hold(v, mode):
        if mode == "exact":
            return v
        f = {"floor": math.floor, "round": lambda z: math.floor(z + 0.5), "trunc": math.trunc, "ceil": math.ceil}[mode]
        return f(v * Q) / Q

    def edge_xs(a, b, ys):
        # x of edge a (upper) -> b (lower) at the row centres ys (descending), walked from the first
        if a[1] == b[1]:
            return None
        dxdy = (b[0] - a[0]) / (b[1] - a[1])      # per unit of y
        out = []
        x = None
        for k, yc in enumerate(ys):
            if x is None:
                x = hold(a[0] + dxdy * (yc - a[1]), init)
                stp = hold(-dxdy, step) if step != "exact" else -dxdy
            else:
                x = x + stp if step != "exact" else a[0] + dxdy * (yc - a[1])
            out.append(x)
        return out
    # rows: centres in [ybot, ytop)
    iy_hi = math.ceil(top[1] - 0.5) - 1
    iy_lo = math.ceil(bot[1] - 0.5)
    ys_all = [iy + 0.5 for iy in range(iy_hi, iy_lo - 1, -1)]
    # long edge top -> bot; short edges top -> mid (rows above mid) and mid -> bot
    longx = edge_xs(top, bot, ys_all)
    up = [yc for yc in ys_all if yc >= mid[1]]
    lo = [yc for yc in ys_all if yc < mid[1]]
    s1 = edge_xs(top, mid, up) if up else []
    s2 = edge_xs(mid, bot, lo) if lo else []
    short = (s1 or [None] * len(up)) + (s2 or [None] * len(lo))
    out = {}
    for k, yc in enumerate(ys_all):
        xa, xb = longx[k], short[k]
        if xb is None:
            continue
        xl, xr = min(xa, xb), max(xa, xb)
        lo_x = math.floor(xl - 0.5) + 1 if incl[0] == "open" else math.ceil(xl - 0.5)
        hi_x = math.floor(xr - 0.5) if incl[1] == "closed" else math.ceil(xr - 0.5) - 1
        if hi_x >= lo_x:
            out[int(yc - 0.5)] = (lo_x, hi_x)
    return out


def score(scene, F, init, step, incl):
    tris, tex, real = TF.load(scene)
    drawn = real != 0
    model = np.zeros_like(drawn)
    for t in tris:
        for y, (a, b) in walk_rows(t, F, init, step, incl=incl).items():
            if 0 <= y < 600:
                model[y, max(0, a):min(799, b) + 1] = True
    return int((model != drawn).sum())


def main():
    scene = sys.argv[1] if len(sys.argv) > 1 else "tie"
    print("exact edges (GlRaster's rule):", score(scene, 16, "exact", "exact", ("open", "closed")))
    for incl in (("open", "closed"), ("closed", "open"), ("open", "open"), ("closed", "closed")):
        print("incl", incl, "exact:", score(scene, 16, "exact", "exact", incl))
    for F in (4, 8, 12, 16, 20, 24, 31):
        for init, step in itertools.product(("round", "floor", "trunc", "exact"), ("round", "floor", "trunc")):
            print(f"F {F:2d} init {init:5s} step {step:5s}:", score(scene, F, init, step, ("open", "closed")), flush=True)


if __name__ == "__main__":
    main()
