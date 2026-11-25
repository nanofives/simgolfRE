"""A glyph's coverage as digits.  py -3.12 cov.py <id> <state> <dark|light> x0 y0 x1 y1 [lo hi]
Each pixel: 0..9 = how far its luminance sits from the ground (lo for a light glyph's ground ... hi) toward the glyph's;
lo / hi default to the 5th / 95th luminance percentile inside the box. '.' outside the cut."""
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hp  # noqa: E402
from hp import fit  # noqa: E402


def main():
    el, st, mode = sys.argv[1:4]
    bx0, by0, bx1, by1 = (int(v) for v in sys.argv[4:8])
    c = [c for c in hp.crops(el) if c[0] == st][0]
    _, src, x0, y0, rgb, mask = c
    sub = fit.lum(rgb[by0 - y0:by1 - y0, bx0 - x0:bx1 - x0].astype(float))
    m = mask[by0 - y0:by1 - y0, bx0 - x0:bx1 - x0]
    lo, hi = (float(sys.argv[8]), float(sys.argv[9])) if len(sys.argv) > 9 else (np.percentile(sub[m], 5), np.percentile(sub[m], 95))
    t = (sub - lo) / max(hi - lo, 1)
    if mode == "dark":
        t = 1 - t
    t = np.clip(t, 0, 0.999)
    print(f"lum lo {lo:.0f} hi {hi:.0f}")
    print("     " + "".join(str(x // 10 % 10) for x in range(bx0, bx1)))
    print("     " + "".join(str(x % 10) for x in range(bx0, bx1)))
    for j in range(sub.shape[0]):
        print(f"{by0 + j:4d} " + "".join(str(int(v * 10)) if mm else "." for v, mm in zip(t[j], m[j])))


if __name__ == "__main__":
    main()
