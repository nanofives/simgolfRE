import json, os, sys
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import common as C, deliver as D, roster as RO, roster_marks as RM

FL = np.array([70., 110, 90])
rs = RO.load_styles(); ms = json.load(open(RM.STYLE_FILE))


def cell_art(x, y, w, h):
    rgb, m = C.sheet(RM.SB)
    a = rgb[y:y + h, x:x + w].copy(); a[~m[y:y + h, x:x + w]] = FL
    b = np.empty_like(a); b[:] = FL
    return a, b


def cells(eid, lst, how, unsure=""):
    states = []
    for label, (x, y), tan, marks in lst:
        a, b = cell_art(x - 1, y - 1, 21, 20)
        states.append((label, a, b, [RM.cell_item(x, y, tan)] + marks, (x - 1, y - 1)))
    D.deliver(eid, states, how, unsure=unsure, z=15)


BI = {(c, round(cc[1])): (cc, r) for sh, c, cc, r in RM.BALLS if sh == RM.SB}
for k, (col, x) in enumerate((("navy", 46), ("silver", 66), ("gold", 86), ("red", 106))):
    lst = []
    for label, y, tan in (("row 1", 1, False), ("row 2", 20, True)):
        cc, r = [v for (c, yy), v in BI.items() if c == col and abs(yy - (y + 9)) < 3][0]
        lst.append((label, (x, y), tan, [RM.ball_item(cc, r, ms, col)]))
    cells(f"roster-icon-{col}", lst, f"a cream (row 1) / tan (row 2) roster cell (flat fill, three rows of shade under the top, two columns at the left: read off the cell's pixel columns) with a {col} sphere: a circle "
          f"(edge-point fit, residual under 0.3 px) lit by its own paint solved on its four instances (cells and legend)")
cells("roster-icon-camera", [("row 1", (126, 1), False, [{"name": "camera", "box": [126, 1, 145, 19], "layers": RM.camera()}]),
                             ("row 2", (126, 20), True, [{"name": "camera", "box": [126, 20, 145, 38], "layers": RM.camera(0, 19)}])],
      "a navy camera from its parts: a body box, a hump on top (trapezoid), a small knob at the top right, a white lens ring "
      "round a navy centre; on the roster cell (cream / tan)", unsure="the knob is 2 px: check it reads as the art's")
cells("roster-icon-heart", [("row 1", (146, 1), False, [{"name": "heart", "box": [146, 1, 165, 19], "layers": RM.heart()}]),
                            ("row 2", (146, 20), True, [{"name": "heart", "box": [146, 20, 165, 38], "layers": RM.heart(0, 19)}])],
      "a navy heart: two round lobes over a triangle to the tip; on the roster cell (cream / tan)")
lst = []
for label, col, x in (("lavender", "lavender", 204), ("silver", "silver2", 224), ("gold", "gold2", 244)):
    cc, r = [(c2, r2) for sh, c, c2, r2 in RM.BALLS if c == col][0]
    a, b = cell_art(x - 1, 0, 21, 20)
    lst.append((label, a, b, [RM.cell_item(x, 1, False), RM.ball_item(cc, r, ms, col)], (x - 1, 0)))
D.deliver("roster-icon-balls", lst, "lavender, silver and gold spheres (r ~6.3, circle fits on the ball's own edge) on the cream icon cell, each lit by its own measured paint, with its soft shadow",
          unsure="these three are not in the legend; what they mark is unknown (the roster calls them unclear)", z=15)
# legend
rgb, m = C.sheet(RO.SHEET)
x, y, w, h = 14, 528, 726, 26
art = rgb[y:y + h, x:x + w]; base = np.empty_like(art); base[:] = [148, 150, 198]
fb = np.empty_like(rgb); fb[:] = [148, 150, 198]
sc = RM.legend_scene()
det = [D.tile(rgb[b_:b_ + 26, a_:a_ + 130], fb[b_:b_ + 26, a_:a_ + 130], sc, (a_, b_), 6) for a_, b_ in ((14, 528), (490, 528))]
D.deliver("roster-legend", [("idle", art, base, sc, (x, y))],
          "navy, silver, gold and red spheres, the camera and the heart (the cell marks moved to their legend places by the dark "
          "pixels' centroid offset), each joined by a short wire to an outlined lavender pill (line y 532..550, r 9)", extra_tiles=det, z=2)
if '--marks' in sys.argv:
    sys.exit(0)
# grid, slab, plate
art_full = rgb.copy(); art_full[~m] = FL
base_full = np.empty_like(art_full); base_full[:] = FL
sc = RO.scene(rs)
x, y, w, h = 376, 52, 386, 476
gsc = [it for it in sc if it["name"] in ("grid", "grid cell", "tab")]
lav = np.empty((h, w, 3)); lav[:] = [148, 150, 198]
det = [D.tile(rgb[b_:b_ + 50, a_:a_ + 90], np.full((50, 90, 3), [148., 150, 198]), gsc, (a_, b_), 6) for a_, b_ in ((370, 50), (680, 480))]
D.deliver("roster-grid", [("idle", rgb[y:y + h, x:x + w], lav, gsc, (x, y))],
          "2 px navy rules at x 380 + 21 k and y 84 + 20 j; 18 x 22 cells of 19 x 18 px, the columns alternately tan and cream, "
          "each with an inner shade (lit paint solved over all cells of its colour); 18 lilac tabs above (rounded squares in a "
          "navy line, alternately deeper and paler)", extra_tiles=det, z=1)
det = [D.tile(art_full[b_:b_ + hh, a_:a_ + ww], base_full[b_:b_ + hh, a_:a_ + ww], sc, (a_, b_), 3)
       for a_, b_, ww, hh in ((0, 0, 260, 120), (100, 250, 200, 80), (560, 470, 240, 130))]
D.deliver("roster-slab", [("idle", art_full, base_full, sc, (0, 0))],
          "the slab outline read as its parts (shoulder curve, top, step, right wall, foot tab, bottom; corners from the mask), lit "
          "by a solved rim paint; the navy wiring line; the title plate; the name column (navy box, rules, cream cells); four "
          "sunk cream columns; the grid and tabs; header pills; the legend. Full view 1x, details 3x",
          unsure="the slab's rim paint shows faint seams where its outline turns (the shoulder)", extra_tiles=det, z=1)
import plates
plates.deliver_plate("roster-title-plate", RO.SHEET, RO.PLATE_G, (96, 6, 434, 46), how_extra=" (this screen's own arch profile)")
