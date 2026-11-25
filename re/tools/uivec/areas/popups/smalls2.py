"""P41 popups fix round: popup-check-box ("shade on the tick and cross"), trans-spinner ("center the arrows"),
trans-lens ("proper lightning of the buttons"), trans-small-arrows (redo).

py -3.12 smalls2.py [id ...]
"""
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import w2               # noqa: E402
import common as c      # noqa: E402
import auto             # noqa: E402
import paint            # noqa: E402
import smalls           # noqa: E402
from auto import ell, mv, poly  # noqa: E402

IB, TP = smalls.IB, smalls.TP


def shaded(S, bands, light=None):
    """A mark lit from the upper left, as layers: the shape in its darkest edge colour, then for each (dx, dy, colour) the shape
    cut to itself moved up-left by (dx, dy), so each later colour stops short of the lower-right edges and the earlier ones
    show there as bands; light: an optional (dx, dy, colour) band along the upper-left edges (the shape minus itself moved
    down-right)."""
    out = [{"shape": S, "fill": list(bands[0][2])}]
    for dx, dy, col in bands[1:]:
        out.append({"shape": {"t": "inter", "of": [S, mv(S, -dx, -dy)]}, "fill": list(col)})
    if light:
        dx, dy, col = light
        out.append({"shape": {"t": "sub", "a": S, "b": mv(S, dx, dy)}, "fill": list(col), "clip": S})
    return out


def check_box():
    """The tick and the cross keep the first round's outlines (smalls.check_box); their paint is now the art's lighting.
    Tick, colours counted on its pixels: body 00f870 (75 px), a band of 00e868 / 08d060 along the long arm's lower right edge
    and the V's underside (the 'C' and 'B' runs of rows 3-19), the outermost row 08b850 there; the upper left edges and the short
    arm's left end stay the body colour. Cross: body f80048 (70 px), e00048 along the right side of both strokes (rows 3-20),
    and the strokes' lower ends darker (c80840, b00838, 581838 on rows 20-23)."""
    frame = paint.bevel_from_bands([[40, 48, 88], [32, 40, 72]], [[80, 88, 112], [120, 120, 160]],
                                   [[48, 56, 88], [32, 40, 80]], [[72, 80, 112], [80, 88, 112]], [240, 216, 168])

    def box_layer(dx):
        return {"shape": c.rr(303 + dx, 3, 323 + dx, 23, 1.0), "fill": frame}
    check = poly((352.9, 8.0), (356.0, 8.0), (360.5, 14.0), (361.0, 13.5), (369.0, 1.0), (369.6, 0.5), (370.8, 0.0),
                 (372.4, 0.1), (373.6, 1.2), (374.1, 3.2), (363.2, 20.0), (360.0, 20.0), (353.4, 13.6), (352.2, 12.0), (352.2, 9.0))
    sA = {"t": "qstroke", "p": [[409.0, 2.0], [411.0, 11.0], [416.7, 23.6]], "w": [2.2, 3.8]}
    sB = {"t": "qstroke", "p": [[420.9, 2.0], [413.0, 11.6], [406.6, 21.0]], "w": [2.8, 3.4]}
    tick = shaded(check, [(0, 0, (8, 184, 80)), (0.9, 0.8, (8, 208, 96)), (1.5, 1.4, (0, 232, 104)), (2.0, 1.9, (0, 248, 112))])
    cross = []
    for S in (sB, sA):
        cross += shaded(S, [(0, 0, (200, 8, 64)), (0.8, 0.3, (224, 0, 72)), (1.3, 0.7, (248, 0, 72))])
    # the strokes' lower ends in shadow: a darker cap over the last 2.5 px of each stroke
    for S, cut in ((sA, {"t": "half", "p": [0.0, 21.2], "n": [0.0, -1.0]}), (sB, {"t": "half", "p": [0.0, 19.0], "n": [0.0, -1.0]})):
        cross.append({"shape": {"t": "inter", "of": [S, cut]}, "fill": [176, 8, 56], "soft": 0.8, "clip": S})
    states = []
    for st, dx, extra in (("empty", 0, []), ("checked", 50, tick), ("crossed", 100, cross)):
        box = (300 + dx, 0, 326 + dx, 26)
        states.append((st, box, c.item(box, [box_layer(dx)] + extra, f"popup-check-box {st}")))
    return auto.deliver("popup-check-box", IB, states, z=8, how="")


