"""Sub-pixel outer edge of a sheet's slab from its alpha: for each column the top / bottom edge, for each row the left / right
edge (a run of alpha across the edge: position = where the cumulative alpha crosses, read as the sum of partial coverage).
Usage: py -3.12 edges.py <sheet> top|bottom|left|right <from> <to> [step] [lo hi]   (lo..hi limits the scan along the ray)"""
import sys
import numpy as np
import common as C


def scan(a, side, k, lo=None, hi=None):
    v = a[:, k] if side in ("top", "bottom") else a[k, :]
    n = len(v)
    lo = 0 if lo is None else lo
    hi = n if hi is None else hi
    seg = v[lo:hi]
    if side in ("bottom", "right"):
        seg = seg[::-1]
    on = np.nonzero(seg > 0.5)[0]
    if len(on) == 0:
        return None
    f = on[0]
    s0 = max(f - 3, 0)
    pos = f - seg[s0:f].sum() + (1 - seg[f]) if False else f + (1 - seg[f]) - seg[s0:f].sum()
    # coverage-based: edge = f + 1 - (sum of alpha over the pixels before and including f within the 4-px window)
    pos = (f + 1) - seg[s0:f + 1].sum() + (s0 - s0)
    pos = s0 + (f + 1 - s0) - seg[s0:f + 1].sum()
    return lo + pos if side in ("top", "left") else hi - pos


if __name__ == "__main__":
    sh, side, a0, a1 = sys.argv[1], sys.argv[2], int(sys.argv[3]), int(sys.argv[4])
    st = int(sys.argv[5]) if len(sys.argv) > 5 else 1
    lo = int(sys.argv[6]) if len(sys.argv) > 6 else None
    hi = int(sys.argv[7]) if len(sys.argv) > 7 else None
    _, a, _ = C.art(sh)
    out = []
    for k in range(a0, a1, st):
        p = scan(a, side, k, lo, hi)
        out.append(f"{k}:{'-' if p is None else round(p, 2)}")
    print(" ".join(out))
