"""Elevation palette pedestals (elevation-point, -square, -area, -smooth): a cylinder standing against the palette's back wall,
its elliptical top coloured by state (teal idle and hover, green selected, blue disabled), holding a rendered picture.

Read on the cuts (2026-11-24): the top's navy outline fitted as an axis-aligned ellipse, the cylinder as wide as the top,
the luminance across the cylinder (lighter left of centre, darker to the right, a dark edge line on its right) and its soft
shadow on the wall to the right. The pictures on the tops (a grass mound with a red peak, a magenta square or a red grid,
two shaded yellow arrows and their shadow; a ball with three coloured trails) are rendered 3D art: kept as art (rule 7),
their region recorded as a keep shape. Local coordinates of each cut.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from geom import rect, line, poly, oval, U  # noqa: E402

STOPS = [0.0, 0.8, 1.6, 2.6, 4.0, 6.0, 10.0]


def pedestal(cx, cy, rx, ry, W, keep):
    return [
        ("back wall", rect(0, 0, W, 99), "bg"),
        ("the cylinder's soft shadow on the wall to its right", rect(cx + rx, cy, min(W, cx + rx + 7.0), 99), "cshadow"),
        ("cylinder body", rect(cx - rx, cy, cx + rx, 99), "cyl"),
        ("cylinder: dark edge line on its right", line((cx + rx - 0.6, cy), (cx + rx - 0.6, 99), w=1.2), "cedge"),
        ("top: an ellipse, navy outline and rim in its measured bevel", oval(cx, cy, rx, ry), "top"),
        ("picture on the top, kept as art", keep, "keep"),
    ]


BIG_KEEP = U(rect(26, 1, 60, 31), oval(41.0, 47.5, 21.0, 14.5), poly((55, 13), (70, 17), (72, 36), (58, 38)))
SMOOTH_KEEP = U(poly((13, 47), (28, 31), (48, 23), (72, 24), (72, 45), (42, 52), (30, 63), (13, 61)), oval(28.6, 50.2, 22.6, 13.8))
PAINT = {"top": ("bevel", STOPS), "bg": "linear", "cyl": "linear", "cshadow": "linear"}
HOW = ("cylinder pedestal: back wall, the cylinder (shading measured across it, a dark edge line on its right, a soft "
       "shadow on the wall), its elliptical top (navy outline and rim, state colour) in a measured bevel; the picture on "
       "top kept as art: ")
SPECS = [
    dict(id="elevation-point", parts=pedestal(40.6, 48.4, 37.6, 23.6, 88, BIG_KEEP), paint=PAINT,
         how=HOW + "a grass mound with a red peak and two shaded yellow arrows"),
    dict(id="elevation-square", parts=pedestal(40.4, 48.4, 37.4, 23.6, 88, BIG_KEEP), paint=PAINT,
         how=HOW + "a grass mound with a magenta flat top and two shaded yellow arrows"),
    dict(id="elevation-area", parts=pedestal(40.4, 48.4, 37.2, 23.6, 88, BIG_KEEP), paint=PAINT,
         how=HOW + "a wide grass mound under a red grid and two shaded yellow arrows"),
    dict(id="elevation-smooth", parts=pedestal(28.6, 50.2, 25.0, 16.2, 72, SMOOTH_KEEP), paint=PAINT,
         how=HOW + "a grass disc filling the top, a ball on it shooting three green, white and yellow trails"),
]

if __name__ == "__main__":
    import fel
    want = sys.argv[1:]
    for sp in SPECS:
        if not want or sp["id"] in want:
            fel.build(sp)