def spinner():
    """The first round's disc, cap and paints (smalls.spinner), with the two triangles centred on the disc.
    Measured on the idle cell: the disc's rim spans x 360-389.6 and y 48-73.9 (circle c (374.85, 60.95), r 15.05); the art's
    triangles have their apex column at x 374.5 and bases x 368-381 (axis 374.5, 0.35 px left of the disc's), the up one
    from y 52.2 to 61.0 and the down one from 62.0 to 70.3, so the yellow gap row between them (y 61) sits 0.55 px below the
    disc's centre. Now both are one triangle mirrored about the disc's horizontal axis and centred on its vertical axis:
    base 15.2 wide, 8.6 tall, the gap 1.0 px centred on the disc's centre."""
    states = []
    for i, st in enumerate(("idle", "hover", "disabled")):
        dx = 40 * i
        dy = -1.0 if st == "hover" else 0.0
        cx, cy = 374.85 + dx, 60.95 + dy
        box = (360 + dx, 34, 390 + dx, 76)
        disc = {"t": "circle", "c": [cx, cy], "r": 15.05}
        cap = c.rr(360 + dx, 45, 390 + dx, 50, 0.0)
        h, half, g = 8.6, 7.6, 0.5
        up = poly((cx, cy - g - h), (cx - half, cy - g), (cx + half, cy - g))
        dn = poly((cx - half, cy + g), (cx + half, cy + g), (cx, cy + g + h))
        parts = [(cap, {"linear": {"n": 5, "dir": (0.0, 1.0)}}),
                 (disc, {"bevel": {"stops": [0.04, 0.08, 0.13, 0.19, 0.27, 0.38, 0.55, 0.75, 1.0], "L": 15.05,
                                   "radial": (cx, cy)}}),
                 (up, "flat"), (dn, "flat")]
        states.append((st, box, auto.measured_item(TP, box, parts, name=f"trans-spinner {st}")))
    return auto.deliver("trans-spinner", TP, states, z=6, how="")


# ----------------------------------------------------------------------------------------------- the small spinner arrows
# The plate, read as each row's opaque extent on the up cell (rows 200-211): a pointed dome (x 19-22 on row 200, then
# 15-24, 12-27, 9-30, 8-33, 7-34, 6-33, 5-32, 4-32) on a bar 3 rows tall (rows 209-211, x 3-32). The down cell's plate is the
# same shape upside down (row y <-> 431 - y: every row extent matches), but its LIGHT is not mirrored: both cells are lit from
# the upper left, so the up plate has a light upper-left rim, the down plate dark lower edges.
DOME = [(3.0, 209.4), (4.0, 208.0), (5.0, 207.0), (6.0, 206.0), (7.0, 205.0), (8.0, 204.0), (9.0, 203.0), (12.0, 202.0),
        (15.0, 201.0), (19.0, 200.0), (23.0, 200.0), (25.0, 201.0), (28.0, 202.0), (31.0, 203.0), (34.0, 204.0),
        (35.0, 205.0), (34.0, 206.0), (33.0, 207.0), (33.0, 209.4)]


def plate_shape(flip, dy):
    dome = poly(*[(x, (432.0 - y if flip else y) + dy) for x, y in DOME])
    by = (432.0 - 210.5 if flip else 210.5) + dy
    return {"t": "union", "of": [dome, c.rr(3.0, by - 1.5, 33.0, by + 1.5, 0.0)]}


def half(y=None, x=None, below=True, right=True):
    if y is not None:
        return {"t": "half", "p": [0.0, y], "n": [0.0, -1.0 if below else 1.0]}
    return {"t": "half", "p": [x, 0.0], "n": [-1.0 if right else 1.0, 0.0]}


