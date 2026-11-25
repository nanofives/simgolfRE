"""Member Roster small marks (memberRoster_buttons.pcx cells 19 x 18, and the legend row of memberRoster.pcx), read
2026-11-24 from pixel dumps and edge-point circle fits:
  balls: a lit sphere (circle; paint by depth and edge direction solved per colour over every instance): navy, silver,
    gold, red (cells r ~6.1), lavender / silver / gold (the three larger r ~7.5 at the strip's end), the legend's r ~7.3.
  camera: a navy body (x 128..143, y 6.5..16.6) with a hump on top (a trapezoid x 131..139.5 at its foot, 134..138 at
    y 2) and a small knob at the top right, a white lens ring (centre (136, 11), r 3.6) round a navy centre (r 2.0).
  heart: two round lobes (centres (151.1, 7.2) and (159.0, 7.2), r 4.1) over a triangle down to the tip (155.5, 17.2).
  cells: the roster grid's cells (cream row 1, tan row 2: the same lit paints as the grid, roster.py).
  legend: each mark joined by a short wire (y 540.7) to an outlined lavender pill (line y 532..550, r 9).
"""
import json
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import common as C      # noqa: E402
import lit              # noqa: E402
import parts as P       # noqa: E402

SB = "memberRoster_buttons.pcx"
NAVY = [36, 47, 84]
LINE = [100, 101, 145]
STYLE_FILE = os.path.join(C.OUT, "read", "roster_marks_style.json")
# (sheet, colour, centre, r)
BALLS = [(SB, "navy", (55.22, 10.24), 6.14), (SB, "navy", (54.95, 29.27), 5.94),
         (SB, "silver", (75.2, 10.3), 6.1), (SB, "silver", (74.95, 29.3), 6.0),
         (SB, "gold", (95.22, 10.38), 5.99), (SB, "gold", (95.21, 29.37), 5.91),
         (SB, "red", (115.19, 10.29), 6.29), (SB, "red", (115.37, 29.52), 5.99),
         (SB, "lavender", (212.44, 11.02), 6.26), (SB, "silver2", (232.51, 11.1), 6.12), (SB, "gold2", (252.36, 10.75), 6.41),
         ("memberRoster.pcx", "navy", (27.32, 541.27), 7.7), ("memberRoster.pcx", "silver", (149.27, 541.16), 7.07),
         ("memberRoster.pcx", "gold", (271.29, 541.26), 7.14), ("memberRoster.pcx", "red", (393.29, 541.18), 7.42)]
LEG_PILLS = [(42, 128), (164, 250), (286, 372), (408, 494), (530, 616), (652, 732)]
CAM_OFF = (379.54, 530.15)       # legend camera = cell camera + this (dark-centroid match, see legend_offsets)
HEART_OFF = (481.08, 530.11)


def camera(dx=0.0, dy=0.0):
    m = lambda S: {"t": "move", "of": S, "d": [dx, dy]}    # noqa: E731
    body = {"t": "union", "of": [P.rr(128.0, 6.5, 143.0, 16.6, 0.6),
                                 {"t": "poly", "rings": [[[134.0, 2.0], [138.0, 2.0], [139.5, 6.8], [131.0, 6.8]]]},
                                 P.rr(141.6, 4.8, 143.6, 6.8, 0.4)]}
    lens = {"t": "circle", "c": [136.0, 11.0], "r": 3.6}
    return [{"shape": m(body), "fill": NAVY}, {"shape": m(lens), "fill": [235, 232, 222]},
            {"shape": m({"t": "circle", "c": [136.0, 11.0], "r": 2.0}), "fill": NAVY}]


def heart(dx=0.0, dy=0.0):
    S = {"t": "union", "of": [{"t": "circle", "c": [151.1, 7.2], "r": 4.1}, {"t": "circle", "c": [159.0, 7.2], "r": 4.1},
                              {"t": "poly", "rings": [[[147.2, 8.2], [162.9, 8.2], [155.5, 17.2]]]}]}
    return [{"shape": {"t": "move", "of": S, "d": [dx, dy]}, "fill": NAVY}]


def ball_shape(c, r):
    return {"t": "circle", "c": list(c), "r": r}


def solve():
    groups = {}
    for sh, col, c, r in BALLS:
        rgb, m = C.sheet(sh)
        box = (max(int(c[0] - r - 2), 0), max(int(c[1] - r - 2), 0), int(c[0] + r + 3), int(c[1] + r + 3))
        # solved on the ball shrunk by 0.8 px: its rim pixels are blends with the cell and painted the ball's edge grey
        groups.setdefault(col, []).append((ball_shape(c, r - 0.8), rgb, box, None))
    st = {}
    for col, lst in groups.items():
        s = lit.new_style(ow=0.0, L=6.0, out_w=0.0)
        lit.solve(lst, s, dir_in=0.5)
        st[col] = s
    # the legend pills: an outline only
    json.dump(st, open(STYLE_FILE, "w"))
    return st


