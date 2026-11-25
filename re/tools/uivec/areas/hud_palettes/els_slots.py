"""Slot shapes: the building palette's iso diamond tiles on pillars (tall and short) and the amenities palette's smaller ones.

Read on the cuts with cmap.py (colour clusters per pixel) and the navy outline's extremes (2026-11-24). A slot is
  ground      the tray floor the cut carries round the tile (the cut's extent, simplified), light lavender, measured;
  shade band  a darker strip along the outside of the diamond's upper right edge (the tile's raised border in shadow);
  left face   the pillar's lit side below the diamond's lower left edge;
  right face  the pillar's shaded side below its lower right edge;
  top         the diamond itself: one rhombus with slightly rounded corners, painted with a bevel measured on it (the 1.5 px
              navy outline, the light rim inside its upper left edges, the darker rim inside its lower right edges, the
              flat teal / green / navy top), so states differ only in paint.
Coordinates local to each cut.
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from geom import poly  # noqa: E402

STOPS = [0.0, 0.8, 1.6, 2.4, 3.2, 4.4, 6.5, 10.0]


def strip(a, b, width):
    """The band along the outside of edge a->b (outward = to the left of the direction of travel on screen), width px."""
    dx, dy = b[0] - a[0], b[1] - a[1]
    n = math.hypot(dx, dy)
    nx, ny = dy / n, -dx / n
    return poly(a, b, (b[0] + nx * width, b[1] + ny * width), (a[0] + nx * width, a[1] + ny * width))


def slot(T, L, R, Bv, bottom, side_y, w, band=5.0, round_=0):
    left_face = poly(L, Bv, (Bv[0], bottom), (0.0, side_y), (0.0, L[1]))
    right_face = poly(Bv, R, (w, R[1]), (w, side_y), (Bv[0] + 1.5, bottom), (Bv[0], bottom))
    top = poly(T, R, Bv, L, smooth=round_) if round_ else poly(T, R, Bv, L)
    return [("ground: the tray floor round the tile (the cut's extent)", "EXTENT", "ground"),
            ("shade band along the outside of the upper right edge (inside the cut's extent)", ("BAND", strip(T, R, band)), "band"),
            ("pillar: lit left face", left_face, "left"),
            ("pillar: shaded right face", right_face, "right"),
            ("tile top: a rhombus, its rims in a measured bevel", top, "top"),
            ("tile top: its 1.4 px navy outline", {"t": "outline", "of": {"t": "grow", "of": top, "d": -0.7}, "w": 1.4}, "outline")]


BUILD_TALL = slot((38.5, 1.4), (1.5, 26.0), (75.6, 25.4), (38.6, 51.6), 64.0, 40.0, 77.0)
BUILD_SHORT = slot((37.0, 1.6), (1.5, 27.0), (75.6, 27.0), (38.6, 52.0), 58.0, 38.0, 77.0)
AMEN = slot((23.6, 3.6), (1.6, 18.6), (45.2, 18.8), (24.2, 33.4), 40.0, 28.0, 47.0, band=3.5)

SPECS = [
    dict(id="building-slot", parts=BUILD_TALL,
         parts_by_state={"idle short": BUILD_SHORT, "selected short": BUILD_SHORT, "disabled short": BUILD_SHORT},
         paint={"top": ("bevel", STOPS), "ground": "linear", "left": "linear", "right": "linear"},
         how="iso diamond tile on a pillar: a rhombus top (1.5 px navy outline, light rim on its upper left edges, darker "
             "rim on its lower right, teal / green / navy by state, one measured bevel), a darker band along the outside "
             "of its upper right edge, the pillar's lit left and shaded right faces below; a tall and a short pillar"),
    dict(id="amenities-slot", parts=AMEN,
         paint={"top": ("bevel", STOPS), "ground": "linear", "left": "linear", "right": "linear"},
         how="a small iso diamond tile on a short pillar, built like the building slot: rhombus top in a measured bevel "
             "(lilac idle, light blue selected, dark disabled), a darker band along its upper right edge, lit left and "
             "shaded right pillar faces"),
]


def resolve(spec):
    """Put each cut's simplified extent in place of the EXTENT marker."""
    import hp
    import bel
    cs = {c[0]: c for c in hp.crops(spec["id"])}
    pbs = {}
    for st, c in cs.items():
        parts = spec.get("parts_by_state", {}).get(st, spec["parts"])
        ext = bel.extent_poly(c[5])
        pbs[st] = [(n, ext if s == "EXTENT" else ({"t": "inter", "of": [s[1], ext]} if isinstance(s, tuple) else s), k)
                   for n, s, k in parts]
    spec = dict(spec)
    spec["parts_by_state"] = pbs
    spec["parts"] = pbs[list(cs)[0]]
    return spec


if __name__ == "__main__":
    import fel
    want = sys.argv[1:]
    for sp in SPECS:
        if not want or sp["id"] in want:
            fel.build(resolve(sp))
