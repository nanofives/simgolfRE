"""uivec: a title glyph on its pixel grid, as text, for reading what it is made of (P41).
Usage: py -3.12 re/tools/uivec/glyphdump.py <name> [idle|hover]
One character per frame pixel: how much of it the dark glyph covers ('#' > 0.85, '+' > 0.6, ':' > 0.35, '.' > 0.12).
The column header gives x (tens and units), the row label y; pixel (x, y) covers [x, x+1) x [y, y+1).
"""
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(__file__))
import fit      # noqa: E402
import title    # noqa: E402


def coverage(name, state="idle"):
    """(coverage array, x0, y0) of the glyph's dark coverage over the button's core."""
    base_idx, base_pal, un_idx, un_pal, mo_idx, mo_pal = title.load_art()
    art = title.composite(base_idx, base_pal, *((un_idx, un_pal) if state == "idle" else (mo_idx, mo_pal))).astype(float)
    m = title.measure(verbose=False)[name]
    cx, cy, r = m[state]["c"]
    k = 0.76 if m["kind"] == "disc" else 0.90
    x0, y0, x1, y1 = int(cx - r), int(cy - r), int(np.ceil(cx + r)), int(np.ceil(cy + r))
    jj, ii = np.mgrid[y0:y1, x0:x1]
    inside = np.hypot(ii + 0.5 - cx, jj + 0.5 - cy) < r * k
    cov, lo, hi = fit.dark_coverage(art[y0:y1, x0:x1], inside)
    return cov, x0, y0


def main():
    name = sys.argv[1]
    state = sys.argv[2] if len(sys.argv) > 2 else "idle"
    cov, x0, y0 = coverage(name, state)
    rows = np.nonzero(cov.max(axis=1) > 0.12)[0]
    cols = np.nonzero(cov.max(axis=0) > 0.12)[0]
    j0, j1, i0, i1 = rows[0] - 1, rows[-1] + 2, cols[0] - 1, cols[-1] + 2
    print(f"{name} {state}: x {x0 + i0}..{x0 + i1}, y {y0 + j0}..{y0 + j1}")
    print("     " + "".join(str((x0 + i) // 10 % 10) for i in range(i0, i1)))
    print("     " + "".join(str((x0 + i) % 10) for i in range(i0, i1)))
    for j in range(j0, j1):
        line = "".join("#" if c > 0.85 else "+" if c > 0.6 else ":" if c > 0.35 else "." if c > 0.12 else " "
                       for c in cov[j, i0:i1])
        print(f"{y0 + j:4d} {line}")


if __name__ == "__main__":
    main()
