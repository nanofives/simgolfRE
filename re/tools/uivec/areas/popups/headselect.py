"""P41 popups area: the Head select tray and its two arrow tabs (HeadSelect.pcx).

A tab is a curved slate lens (two circle arcs meeting at its ends, fitted on its edge rows) on the tray's lilac, lit by the
measured bevel paint (its dark left wall and dark right rim come out of the measurement), holding an arrow: a triangle with a
slightly hollow back, read as each row's run of light pixels; the left tab also carries the light lilac bar of the window's
bottom edge. The tray is the lilac frame round a recessed slate window whose sides are arcs, its top edge in shadow, its
bottom and right edges lit, with the two tabs (drawn as above) at the places the sheet's cells match (left (10, 425), right
(707, 284)); the golf ball above the tray is art (head-select-ball) and is not part of it.

Usage: py -3.12 headselect.py [id ...]
"""
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import auto             # noqa: E402
import common as c      # noqa: E402
from auto import mv, poly  # noqa: E402

HS = "HeadSelect.pcx"


def fit_lens(cell, side):
    """The lens' two arcs from the cell's rows: the edge between the tray lilac (blue clearly above red) and the slate, and the
    lens' far edge (its darkest column near the cell's far side). side 'left': lens opens to the right (left tab)."""
    rgb, m, _ = c.sheet(HS)
    L = c.fit.lum(rgb)
    x0, y0, x1, y1 = cell
    near, far = [], []
    for y in range(y0 + 4, y1 - 10, 3):
        row = rgb[y, x0:x1]
        ok = m[y, x0:x1]
        slate = ((row[:, 2] - row[:, 0]) < 45) & ok
        xs = np.nonzero(slate)[0]
        if len(xs) < 5:
            continue
        if side == "left":
            near.append((x0 + xs[0], y + 0.5))
            seg = L[y, x0 + xs[0] + 20:x1]
            if len(seg) and xs[-1] < (x1 - x0) - 2:
                far.append((x0 + xs[-1] + 1.0, y + 0.5))
            elif len(seg):
                far.append((x0 + xs[0] + 20 + int(np.argmin(seg)) + 1.0, y + 0.5))
        else:
            near.append((x0 + xs[-1] + 1.0, y + 0.5))
    return np.array(near), np.array(far)


