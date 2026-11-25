"""P41 fix round (2026-11-24): the roster camera (redo) and heart ("weird heart"), re-read from memberRoster_buttons.pcx
pixel dumps (cell row 1 at x 126 / 146, y 1; the legend copies are the same pixels moved by RM.CAM_OFF / RM.HEART_OFF).

HEART (cell x 146..164): symmetric about x 155.5. Not two circles over a triangle: the outer sides are STRAIGHT and
vertical from y 4.3 to 8.3 (x 147.4 / 163.6; column 147 is 0.4-0.6 dark on rows 5..7), the lobe tops flat at y 3.0 with
rounded outer corners (row 3: 0.45 at x 148, 0.8 at 149), a V notch between the lobes down to y 5.0 at the axis (row 4:
x 155 light, 154 / 156 half), then straight 45-degree sides (the edge moves 0.93 px per row: 147.9 at y 8.5 .. 154.4 at
y 15.5) to a narrow drawn-out tip (one column, x 154.9..156.1, to y 17.6). Flat navy (36, 47, 84): no highlight in the art.

CAMERA (cell x 127..144): a body (x 128.4..142.9, y 6.0..17.0), a prism hump (top x 133.9..137.6 at y 1.9, foot x 131.4..
139.4 at y 6.2: the left wall slants 0.65 px / row, the right one 0.5), a knob at the top right (x 141.4..143.2, y 4.8..
6.4), a cream lens ring (outer x 132.3..139.2, y 7.6..14.3: centre (135.75, 10.95), r 3.45) round a blue lens (centre
(135.65, 10.8), r 1.75, (49, 65, 119)). LIT from the top left: the hump's left half and the body's top-left edge and left
column are light blue (63, 83, 150 / 72, 85, 133), the right of the hump dark (31, 44, 89); the ring is white at the top
(249) and greyer at the bottom right (226). The body's light is the measured bevel paint (lit.py: depth x edge direction,
solved on the cream cell, the tan cell and the legend with the lens left out); the ring a measured linear tone.
"""
import json
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import common as C          # noqa: E402
import lit                  # noqa: E402
import parts as P           # noqa: E402
import roster_marks as RM   # noqa: E402

OUT2 = os.path.join(C.ROOT, "log", "roadmap", "P41", "work2", "reports_maps")
CAM_FILE = os.path.join(OUT2, "read", "camera_style.json")
NAVY = [36, 47, 84]


def mv(S, dx, dy):
    return S if not (dx or dy) else {"t": "move", "of": S, "d": [dx, dy]}


def heart_shape():
    ax = 155.5
    # right half, notch -> round lobe top -> straight vertical side -> straight 45-degree side -> tip (read off the rows)
    half = [(ax, 5.0), (ax, 5.0), (156.4, 4.0), (157.4, 3.3), (158.6, 2.95), (160.6, 2.95), (162.2, 3.25), (163.2, 4.0),
            (163.6, 5.2), (163.6, 8.0), (163.3, 8.7), (156.3, 15.9), (155.5, 17.9), (155.5, 17.9)]
    right = [[x, y] for x, y in half]
    left = [[2 * ax - x, y] for x, y in reversed(half[:-2])]
    # two rounds of corner cutting round the lobes; the notch and the tip are doubled points, so they stay sharp
    return {"t": "poly", "rings": [right + left], "smooth": 2}


def heart(dx=0.0, dy=0.0):
    return [{"shape": mv(heart_shape(), dx, dy), "fill": NAVY}]


def cam_parts():
    body = P.rr(128.4, 6.0, 142.9, 17.0, 0.7)
    hump = {"t": "poly", "rings": [[[133.9, 1.9], [137.6, 1.9], [139.4, 6.4], [131.4, 6.4]]], "smooth": 1}
    knob = P.rr(141.4, 4.8, 143.2, 6.6, 0.5)
    S = {"t": "union", "of": [body, hump, knob]}
    ring = {"t": "circle", "c": [135.75, 10.95], "r": 3.45}
    lens = {"t": "circle", "c": [135.65, 10.8], "r": 1.75}
    return S, ring, lens


LIGHT = [66, 82, 142]


def camera(dx=0.0, dy=0.0, st=None):
    """Navy silhouette (body, hump, knob) with the lens ring cut out of it as a HOLE (the art shows the cell's cream / tan and
    the legend's lavender through it), the blue lens in the middle. Light from the top left, as the art's pixels: a lit rim
    1.3 px wide along the body's top and left edges (63..72, 83..85, 133..150 in the art), the hump shaded across from light
    blue at its top-left to dark navy at its foot on the right."""
    S, ring, lens = cam_parts()
    body = P.rr(128.4, 6.0, 142.9, 17.0, 0.7)
    knob = P.rr(141.4, 4.8, 143.2, 6.6, 0.5)
    hump = {"t": "poly", "rings": [[[133.9, 1.9], [137.6, 1.9], [139.4, 6.4], [131.4, 6.4]]], "smooth": 1}
    hole = mv(ring, dx, dy)
    sil = {"t": "sub", "a": mv(S, dx, dy), "b": hole}
    rim = {"t": "sub", "a": {"t": "sub", "a": mv(body, dx, dy), "b": mv(body, dx + 1.2, dy + 1.2)}, "b": hole}
    return [{"shape": sil, "fill": NAVY},
            {"shape": rim, "fill": LIGHT, "clip": sil},
            {"shape": mv(hump, dx, dy), "fill": {"linear": {"a": [133.6 + dx, 2.0 + dy], "b": [138.6 + dx, 6.2 + dy],
                                                            "stops": [[0, [64, 84, 150]], [0.45, [55, 72, 130]], [1, [32, 43, 86]]]}}},
            {"shape": mv(lens, dx, dy), "fill": {"linear": {"a": [134.4 + dx, 9.6 + dy], "b": [136.9 + dx, 12.1 + dy],
                                                            "stops": [[0, [60, 72, 122]], [1, [46, 58, 110]]]}}}]