def ball_item(c, r, st, col):
    # the soft shadow it casts down-right on the cell (read: about 1.5 px of grey below and right of the ball)
    sh = {"shape": {"t": "circle", "c": [c[0] + 0.8, c[1] + 1.0], "r": r + 0.2}, "fill": [90, 85, 80], "alpha": 0.35, "soft": 1.2}
    return {"name": f"{col} ball", "box": [c[0] - r - 3, c[1] - r - 3, c[0] + r + 4, c[1] + r + 4],
            "layers": [sh] + lit.layers(ball_shape(c, r), st[col])}


CELL_FILE = os.path.join(C.OUT, "read", "roster_iconcell_style.json")


def solve_cells():
    """The icon cells' own paint (lighter than the grid's, a soft edge), solved on the cells with their marks left out."""
    rgb, m = C.sheet(SB)
    out = {}
    for tan, y in ((False, 1), (True, 20)):
        lst = []
        for x in (46, 66, 86, 106, 126, 146):
            sub = rgb[y:y + 18, x:x + 19]
            mark = np.abs(sub - np.median(sub.reshape(-1, 3), axis=0)).max(2) > 30     # the mark and its shadow
            mk = mark.copy()
            for dj, di in ((1, 0), (-1, 0), (0, 1), (0, -1), (1, 1), (-1, -1), (1, -1), (-1, 1)):
                mk |= np.roll(np.roll(mark, dj, 0), di, 1)
            keep = ~mk
            lst.append((P.rr(x, y, x + 19, y + 18), rgb, (x, y, x + 19, y + 18), keep))
        st = lit.new_style(ow=0.0, L=3.0, out_w=0.0)
        lit.solve(lst, st, dir_in=0.5)
        out["tan" if tan else "cream"] = st
    json.dump(out, open(CELL_FILE, "w"))
    return out


# The icon cells, read off their pixel columns (the paint solved by depth on 19 x 18 px made up a grey middle): a flat fill,
# three rows of shade under the top edge, two columns of shade at the left edge.
CELL = {False: {"fill": [252, 248, 241], "top": [[182, 179, 173], [195, 192, 185], [220, 216, 206]], "left": [229, 226, 217]},
        True: {"fill": [232, 226, 209], "top": [[159, 153, 140], [168, 163, 151], [190, 184, 172]], "left": [204, 197, 182]}}


def cell_item(x, y, tan):
    c = CELL[tan]
    L = [{"shape": P.rr(x, y, x + 19, y + 18), "fill": c["fill"]}, {"shape": P.rr(x, y, x + 2, y + 18), "fill": c["left"]}]
    for k, col in enumerate(c["top"]):
        L.append({"shape": P.rr(x, y + k, x + 19, y + k + 1), "fill": col})
    return {"name": "cell", "box": [x - 1, y - 1, x + 20, y + 19], "layers": L}


def legend_scene():
    st = json.load(open(STYLE_FILE))
    out = []
    for (x0, x1), bx in zip(LEG_PILLS, (27.32, 149.27, 271.29, 393.29, 515.5, 636.6)):
        out.append({"name": "wire", "box": [bx, 536, x0 + 1, 546],
                    "layers": [{"shape": {"t": "stroke", "pts": [[bx, 540.7], [x0 + 0.5, 540.7]], "w": 1.6}, "fill": LINE}]})
        out.append({"name": "legend pill", "box": [x0 - 4, 528, x1 + 5, 554],
                    "layers": [{"shape": {"t": "outline", "of": P.rr(x0 - 1.0, 532.0, x1 + 1.5, 550.0, 9.0), "w": 1.8},
                                "fill": LINE}]})
    for sh, col, c, r in BALLS[-4:]:
        out.append(ball_item(c, r, st, col))
    out.append({"name": "camera", "box": [500, 528, 530, 552], "layers": camera(*CAM_OFF)})
    out.append({"name": "heart", "box": [625, 528, 650, 552], "layers": heart(*HEART_OFF)})
    return out


def legend_offsets():
    """The legend camera / heart's offset from the cell ones: the dark pixels' centroids."""
    a, ma = C.sheet(SB)
    b, mb = C.sheet("memberRoster.pcx")
    out = []
    for (cx0, cy0), (lx0, ly0) in (((126, 1), (500, 528)), ((146, 1), (624, 528))):
        A = C.fit.lum(a[cy0:cy0 + 18, cx0:cx0 + 19]) < 90
        B = C.fit.lum(b[ly0:ly0 + 24, lx0:lx0 + 26]) < 90
        ja, ia = np.nonzero(A)
        jb, ib = np.nonzero(B)
        out.append((round(float(ib.mean() + lx0 - ia.mean() - cx0), 2), round(float(jb.mean() + ly0 - ja.mean() - cy0), 2)))
    return out


if __name__ == "__main__":
    print("legend offsets", legend_offsets())
    solve()
    solve_cells()