# colours read on the cells' pixel rows (hex in the comments of small_arrows); pair 1 = cream triangles, pair 2 = orange
PAL = {
    1: {"plate": [149, 151, 198], "rim_r": [163, 164, 206], "rim_l": [200, 201, 226], "groove": [129, 131, 171],
        "lip": [206, 207, 230], "face": [232, 202, 125], "shade1": [212, 194, 123], "shade2": [197, 172, 106],
        "cream": [250, 234, 196], "white": [250, 248, 240],
        "dplate": [145, 147, 192], "dbar": [126, 128, 167], "dedge1": [108, 109, 144], "dedge2": [85, 87, 113],
        "dedge3": [58, 62, 80], "dlip": [216, 217, 234], "dface": [226, 200, 128], "dshade": [200, 178, 110],
        "dtop": [245, 233, 200], "dwhite": [245, 246, 249]},
    2: {"plate": [149, 151, 198], "rim_r": [163, 164, 206], "rim_l": [200, 201, 226], "groove": [104, 112, 186],
        "lip": [206, 207, 230], "face": [252, 193, 2], "shade1": [246, 180, 2], "shade2": [246, 164, 2],
        "cream": [254, 232, 138], "white": [254, 250, 232],
        "dplate": [145, 147, 192], "dbar": [104, 112, 186], "dedge1": [108, 109, 144], "dedge2": [85, 87, 113],
        "dedge3": [58, 62, 80], "dlip": [216, 217, 234], "dface": [246, 180, 2], "dshade": [246, 164, 2],
        "dtop": [254, 232, 138], "dwhite": [245, 246, 249]},
}


def up_cell(y0, apex, base_y, bx0, bx1, k):
    """An up arrow cell: plate + raised triangle lit from the upper left, every band its own layer and colour."""
    C = PAL[k]
    P = plate_shape(False, y0 - 200)
    T = poly(apex, (bx1, base_y), (bx0, base_y))
    ax = apex[0]
    return [
        (P, C["plate"]),
        ({"t": "inter", "of": [{"t": "sub", "a": P, "b": mv(P, -1.2, 1.2)}, half(x=ax, right=True)]}, C["rim_r"]),
        ({"t": "inter", "of": [{"t": "sub", "a": P, "b": mv(P, 1.6, 1.3)}, half(x=ax, right=False)]}, C["rim_l"]),
        ({"t": "inter", "of": [{"t": "sub", "a": mv(T, -2.0, 0.0), "b": T}, P]}, C["groove"]),
        (c.rr(bx0 + 1.0, base_y, 32.5, base_y + 2.0, 0.0), C["lip"]),
        (T, C["face"]),
        ({"t": "inter", "of": [T, half(y=base_y - 2.6)]}, C["shade1"], {"soft": 0.4}),
        ({"t": "inter", "of": [T, half(y=base_y - 1.4)]}, C["shade2"], {"soft": 0.4}),
        ({"t": "sub", "a": T, "b": mv(T, 4.4, 0.0)}, C["cream"], {"soft": 0.4, "clip": T}),
        ({"t": "sub", "a": T, "b": mv(T, 2.5, 0.0)}, C["white"], {"clip": T}),
    ]


