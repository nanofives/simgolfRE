"""Straight runs of a sheet's silhouette (alpha > 0.5), to read a slab's vertex list by eye.
For every row: the left-most and right-most covered x (and inner gaps); printed as runs "y0-y1: x" where it stays put.
For every column: the top-most and bottom-most covered y, as runs "x0-x1: y".
Usage: py -3.12 runs.py <sheet.pcx> [y0 y1]"""
import sys

import numpy as np

import common as C


def runs(vals, keys, minlen=1):
    out = []
    s = None
    for k, v in zip(keys, vals):
        if s is None:
            s = [k, k, v]
        elif v == s[2] and k == s[1] + 1:
            s[1] = k
        else:
            out.append(tuple(s))
            s = [k, k, v]
    if s:
        out.append(tuple(s))
    return [r for r in out if r[1] - r[0] + 1 >= minlen]


def main():
    sh = sys.argv[1]
    _, a, _ = C.art(sh)
    m = a > 0.5
    y0 = int(sys.argv[2]) if len(sys.argv) > 2 else 0
    y1 = int(sys.argv[3]) if len(sys.argv) > 3 else 600
    ys = [y for y in range(y0, y1) if m[y].any()]
    L = [int(np.nonzero(m[y])[0][0]) for y in ys]
    R = [int(np.nonzero(m[y])[0][-1]) + 1 for y in ys]
    print("LEFT  " + "  ".join(f"{a_}-{b_}:{v}" for a_, b_, v in runs(L, ys)))
    print("RIGHT " + "  ".join(f"{a_}-{b_}:{v}" for a_, b_, v in runs(R, ys)))
    sub = m[y0:y1]
    xs = [x for x in range(800) if sub[:, x].any()]
    T = [int(np.nonzero(sub[:, x])[0][0]) + y0 for x in xs]
    B = [int(np.nonzero(sub[:, x])[0][-1]) + 1 + y0 for x in xs]
    print("TOP   " + "  ".join(f"{a_}-{b_}:{v}" for a_, b_, v in runs(T, xs, 2)))
    print("BOT   " + "  ".join(f"{a_}-{b_}:{v}" for a_, b_, v in runs(B, xs, 2)))


if __name__ == "__main__":
    main()
