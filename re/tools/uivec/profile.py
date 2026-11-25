"""uivec: the radial colour profile of a title bubble across its rim, art against shapes (P41).
Usage: py -3.12 re/tools/uivec/profile.py [name] [idle|hover]
Rows: distance from the rim's measured outer radius (negative inside), in four direction sectors (the mean colour of the
pixels whose centre falls in that half-pixel band).
"""
import copy
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(__file__))
import accept   # noqa: E402
import recipes  # noqa: E402
import sdf      # noqa: E402


def main():
    name = sys.argv[1] if len(sys.argv) > 1 else "sandbox"
    hover = len(sys.argv) > 2 and sys.argv[2] == "hover"
    b = accept.Bench()
    style = copy.deepcopy(recipes.STYLE)
    g = b.meas[name][accept.ST[hover]]
    cx, cy, r = g["c"]
    x, y = b.grid(name, "button", hover)
    v = b.render(name, "button", hover, style, 6)
    a = b.target(name, "button", hover)
    far = sdf.sd({"t": "poly", "rings": g["glyph"]}, x, y) > 1.5
    rr = np.hypot(x - cx, y - cy) - r
    ang = np.degrees(np.arctan2(y - cy, x - cx))
    sectors = {"top": (ang > -135) & (ang < -45), "right": (ang >= -45) & (ang <= 45), "bottom": (ang > 45) & (ang < 135),
               "left": (ang >= 135) | (ang <= -135)}
    print(f"{name} {accept.ST[hover]}: rim r={r} body={g.get('body')}")
    print(f"{'d':>6s} " + " ".join(f"{s + ' art':>16s} {s + ' vec':>16s}" for s in sectors))
    for d0 in np.arange(-5.0, 3.01, 0.5):
        band = (rr >= d0 - 0.25) & (rr < d0 + 0.25) & far
        cells = []
        for s, mk in sectors.items():
            mk = mk & band
            if mk.any():
                ca, cv = a[mk].mean(0), v[mk].mean(0)
                cells.append(f"{ca[0]:5.0f}{ca[1]:5.0f}{ca[2]:5.0f}  {cv[0]:5.0f}{cv[1]:5.0f}{cv[2]:5.0f} ")
            else:
                cells.append(" " * 33)
        print(f"{d0:6.1f} " + " ".join(cells))


if __name__ == "__main__":
    main()