def down_cell(y0, apex, base_y, bx0, bx1, k):
    C = PAL[k]
    P = plate_shape(True, y0 - 220)
    T = poly((bx0, base_y), (bx1, base_y), apex)
    ax = apex[0]
    return [
        (P, C["dplate"]),
        (c.rr(7.5, y0, 28.5, y0 + 2.0, 0.0), C["dbar"]),
        ({"t": "inter", "of": [{"t": "sub", "a": P, "b": mv(P, 0.0, -3.0)}, half(y=y0 + 4.0)]}, C["dedge1"], {"clip": P}),
        ({"t": "inter", "of": [{"t": "sub", "a": P, "b": mv(P, 0.0, -1.5)}, half(y=y0 + 4.0)]}, C["dedge2"], {"clip": P}),
        ({"t": "inter", "of": [{"t": "sub", "a": P, "b": mv(P, -1.2, -1.2)}, half(x=ax + 2.0, right=True), half(y=y0 + 5.0)]},
         C["dedge3"], {"clip": P}),
        ({"t": "inter", "of": [{"t": "sub", "a": mv(T, 1.8, 0.0), "b": T}, P, half(y=y0 + 6.0, below=False)]}, C["dlip"]),
        (T, C["dface"]),
        ({"t": "inter", "of": [T, half(y=base_y + 3.0)]}, C["dshade"], {"soft": 0.4}),
        ({"t": "inter", "of": [T, half(y=base_y + 1.7, below=False)]}, C["dtop"], {"soft": 0.4}),
        ({"t": "sub", "a": T, "b": mv(T, -2.8, 0.0)}, C["dwhite"], {"clip": T}),
    ]


SA_CELLS = [  # state, cell top, kind, pair, apex, base y, base x0, base x1 (first / last triangle pixel per row on each cell)
    ("up", 200, "up", 1, (20.2, 200.9), 209.0, 8.6, 31.8),
    ("down", 220, "down", 1, (20.6, 230.9), 222.0, 9.0, 32.4),
    ("up 2", 250, "up", 2, (20.2, 251.9), 260.0, 8.6, 31.9),
    ("down 2", 270, "down", 2, (20.7, 279.9), 271.0, 9.0, 32.4),
]


def small_arrows():
    """Redo from a new reading (see SA_CELLS and the plate notes above). Up cells: a plate whose upper-left rim is lit (cccde4,
    bebfdc), a dark groove beside the triangle's left edge (8183ab; 6870ba in the second pair), a light lip under the base
    (rows 209-210: cccde4 / d8d9ea), and a raised triangle: white light edge along its left side (f2f2ed / fdf9f3, 2.4 px),
    a cream band inside it (fcf1d9 / f5dda4), tan face e8ca7d (orange fcc102 / f6b402 in the second pair) and its last rows in
    shade (c5ac6a; f6a402). Down cells: the plate's lower edges in shadow (666887 .. 373949, darkest at the lower right), a
    darker bar on top (8183ab / 7b7ca3; 6870ba), the triangle's top rows light (f5e9c8; fee88a / fef1c6), a white light edge
    along its right side (f5f6f9, 2.8 px), its lower body tan (c5ac6a / d4c27b; f6a402). In the second pair both triangles sit
    one row nearer the bar (up: apex 251.9 and base 260 against 200.9 / 209 + 50; down: base 271 and apex 279.9 against 222 /
    230.9 + 50)."""
    cells = []
    for st, y0, kind, k, apex, by, bx0, bx1 in SA_CELLS:
        box = (0, y0, 35, y0 + 12)
        parts = up_cell(y0, apex, by, bx0, bx1, k) if kind == "up" else down_cell(y0, apex, by, bx0, bx1, k)
        cells.append((st, box, auto.measured_item(TP, box, parts, inset=0.25, gap=0.35, name=f"trans-small-arrows {st}")))
    return auto.deliver("trans-small-arrows", TP, cells, z=8, how="")


