"""Glyph coverage of an info-button cell as digits (0-9, '.' = 0): darkness between the body's local tan and navy."""
import sys
import numpy as np
import common as c
import infobuttons as ib


def cov(r, col=0):
    box = ib.cell(r, col)
    disc = ib.measure_disc(r, col)
    a = c.art(ib.SHEET, box)
    jj, ii = np.mgrid[0:ib.H, 0:ib.W]
    inside = np.hypot(ii + .5 + box[0] - disc[0], jj + .5 + box[1] - disc[1]) < disc[2] - 2.3
    cv, lo, hi = c.fit.dark_coverage(a, inside, navy_l=50.0)
    return cv, box


if __name__ == "__main__":
    r = int(sys.argv[1]); col = int(sys.argv[2]) if len(sys.argv) > 2 else 0
    cv, box = cov(r, col)
    print("    " + "".join(str(x % 10) for x in range(ib.W - 10)))
    for j in range(2, 35):
        print(f"{j:3d} " + "".join("." if v < 0.05 else str(min(int(v * 10), 9)) for v in cv[j, :ib.W - 10]))
