"""P41 round 7: how far the ART moves a button between states (whole pixels), independent of the scenes.

For each state, the shift s in [-2, 2]^2 that best maps the idle cell's edges onto the state cell's edges inside a
region (default: the cell minus a 2 px border): edges = luminance gradient magnitude, each image scaled by its own 90th
percentile, so paint changes (tan / yellow / dark) matter less than where the outlines are. Prints the best shift, its
score and the runner-up, so a near tie is visible.

Usage: py -3.12 fix7_artmove.py [element ...]
"""
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import common as C  # noqa: E402
import fit  # noqa: E402
import fix7_audit as A  # noqa: E402


def grad(a):
    L = fit.lum(a)
    g = np.zeros_like(L)
    g[:, 1:-1] = np.maximum(g[:, 1:-1], np.abs(L[:, 2:] - L[:, :-2]) / 2)
    g[1:-1, :] = np.maximum(g[1:-1, :], np.abs(L[2:, :] - L[:-2, :]) / 2)
    return g / max(np.percentile(g, 90), 1e-6)


def art_moves(el, ref=None, region=None, reach=2):
    """-> {state: ((dx, dy), score, runner-up (dx, dy), its score)} relative to ref (default idle / first state).
    region: (x0, y0, x1, y1) in cell coordinates to compare (default the cell minus 2 px)."""
    cells = {st: (key, r) for st, key, r in C.CELLS[el]}
    ref = ref or ("idle" if "idle" in cells else list(cells)[0])
    k0, (x0, y0, w, h) = cells[ref]
    gr = grad(C.sheet(k0)[0][y0:y0 + h, x0:x0 + w].astype(float))
    rx0, ry0, rx1, ry1 = region or (2 + reach, 2 + reach, w - 2 - reach, h - 2 - reach)
    out = {}
    for st, (k, (x, y, ww, hh)) in cells.items():
        if st == ref:
            continue
        gs = grad(C.sheet(k)[0][y:y + hh, x:x + ww].astype(float))
        sc = []
        for dy in range(-reach, reach + 1):
            for dx in range(-reach, reach + 1):
                a = gr[ry0:ry1, rx0:rx1]
                b = gs[ry0 + dy:ry1 + dy, rx0 + dx:rx1 + dx]
                sc.append((float(np.abs(a - b).mean()), (dx, dy)))
        sc.sort()
        out[st] = (sc[0][1], sc[0][0], sc[1][1], sc[1][0])
    return out


if __name__ == "__main__":
    for el in sys.argv[1:] or A.MULTI:
        fams = {"employee-pedestal": ["idle", "idle 2"]}.get(el, [None])
        for ref in fams:
            res = art_moves(el, ref)
            if el == "employee-pedestal":
                res = {s: v for s, v in res.items() if (s.endswith(" 2")) == (ref == "idle 2")}
            print(f"{el:24s} " + "; ".join(f"{st}: {m} ({s:.3f}; next {m2} {s2:.3f})" for st, (m, s, m2, s2) in
                                            res.items()))