def lens():
    """The lens body and the white oval at its right end as in the first round (smalls.lens: ellipse c (21, 162), r (21, 12),
    its lit surface measured as a bevel paint). The two triangle buttons are redrawn with the light the art gives them, as
    layers (colours read on the rows of TransPopups.pcx 150-174):
    - up button: apex (19.9, 152.2), base y 160, x 8.6-31.6; face e8ca7d; a cream band f5dda4 along its upper-left side (x 15-18
      on rows 154-157); its last rows in shade: d4c27b (row 157), c5ac6a (158), 9e8c50 (159, the base row); a dark groove beside
      its left edge (666887 / 5e607e, x 11-14 on rows 154-157) and a darker band beside its right edge (8183ab); a light lip
      row under the base (row 160: ababd2);
    - a dark row across the lens between the buttons (row 162: 6c6d90);
    - down button: base y 163, x 8.6-31.6, apex (20.2, 170.4); its top rows light (f5dda4 on row 163, eed697 on 164), face
      e8ca7d, its right side in shade (c5ac6a, then 9e8c50 2 px along the right edge from row 165), and a lit lip of the lens
      beside its right edge (b9b8c9 / bebfdc on rows 165-169)."""
    box = (0, 150, 40, 174)
    body = ell(21.0, 162.0, 21.0, 12.0)
    shine = {"t": "inter", "of": [body, ell(40.0, 161.2, 6.0, 5.2)]}
    U = poly((19.9, 152.2), (31.6, 160.0), (8.6, 160.0))
    D = poly((8.6, 163.0), (31.6, 163.0), (20.2, 170.4))
    parts = [
        (body, {"bevel": {"stops": [0.5, 1.5, 2.5, 4.0, 6.0, 9.0]}}),
        (shine, "flat"),
        # bands beside the up button's slanted edges: the button grown 2 px minus itself, cut to one side of its axis and to
        # the rows its edges span (a horizontal shift of the triangle would poke past the apex)
        ({"t": "inter", "of": [{"t": "sub", "a": {"t": "grow", "of": U, "d": 1.8}, "b": U}, body, half(x=19.9, right=False),
                               half(y=153.0), half(y=159.6, below=False)]}, [100, 102, 134], {"soft": 0.3}),
        ({"t": "inter", "of": [{"t": "sub", "a": {"t": "grow", "of": U, "d": 1.4}, "b": U}, body, half(x=19.9, right=True),
                               half(y=153.5), half(y=159.0, below=False)]}, [129, 131, 171], {"soft": 0.3}),
        (c.rr(8.6, 160.0, 32.0, 161.0, 0.0), [171, 171, 210]),
        (U, [232, 202, 125]),
        ({"t": "sub", "a": U, "b": mv(U, 4.0, 0.0)}, [245, 221, 164], {"clip": U, "soft": 0.4}),
        ({"t": "inter", "of": [U, half(y=157.0)]}, [212, 194, 123]),
        ({"t": "inter", "of": [U, half(y=158.0)]}, [197, 172, 106]),
        ({"t": "inter", "of": [U, half(y=159.0)]}, [158, 140, 80]),
        (c.rr(9.5, 162.0, 29.0, 163.0, 0.0), [108, 109, 144]),
        ({"t": "inter", "of": [{"t": "sub", "a": {"t": "grow", "of": D, "d": 1.8}, "b": D}, body, half(x=20.2, right=True),
                               half(y=164.5), half(y=170.0, below=False)]}, [190, 191, 220], {"soft": 0.3}),
        (D, [232, 202, 125]),
        ({"t": "inter", "of": [D, half(y=164.8, below=False)]}, [238, 214, 151]),
        ({"t": "inter", "of": [D, half(y=164.0, below=False)]}, [245, 221, 164]),
        ({"t": "inter", "of": [D, half(y=166.0)]}, [210, 190, 120], {"soft": 0.5}),
        ({"t": "inter", "of": [{"t": "sub", "a": D, "b": mv(D, -3.8, 0.0)}, half(y=163.8)]}, [197, 172, 106], {"clip": D}),
        ({"t": "inter", "of": [{"t": "sub", "a": D, "b": mv(D, -2.0, 0.0)}, half(y=164.8)]}, [158, 140, 80], {"clip": D}),
    ]
    it = auto.measured_item(TP, box, parts, name="trans-lens")
    return auto.deliver("trans-lens", TP, [("idle", box, it)], z=10, how="")


FNS = {"popup-check-box": check_box, "trans-spinner": spinner, "trans-small-arrows": small_arrows, "trans-lens": lens}

if __name__ == "__main__":
    todo = sys.argv[1:] or list(FNS)
    for k, eid in enumerate(todo):
        print(f"[{k + 1}/{len(todo)}] {eid} {FNS[eid]()}", flush=True)
