"""P41 hud_panels fix round (2026-11-24): the Employee panel's pedestal (six cells: idle / selected / disabled, two rows).

Re-read from the luminance dump of E 0,0 (63 x 43), cell-relative:
- Cap: an ellipse with a one-pixel dark outline (16-43), x 6-56, rows 5-36. Its inside is a CONVEX lit surface: a bright
  band along the upper-left inside edge (191-200), flat lilac in the middle (152), a darker band along the lower right
  (115-135) and a thin reflected light just inside the lower-right outline (123-141). Painted as one sdf bevel (colour
  by depth below the edge and by which way the edge faces) fitted by least squares on the cell's own pixels, so the
  light is baked, not a flat fill (round 1 was flat with a dark strip at the bottom).
- Under the cap on the cylinder: a dark band 2-3 px (82-105) where the cap shades the side.
- Cylinder side x 6-56.6 down to the cell foot: lit from the front left (brightest column x 18-27, 200 at the bottom),
  darker to the right (152); a horizontal paint through the bottom row's colours (the rows between the cap's shade and
  the foot are within a few levels of it).
- To the right of the whole pedestal a soft shadow on the panel (59 -> 104 over x 57-62): the pedestal's silhouette moved
  right and blurred (round 1 drew a hard dark rectangle).
"""
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import common as C  # noqa: E402
import kit as K  # noqa: E402
import bubble as B  # noqa: E402
import el_bubbles as EB  # noqa: E402
import fit  # noqa: E402
import sdf  # noqa: E402
from kit import U, rect, circle, ellipse, poly, sub, inter, outline  # noqa: E402,F401

STOPS = [0.0, 0.05, 0.12, 0.22, 0.38, 0.6, 1.0]


def cap_ellipse(key, rct):
    rgb, m = C.sheet(key)
    x0, y0, w, h = rct
    a = rgb[y0:y0 + h, x0:x0 + w]
    dk = (fit.lum(a) < 70) & m[y0:y0 + h, x0:x0 + w]
    pts = fit.outermost(dk, (31.5, 20.5), rays=160)
    pts = pts[(pts[:, 0] < 56.5) & (pts[:, 1] < 37.5)]
    cx, cy, rx, ry, _ = fit.fit_ellipse(pts, rot=0.0)
    return cx + x0, cy + y0, rx + 0.4, ry + 0.4


def fit_cap(key, E):
    """Bevel coefficients for the cap's inside (inner ellipse) from this cell's pixels."""
    cx, cy, rx, ry = E
    S = ellipse(cx, cy, rx - 1.0, ry - 1.0)
    rgb, m = C.sheet(key)
    x0, y0, x1, y1 = int(cx - rx), int(cy - ry), int(cx + rx) + 1, int(cy + ry) + 1
    jj, ii = np.mgrid[y0:y1, x0:x1]
    x, y = ii + 0.5, jj + 0.5
    d = sdf.sd(S, x, y)
    sel = d < -0.5
    nx, ny = sdf.normal(S, x, y, d)
    Lsc = ry - 1.0
    rows = [((-d / Lsc)[sel], nx[sel], ny[sel], rgb[y0:y1, x0:x1][sel])]
    coef = _solve(rows)
    coef[-1] = [coef[-1][0], [0, 0, 0], [0, 0, 0], [0, 0, 0], [0, 0, 0]]
    return S, {"bevel": {"L": Lsc, "stops": STOPS, "coef": coef}}


def _solve(rows):
    import accept
    coef, _ = accept.solve_bevel(rows, STOPS)
    return coef


def build(el):
    scenes = {}
    for st, key, rct in C.CELLS[el]:
        x0, y0, w, h = rct
        rgb, m = C.sheet(key)
        cx, cy, rx, ry = E = cap_ellipse(key, rct)
        lav = K.sample(key, rect(x0 + 1, y0 + 2, x0 + 3, y0 + 6), depth=0.0)
        above = K.sample(key, rect(int(cx) - 10, y0, int(cx) - 9, y0 + 2), depth=0.0)
        ys = y0 if np.abs(np.subtract(above, lav)).max() > 12 else cy
        xl, xr = cx - rx + 0.4, cx + rx - 0.4
        side = rect(xl, ys, xr, y0 + h)
        silh = U(ellipse(cx, cy, rx, ry), side)
        L = [K.lay(rect(x0, y0, x0 + w, y0 + h), lav)]
        # soft shadow to the right on the panel
        shc = [round(float(v), 1) for v in rgb[y0 + h - 2, min(int(xr) + 1, x0 + w - 1)]]
        L.append(K.lay(K.shift(silh, 2.6, 0.4), shc, soft=2.6, alpha=0.95, clip=rect(xr - 0.5, y0, x0 + w, y0 + h)))
        # cylinder side: horizontal light along the bottom row, darkening upward toward the cap (vertical overlay)
        bottom = [[float(t), [round(float(v), 1) for v in rgb[y0 + h - 2, int(xl + 0.5 + t * (xr - xl - 1))]]] for t in np.linspace(0, 1, 9)]
        L.append(K.lay(side, {"linear": {"a": [xl, 0], "b": [xr, 0], "stops": bottom}}))
        # the cap's shade on the side, right under its lower edge
        capsh = K.sample(key, rect(cx - 6, cy + ry + 0.5, cx + 6, cy + ry + 1.5), depth=0.0)
        L.append(K.lay(ellipse(cx, cy + 1.8, rx - 0.6, ry), capsh, soft=1.3, clip=side))
        # outline and the lit cap
        L.append(K.lay(ellipse(cx, cy, rx, ry), K.sample_dark(key, outline(ellipse(cx, cy, rx - 0.5, ry - 0.5), 1.0), q=0.25)))
        S, paint = fit_cap(key, E)
        L.append(K.lay(S, paint))
        scenes[st] = [K.item(L, box=[x0, y0, x0 + w, y0 + h])]
    return scenes


EB.BUILD["employee-pedestal"] = build