def tab_items(cell, side, state, place=None):
    """The tab of `cell` (its own measured paints), drawn at `place` (default: where the cell is)."""
    x0, y0, x1, y1 = cell
    if side == "left":
        # left arc through the lilac / slate edge (fitted below), right arc through the lens' far edge; arrow and light bar read
        ox, oy = x0 - 432, y0 - 105
        A = c.circ(LEFT_ARCS[0][0] + ox, LEFT_ARCS[0][1] + oy, LEFT_ARCS[0][2])
        B = c.circ(LEFT_ARCS[1][0] + ox, LEFT_ARCS[1][1] + oy, LEFT_ARCS[1][2])
        lens = {"t": "inter", "of": [A, B, c.rr(x0 - 5, y0 + 4.5, x1 + 5, y0 + 139.0, 0.0)]}
        dx = x0 - 432
        arrow = poly((458.0 + dx, 173.5 + y0 - 105), (486.5 + dx, 159.0 + y0 - 105), (485.0 + dx, 163.5 + y0 - 105),
                     (484.0 + dx, 173.5 + y0 - 105), (485.0 + dx, 183.5 + y0 - 105), (487.0 + dx, 189.0 + y0 - 105))
        bar = c.rr(x0 + 8, y0 + 139.0, x1 + 2, y0 + 147.5, 0.0)
        parts = [(c.rr(x0 - 1, y0 - 1, x1 + 1, y1 + 1, 0.0), "flat"),
                 (bar, {"linear": {"n": 4, "dir": (0.0, 1.0)}}),
                 (lens, {"bevel": {"stops": [0.5, 1.5, 3.0, 5.0, 7.5, 10.0, 14.0, 20.0]}}),
                 (arrow, "flat")]
    else:
        # read on the face map of the lit cell (lilac frame / light band / window / lens per pixel): the lens' left side is a
        # circle arc (c (694.2, 74.2), r 73.6, fitted on 20 rows), its right side an arc bulging right (c (433.7, 134.5),
        # r 258.9, 15 rows), its top cut by the frame's top edge at y 10.5; right of it a light lilac band whose right edge is
        # another arc (c (350.1, 173.5), r 351.6); left of the lens the window's slate; the cell itself ends in a curve at the
        # bottom left (rows 117-152: x 614, 622, 628, 635, 643, 650, 669)
        ox, oy = x0 - 614, y0
        def C(cx, cy, r):
            return c.circ(cx + ox, cy + oy, r)
        cellP = poly((614 + ox, -1 + oy), (708 + ox, -1 + oy), (708 + ox, 153 + oy), (669 + ox, 153 + oy), (650 + ox, 144.5 + oy),
                     (643 + ox, 138.5 + oy), (635 + ox, 132.5 + oy), (628 + ox, 126.5 + oy), (622 + ox, 120.5 + oy),
                     (614 + ox, 117.0 + oy), (613 + ox, 117.0 + oy), (613 + ox, -1 + oy))
        below = c.rr(x0 - 5, y0 + 10.5, x1 + 5, y1 + 5, 0.0)
        band = {"t": "inter", "of": [cellP, below, C(350.14, 173.46, 351.64)]}
        window = {"t": "inter", "of": [cellP, below, C(433.74, 134.54, 258.9)]}
        lens = {"t": "inter", "of": [C(694.21, 74.21, 73.6), C(433.74, 134.54, 258.9), below]}
        dx = x0 - 614
        arrow = poly((674.0 + dx, 79.5 + y0), (645.8 + dx, 65.5 + y0), (647.2 + dx, 70.0 + y0), (648.0 + dx, 79.5 + y0),
                     (647.2 + dx, 89.0 + y0), (645.0 + dx, 94.8 + y0))
        parts = [(cellP, "flat"),
                 (band, {"linear": {"n": 3, "dir": (1.0, 0.0)}}),
                 (window, "flat"),
                 ({"t": "stroke", "pts": [[613.0 + ox, 9.4 + oy], [663.0 + ox, 9.4 + oy]], "w": 1.8}, "flat"),
                 (lens, {"bevel": {"stops": [0.5, 1.5, 3.0, 5.0, 7.5, 10.0, 14.0, 20.0]}}),
                 (arrow, "flat")]
    it = auto.measured_item(HS, cell, parts, name=f"head-select-{side} {state}")
    if place is not None:
        ddx, ddy = place[0] - x0, place[1] - y0
        for Ld in it["layers"]:
            Ld["shape"] = mv(Ld["shape"], ddx, ddy)
            if isinstance(Ld["fill"], dict) and "linear" in Ld["fill"]:
                for k in ("a", "b"):
                    Ld["fill"]["linear"][k] = [Ld["fill"]["linear"][k][0] + ddx, Ld["fill"]["linear"][k][1] + ddy]
        it["box"] = [place[0], place[1], place[0] + x1 - x0, place[1] + y1 - y0]
    return it


LEFT_CELLS = {"lit": (432, 105, 519, 257), "dim": (519, 105, 606, 257)}
RIGHT_CELLS = {"lit": (614, 0, 707, 152), "dim": (707, 0, 800, 152)}


def arcs():
    global LEFT_ARCS, RIGHT_ARCS
    near, far = fit_lens(LEFT_CELLS["lit"], "left")
    a = c.fit.fit_circle(near, trim=1.5)
    b = c.fit.fit_circle(far, trim=2.0)
    LEFT_ARCS = ((a[0], a[1], a[2]), (b[0], b[1], b[2]))
    near, _ = fit_lens(RIGHT_CELLS["lit"], "right")
    a = c.fit.fit_circle(near, trim=1.5)
    # the right tab's lower left edge (rows 120-144: x 622, 630, 640, 650) is the inside of a second circle
    pts = np.array([(622.0, 120.5), (630.0, 128.5), (640.0, 136.5), (650.0, 144.5), (614.0, 113.5)])
    b = c.fit.fit_circle(pts, trim=3.0)
    RIGHT_ARCS = ((a[0], a[1], a[2]), (b[0], b[1], b[2]))
    print("arcs", [tuple(round(v, 1) for v in q) for q in LEFT_ARCS], [tuple(round(v, 1) for v in q) for q in RIGHT_ARCS])


LEFT_ARCS = RIGHT_ARCS = None


def head_left():
    states = [(st, cell, tab_items(cell, "left", st)) for st, cell in LEFT_CELLS.items()]
    return auto.deliver("head-select-left", HS, states, how="a curved slate lens (two fitted circle arcs) on the tray lilac, its "
                        "dark left wall and dark right rim measured as one bevel paint, a cream left arrow (dim: a dark one) with "
                        "a slightly hollow back read as each row's light run, and the window's light bottom bar", z=3,
                        unsure="the lens' far arc leaves the cell at its right side; fitted on the rows where it is inside")


def head_right():
    states = [(st, cell, tab_items(cell, "right", st)) for st, cell in RIGHT_CELLS.items()]
    return auto.deliver("head-select-right", HS, states, how="the left tab turned round: a slate lens whose right arc was fitted "
                        "on its edge rows and whose lower left edge is a second circle, lit by a measured bevel paint, a white "
                        "right arrow (dim: dark) read as each row's light run, and the window's light right bar", z=3)


