"""P41 popups area: small chrome: popup-check-box, trans-spinner, trans-lens, trans-small-arrows, popup-arrow-glyph.

Shapes are read on the pixel grid (face maps printed per pixel class: navy / lilac / yellow / white / tan); paints are measured
on each part's own pixels (auto.measured_item) or read as exact edge rows (paint.bevel_from_bands).

Usage: py -3.12 smalls.py [id ...]
"""
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import auto             # noqa: E402
import common as c      # noqa: E402
import paint            # noqa: E402
from auto import ell, mv, poly  # noqa: E402

IB, TP = "InfoButtons.pcx", "TransPopups.pcx"
BLACK = np.array((0.0, 0.0, 0.0))


def check_box():
    # the box (cell 0): x 303-323, y 3-23; a 2 px frame read side by side: dark navy top and left, greyer navy-lilac below and
    # right (it doubles as the shadow); cream inside; corners rounded about 1 px
    frame = paint.bevel_from_bands([[40, 48, 88], [32, 40, 72]], [[80, 88, 112], [120, 120, 160]],
                                   [[48, 56, 88], [32, 40, 80]], [[72, 80, 112], [80, 88, 112]], [240, 216, 168])
    def box_layer(dx):
        return {"shape": c.rr(303 + dx, 3, 323 + dx, 23, 1.0), "fill": frame}
    GREEN = [0, 236, 106]
    # the check (cell 1), read as runs of green per row: a short thick arm with a flat top (y 8, x 352.9-356) that reaches out
    # over the box's left edge, its far end square (x 352.2, rows 9-12); the notch between the arms at (360.5, 14); the long
    # arm's straight edges (right: (374.1, 3.2) to (363.2, 20), left: (361, 13.5) to (369, 1)) and its rounded top end at
    # (371.5, 0); a flat bottom y 20, x 360-363.2
    check = poly((352.9, 8.0), (356.0, 8.0), (360.5, 14.0), (361.0, 13.5), (369.0, 1.0), (369.6, 0.5), (370.8, 0.0),
                 (372.4, 0.1), (373.6, 1.2), (374.1, 3.2), (363.2, 20.0), (360.0, 20.0), (353.4, 13.6), (352.2, 12.0), (352.2, 9.0))
    RED = [220, 6, 68]
    # the cross (cell 2), read as runs of red per row: two brush strokes that widen as they go down, crossing at (413, 12): one
    # from the top left (409, 2), nearly upright at first, to the bottom right (416.7, 23.6); the other from the top right
    # (420.9, 2) to the bottom left (406.6, 21)
    sA = {"t": "qstroke", "p": [[409.0, 2.0], [411.0, 11.0], [416.7, 23.6]], "w": [2.2, 3.8]}
    sB = {"t": "qstroke", "p": [[420.9, 2.0], [413.0, 11.6], [406.6, 21.0]], "w": [2.8, 3.4]}
    states = []
    for st, dx, extra in (("empty", 0, []), ("checked", 50, [{"shape": check, "fill": GREEN}]),
                          ("crossed", 100, [{"shape": sA, "fill": RED}, {"shape": sB, "fill": RED}])):
        box = (300 + dx, 0, 326 + dx, 26)
        states.append((st, box, c.item(box, [box_layer(dx)] + extra, f"popup-check-box {st}")))
    return auto.deliver("popup-check-box", IB, states, how="a cream square 20 x 20 in a 2 px navy frame (dark on top and left, "
                        "greyer below and right, read side by side), corners rounded 1 px; checked: a thick green check whose "
                        "short arm reaches out over the frame's left edge and whose long arm ends above the box (one outline read "
                        "row by row, a darker green rim); crossed: two red brush strokes that widen downward, crossing a little "
                        "above the middle", z=8)


