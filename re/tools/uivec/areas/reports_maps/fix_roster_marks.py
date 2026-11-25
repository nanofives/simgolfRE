"""P41 fix round: deliver roster-icon-camera (redo) and roster-icon-heart (fix "weird heart")."""
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import common as C          # noqa: E402
import deliver as D         # noqa: E402
import deliver2 as D2       # noqa: E402
import roster_marks as RM   # noqa: E402
import roster_marks2 as RM2  # noqa: E402

FL = np.array([70., 110, 90])


def cell_art(x, y, w, h):
    rgb, m = C.sheet(RM.SB)
    a = rgb[y:y + h, x:x + w].copy()
    a[~m[y:y + h, x:x + w]] = FL
    b = np.empty_like(a)
    b[:] = FL
    return a, b


def states(x0, layers_fn):
    out = []
    for label, y, tan in (("row 1", 1, False), ("row 2", 20, True)):
        a, b = cell_art(x0 - 1, y - 1, 21, 20)
        out.append((label, a, b, [RM.cell_item(x0, y, tan), {"name": "mark", "box": [x0, y, x0 + 19, y + 18],
                                                              "layers": layers_fn(0, y - 1)}], (x0 - 1, y - 1)))
    return out


def legend_tile(off, layers, x, w=26):
    rgb, _ = C.sheet("memberRoster.pcx")
    y = 528
    art = rgb[y:y + 24, x:x + w]
    base = np.empty_like(art)
    base[:] = [148, 150, 198]
    return D.tile(art, base, [{"name": "mark", "box": [x, y, x + w, y + 24], "layers": layers}], (x, y), 15)


which = sys.argv[1:] or ["heart", "camera"]
if "heart" in which:
    D2.deliver("roster-icon-heart", states(146, RM2.heart),
               "a navy heart read row by row, symmetric about x 155.5: two rounded lobe tops (y 3.0), a sharp V notch "
               "to y 5.0 on the axis, STRAIGHT vertical outer sides (x 147.4 / 163.6, y 4.6..8.3), straight 45-degree sides "
               "down to a narrow drawn-out tip (to y 17.9); one mirrored polygon, two rounds of corner cutting with the notch and tip kept sharp; flat "
               "navy (the art has no highlight). Rows: cream cell, tan cell; then the legend copy at 15x",
               "the old heart was two circles over a triangle: round bulging lobes, a waist where they met the triangle and a short "
               "blunt point. Now the lobes are the art's low rounded tops over straight vertical outer sides, the sides run straight to the art's long narrow tip",
               z=15, extra_tiles=[legend_tile(RM.HEART_OFF, RM2.heart(*RM.HEART_OFF), 624)])
if "camera" in which:
    D2.deliver("roster-icon-camera", states(126, RM2.camera),
               "a navy camera from its parts: body (x 128.4..142.9, y 6..17, r 0.7), prism hump (top x 133.9..137.6 at y 1.9, foot "
               "x 131.4..139.4, its left wall more slanted), a small dark knob at the top right; the lens ring is a HOLE in the body "
               "(r 3.45: the art shows the cell's cream / tan and the legend's lavender through it) round a blue lens (r 1.75, lighter "
               "top left). Light from the top left as the art's pixels: a 1.2 px light-blue rim along the body's top and left edges, "
               "the hump shaded from light blue at its top left to navy at its foot on the right. Rows: cream cell, tan cell; then "
               "the legend copy at 15x",
               "redone from a new reading: the ring is a hole showing what lies behind (was a white ring), the lighting is baked "
               "(lit top-left rim, shaded hump, lit lens; was flat navy), the hump's left wall slants as in the art, the knob is a "
               "low dark bump at the body's top right, proportions re-measured",
               unsure="the knob is under 2 px in the art",
               z=15, extra_tiles=[legend_tile(RM.CAM_OFF, RM2.camera(*RM.CAM_OFF), 500)])