def lens_parts(side, ox, oy):
    """The lens and arrow of a tab (lit cell geometry) moved by (ox, oy)."""
    if side == "left":
        A, B = LEFT_ARCS
        lens = {"t": "inter", "of": [c.circ(A[0] + ox, A[1] + oy, A[2]), c.circ(B[0] + ox, B[1] + oy, B[2]),
                                     c.rr(427 + ox, 109.5 + oy, 525 + ox, 244.0 + oy, 0.0)]}
        arrow = poly((458.0 + ox, 173.5 + oy), (486.5 + ox, 159.0 + oy), (485.0 + ox, 163.5 + oy), (484.0 + ox, 173.5 + oy),
                     (485.0 + ox, 183.5 + oy), (487.0 + ox, 189.0 + oy))
    else:
        lens = {"t": "inter", "of": [c.circ(694.21 + ox, 74.21 + oy, 73.6), c.circ(433.74 + ox, 134.54 + oy, 258.9),
                                     c.rr(600 + ox, 10.5 + oy, 720 + ox, 160 + oy, 0.0)]}
        arrow = poly((674.0 + ox, 79.5 + oy), (645.8 + ox, 65.5 + oy), (647.2 + ox, 70.0 + oy), (648.0 + ox, 79.5 + oy),
                     (647.2 + ox, 89.0 + oy), (645.0 + ox, 94.8 + oy))
    return lens, arrow


def head_tray():
    box = (0, 258, 800, 600)
    # the window, read as each row's slate extent on the tray: top y 289 (a dark shadow row 289-292), bottom y 564 (a lit row
    # 564-569 under it), its left side an arc (c (300.4, 424.8), r 293.1, 12 rows) and its right side an arc (c (513.1, 431.6),
    # r 281.2, 12 rows)
    left_ok = {"t": "union", "of": [c.circ(300.44, 424.75, 293.12), {"t": "half", "p": [300.44, 0.0], "n": [-1.0, 0.0]}]}
    right_ok = {"t": "union", "of": [c.circ(513.14, 431.62, 281.23), {"t": "half", "p": [513.14, 0.0], "n": [1.0, 0.0]}]}
    win = {"t": "inter", "of": [c.rr(0, 289.0, 800, 564.0, 0.0), left_ok, right_ok]}
    rim = {"t": "grow", "of": win, "d": 5.0}
    ll, la = lens_parts("left", 10 - 432, 425 - 105)
    rl, ra = lens_parts("right", 707 - 614, 284 - 0)
    parts = [(c.rr(-1, 257, 801, 601, 0.0), "flat"),
             (rim, {"bevel": {"stops": [0.5, 1.5, 2.5, 3.5, 4.5]}}),
             (win, {"bevel": {"stops": [0.5, 1.5, 2.5, 4.0, 6.0, 9.0, 13.0, 20.0]}}),
             (ll, {"bevel": {"stops": [0.5, 1.5, 3.0, 5.0, 7.5, 10.0, 14.0, 20.0]}}), (la, "flat"),
             (rl, {"bevel": {"stops": [0.5, 1.5, 3.0, 5.0, 7.5, 10.0, 14.0, 20.0]}}), (ra, "flat")]
    it = auto.measured_item(HS, box, parts, name="head-select-tray")
    sc = [it]
    mad, w16 = c.score(sc, HS, box, s=2)
    p = c.compare([(sc, HS, box, c.FLAT)], "head-select-tray", zoom_k=1)
    c.save_scene("head-select-tray", {"id": "head-select-tray", "sheet": "Interface/" + HS, "states": {"idle": {"box": list(box), "scene": sc}}})
    c.record("head-select-tray", "done", "the lilac tray round a recessed slate window (top y 289, bottom y 564, both sides "
             "circle arcs fitted on 12 rows each); a 5 px rim round the window (dark shadow on top, lit below and right) and the "
             "window's own shading measured as bevel paints; the two dim arrow tabs inside it (lens and arrow as in "
             "head-select-left / -right, at the places their cells match: (10, 425) and (707, 284)), their paints measured on "
             "the tray", mad, None, "the dim arrows in the tray are a brownish orange, unlike the grey dim cells; measured here")
    return p


FNS = {"head-select-left": head_left, "head-select-right": head_right, "head-select-tray": head_tray}

if __name__ == "__main__":
    arcs()
    todo = sys.argv[1:] or list(FNS)
    for k, eid in enumerate(todo):
        print(f"[{k + 1}/{len(todo)}] {eid} {FNS[eid]()}", flush=True)