def spinner():
    states = []
    for i, st in enumerate(("idle", "hover", "disabled")):
        dx = 40 * i
        dy = -1.0 if st == "hover" else 0.0     # the hover disc and its triangles sit one row higher (top rim y 45, gap row 60)
        box = (360 + dx, 34, 390 + dx, 76)
        disc = {"t": "circle", "c": [374.85 + dx, 60.95 + dy], "r": 15.05}
        cap = c.rr(360 + dx, 45, 390 + dx, 50, 0.0)
        up = poly((374.5 + dx, 52.4 + dy), (366.9 + dx, 61.0 + dy), (382.1 + dx, 61.0 + dy))
        dn = poly((366.9 + dx, 62.0 + dy), (382.1 + dx, 62.0 + dy), (374.5 + dx, 70.6 + dy))
        parts = [(cap, {"linear": {"n": 5, "dir": (0.0, 1.0)}}),
                 (disc, {"bevel": {"stops": [0.04, 0.08, 0.13, 0.19, 0.27, 0.38, 0.55, 0.75, 1.0], "L": 15.05,
                                   "radial": (374.85 + dx, 60.95 + dy)}}),
                 (up, "flat"), (dn, "flat")]
        states.append((st, box, auto.measured_item(TP, box, parts, name=f"trans-spinner {st}")))
    return auto.deliver("trans-spinner", TP, states, how="a lilac cap strip 30 x 5 (lit from above, measured) under a yellow disc "
                        "r 15 in a navy rim (the disc's whole lit surface, rim included, measured per state), and two navy "
                        "triangles, up (apex y 52.4) and down (apex y 70.6), on bases 15.2 px wide a pixel apart (the yellow line "
                        "between them is the disc showing through)", z=6)


def tri_up(apex_y, base_y, cx=20.5, half=11.5):
    return poly((cx - 0.5, apex_y), (cx + 0.5, apex_y), (cx + half, base_y), (cx - half, base_y))


def lit_edge(apex_y, base_y, cx=20.5, half=11.5, up=True):
    """The white light line along the triangle's left side (2 px of white at rows 202-206 of the up arrow)."""
    return {"t": "stroke", "pts": [[cx - 0.6, apex_y + (1.0 if up else -1.0)], [cx - half + 1.4, base_y - (0.6 if up else -0.6)]],
            "w": 1.3, "cap": "butt"}


def lens():
    box = (0, 150, 40, 174)
    body = ell(21.0, 162.0, 21.0, 12.0)
    shine = {"t": "inter", "of": [body, ell(40.0, 161.2, 6.0, 5.2)]}
    up, dn = tri_up(152.0, 160.0), poly((9.0, 163.0), (32.0, 163.0), (21.0, 170.0), (20.0, 170.0))
    parts = [(body, {"bevel": {"stops": [0.5, 1.5, 2.5, 4.0, 6.0, 9.0]}}), (shine, "flat"),
             (up, {"linear": {"n": 3}}), (lit_edge(152.0, 160.0), [250, 248, 238], {"clip": up, "alpha": 0.6}),
             (dn, {"linear": {"n": 3}})]
    it = auto.measured_item(TP, box, parts, name="trans-lens")
    return auto.deliver("trans-lens", TP, [("idle", box, it)], how="a lilac lens (ellipse c (21, 162), r (21, 12), cut by the cell's "
                        "right edge; lit from above with a dark lower rim, measured) holding the up and down arrow triangles of "
                        "the small spinner arrows (apex (20, 152) / (20.5, 170), bases 23 px wide on y 160 / 163; cream lit from "
                        "the upper left to tan, a white light line on the up one's left side), and a white oval at its right end",
                        z=10, unsure="what this cell is (roster: 'Unclear: lens with arrows'); the white oval at the right end "
                        "reads as the start of a white field drawn next to the lens")


