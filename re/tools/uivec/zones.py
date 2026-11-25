"""uivec: where a button's remaining difference to the art sits (P41). Splits each button's scored region into zones and
prints the mean absolute difference and the share of the total error of each.
Usage: py -3.12 re/tools/uivec/zones.py
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
    b = accept.Bench()
    style = copy.deepcopy(recipes.STYLE)
    print(f"{'button':10s} {'state':6s} " + " ".join(f"{z:>20s}" for z in ("body", "glyph inside", "glyph edge", "rim", "outside rim")))
    tot = {}
    for name, m in b.meas.items():
        for hover in (False, True):
            g = m[accept.ST[hover]]
            cx, cy, r = g["c"]
            x, y = b.grid(name, "button", hover)
            v = b.render(name, "button", hover, style, 6)
            a = b.target(name, "button", hover)
            d = np.abs(v - a).mean(axis=2)
            reg = b.region(name, "button", hover)
            gd = sdf.sd({"t": "poly", "rings": g["glyph"]}, x, y)
            rr = np.hypot(x - cx, y - cy)
            if g.get("body"):
                bd = np.hypot(x - g["body"][0], y - g["body"][1]) - g["body"][2]
            else:
                bd = rr - r * 0.9
            zones = {"body": (bd < -1.0) & (gd > 1.2), "glyph inside": (gd < -1.2) & (bd < -1.0),
                     "glyph edge": (np.abs(gd) <= 1.2) & (bd < -1.0), "rim": (bd >= -1.0) & (rr <= r + 0.5),
                     "outside rim": rr > r + 0.5}
            total = d[reg].sum()
            cells = []
            for z, mk in zones.items():
                mk = mk & reg
                share = d[mk].sum() / max(total, 1e-9)
                cells.append(f"{d[mk].mean() if mk.any() else 0:6.1f} {100 * share:4.0f}% {int(mk.sum()):5d}px")
                t = tot.setdefault(z, [0.0, 0])
                t[0] += d[mk].sum()
                t[1] += int(mk.sum())
            print(f"{name:10s} {accept.ST[hover]:6s} " + " ".join(f"{c:>20s}" for c in cells))
    all_err = sum(t[0] for t in tot.values())
    print("\nall buttons: " + ", ".join(f"{z} {t[0] / max(t[1], 1):.1f} levels ({100 * t[0] / all_err:.0f}% of the error)"
                                       for z, t in tot.items()))


if __name__ == "__main__":
    main()