def small_arrows():
    cells = []
    # the up plate, read as each row's opaque extent: a pointed dome (x 19-22 at y 200 widening to x 4-32 at y 208, a small
    # bump on the right at rows 204-205) on a bar 3 rows tall (y 209-212, x 3-33)
    dome = poly((3.0, 209.4), (4.0, 208.0), (5.0, 207.0), (6.0, 206.0), (7.0, 205.0), (8.0, 204.0), (9.0, 203.0), (12.0, 202.0),
                (15.0, 201.0), (19.0, 200.0), (23.0, 200.0), (25.0, 201.0), (28.0, 202.0), (31.0, 203.0), (34.0, 204.0),
                (35.0, 205.0), (34.0, 206.0), (33.0, 207.0), (33.0, 209.4))
    bar = c.rr(3.0, 209.0, 33.0, 212.0, 0.0)
    def flip(S):     # the down cells are the up ones mirrored (row y <-> 431 - y, checked on the opaque extents)
        if S["t"] == "rrect":
            return dict(S, c=[S["c"][0], 432.0 - S["c"][1]])
        return poly(*[(x, 432.0 - y) for x, y in S["rings"][0]])
    up_t = tri_up(200.8, 209.0)
    edge = lit_edge(200.8, 209.0)
    for st, y0, dy, flipped in (("up", 200, 0, False), ("down", 220, 0, True), ("up 2", 250, 50, False), ("down 2", 270, 50, True)):
        box = (0, y0, 35, y0 + 12)
        D, B, T = (flip(dome), flip(bar), flip(up_t)) if flipped else (dome, bar, up_t)
        E = edge if not flipped else {"t": "stroke", "pts": [[19.9, 230.2], [10.4, 222.6]], "w": 1.7, "cap": "butt"}
        D, B, T, E = mv(D, 0, dy), mv(B, 0, dy), mv(T, 0, dy), mv(E, 0, dy)
        parts = [(D, {"linear": {"n": 3}}), (B, {"linear": {"n": 3, "dir": (0.0, 1.0)}}), (T, {"linear": {"n": 3}}),
                 (E, [250, 248, 238], {"clip": T, "alpha": 0.6})]
        cells.append((st, box, auto.measured_item(TP, box, parts, name=f"trans-small-arrows {st}")))
    return auto.deliver("trans-small-arrows", TP, cells, how="a lilac plate (a pointed dome on a 3-row bar, read as each row's "
                        "extent) holding a triangle (apex (20.5, 200.8), base 23 px wide) lit from the upper left: cream to tan "
                        "in the first pair, orange in the second, a white light line along its left side; the down cells are the "
                        "up ones mirrored (their row extents match exactly); paints are measured linear gradients", z=8,
                        unsure="the white light line of the down arrows was placed by mirroring, check it")


def arrow_glyph():
    box = (446, 348, 486, 374)
    # tip (450.0, 357.8), head base x 458.8 (head y 350.5-365.1), shaft y 354.0-362.0 to a flat end at x 468.0
    S = poly((450.0, 357.8), (458.8, 350.5), (458.8, 354.0), (468.0, 354.0), (468.0, 362.0), (458.8, 362.0), (458.8, 365.1))
    it = auto.measured_item(TP, box, [(S, "flat")], ground=BLACK, name="popup-arrow-glyph")
    return auto.deliver("popup-arrow-glyph", TP, [("idle", box, it)], ground=BLACK, how="a navy left arrow on the black "
                        "ground: one polygon, triangular head (tip x 450, base x 458.8, 14.6 tall) and a straight shaft 8 tall "
                        "with a flat end at x 468", z=8)


FNS = {"popup-check-box": check_box, "trans-spinner": spinner, "trans-lens": lens, "trans-small-arrows": small_arrows,
       "popup-arrow-glyph": arrow_glyph}

if __name__ == "__main__":
    todo = sys.argv[1:] or list(FNS)
    for k, eid in enumerate(todo):
        print(f"[{k + 1}/{len(todo)}] {eid} {FNS[eid]()}", flush=True)
