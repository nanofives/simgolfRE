"""P41 hud_panels: the three HUD trays (Member list, Golfer action, Employee), 585 x 126 each.

A tray is composed, not traced: a lavender plate (top edge y 494, its left side the curved lobe under the three tab
bubbles, x 225 below y 558; read off the silhouette's row extents) lit as one measured surface (el_slabs.fit_bevel), its
recessed areas (rounded boxes whose own inner shading is measured the same way), and the buttons drawn on it. Each button
is the element this area already redrew, placed where its cell's art sits in the tray (kit.find_cell: the masked best
match of the cell over the tray; the tray shows the club / crossed / trophy and hire / fire / details cells in their hover
state). The one tab per tray that is drawn yellow (the selected tab) has no cell of its own: it is drawn with the blue
tab's recipe and glyph, its rim and body measured in place, its paint measured on it.
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
import el_blue as BL  # noqa: E402
import el_slabs as S  # noqa: E402
import el_marks as M  # noqa: E402
from kit import U, line, rect, circle, inter, sub  # noqa: E402,F401

REGION = {"M": (215, 474, 800, 600), "J": (214, 474, 800, 600), "E": (215, 474, 800, 600)}

# (element, state, whole scene or just its bubble item); placed by find_cell
PLACED = {
    "M": [("tab-golfer", "idle", "all"), ("tab-golfer-staff", "idle", "all"), ("member-page-left", "hover", "all"),
          ("member-page-right", "hover", "all")],
    "J": [("tab-golfers-group", "idle", "all"), ("joecool-tab-staff", "idle", "all"), ("golfer-club-button", "hover", "all"),
          ("golfer-crossed-button", "hover", "all"), ("golfer-trophy-button", "hover", "all"),
          ("joecool-stats-button", "idle", "bubble")],
    "E": [("tab-staff-group", "idle", "all"), ("employee-tab-golfer", "idle", "all"), ("employee-hire", "hover", "all"),
          ("employee-fire", "hover", "all"), ("employee-resume", "hover", "all"), ("employee-thumbs-up", "idle", "bubble")],
}
# the selected (yellow) tab: (top-left of its 33 px cell in the tray, the glyph, the element whose glyph it is)
YELLOW = {"M": ((270, 476), BL.glyph_group, "tab-golfers-group"), "J": ((240, 495), BL.glyph_golfer, "tab-golfer"),
          "E": ((217, 523), BL.glyph_staff, "tab-golfer-staff")}
PEDESTALS = [(336 + 63 * i, 507, "idle") for i in range(4)] + [(336 + 63 * i, 550, "idle 2") for i in range(4)]


def body():
    """The tray plate: the long plate (top y 494) and the lobe on its left (x 225 below the tabs); it runs on past the
    cell's right and bottom ends, so only its top and left are lit edges."""
    return U(rect(296.0, 494.0, 840.0, 650.0, 10.0), rect(250.0, 505.0, 330.0, 650.0, 14.0),
             rect(225.0, 540.0, 330.0, 650.0, 12.0))


def recesses(key):
    """The recessed boxes of each tray (outer extents read on the rows / columns printed 2026-11-24)."""
    if key == "M":
        return [rect(307.0, 496.0, 797.0, 588.0, 2.0), rect(307.0, 588.0, 797.0, 599.0, 1.0)]
    if key == "J":
        # a raised inner panel first (top edge y 506, left edge x 332..335, corner r 8), the three boxes on it
        return [rect(331.5, 505.5, 840.0, 650.0, 8.0), rect(336.0, 535.0, 482.0, 600.0, 3.0), rect(482.0, 535.0, 633.0, 600.0, 3.0), rect(633.0, 535.0, 797.0, 600.0, 3.0)]
    return [rect(332.0, 497.0, 594.0, 600.0, 3.0), rect(619.0, 523.0, 798.0, 598.0, 4.0)]


def placed_items(key):
    out, covers = [], []
    for el, st, how in PLACED[key]:
        cell = [c for c in C.CELLS[el] if c[0] == st][0]
        dx, dy, mad = K.find_cell(key, cell[2], REGION[key])
        sc = EB.BUILD[el](el)[st]
        items = sc if how == "all" else sc[-1:]
        for it in items:
            moved = K.shift(it, int(dx), int(dy))
            out.append(moved)
        x, y, w, h = cell[2]
        covers.append(rect(x + dx, y + dy, x + dx + w, y + dy + h))
    if key == "E":
        for x, y, st in PEDESTALS:
            cell = [c for c in C.CELLS["employee-pedestal"] if c[0] == st][0]
            sc = EB.BUILD["employee-pedestal"]("employee-pedestal")[st]
            out += [K.shift(it, x - cell[2][0], y - cell[2][1]) for it in sc]
    return out, covers


def yellow_tab(key):
    (x, y), gl, ref = YELLOW[key]
    rct = (x, y, 33, 33)
    c, body_ = BL.measure_blue(key, rct)
    rg = EB.GEOM[ref]["idle"]
    rg_c, rg_b = BL.measure_blue(rg["key"], tuple(rg["rect"]))
    dx, dy = K.match_offset((rg["key"], rg_b), (key, body_))
    parts = gl()
    shapes = [K.shift(sh, dx, dy) for _, sh, _ in parts]
    navy = K.sample_dark(key, U(*shapes), q=0.35)
    gu = U(*shapes)
    shade = B.solve_family([B.body_rows(key, c, body_, trim_glyph=gu)])
    rim = K.sample_dark(key, K.outline(circle(c[0], c[1], (c[2] + body_[2]) / 2), c[2] - body_[2]), q=0.5, grow=0.0)
    ground, _ = BL.halo(key, rct, c)
    it = B.item(c, body_, {"shade": shade, "rim": rim}, glyph=[{"shape": s, "fill": navy} for s in shapes])
    return [K.item(ground, box=[x, y, x + 33, y + 33]), it], rect(x, y, x + 33, y + 33)


def build_tray(el):
    st, key, rct = C.CELLS[el][0]
    x0, y0, w, h = rct
    cell = rect(x0, y0, x0 + w, y0 + h)
    bd = body()
    plate = inter(bd, cell)
    rec = recesses(key)
    items, covers = placed_items(key)
    ytab, ycover = yellow_tab(key)
    covers.append(ycover)
    excl = U(*(rec + covers))
    L = [K.lay(plate, S.fit_bevel(key, plate, [x0, y0, x0 + w, y0 + h], exclude=excl, L=5.0, of=bd))]
    stub = rect(214.0 if key == "J" else 215.0, 588.0, 226.0, 600.0, 1.0)
    L.append(K.lay(stub, K.sample(key, rect(216, 592, 222, 598), depth=0.0)))
    for i, r in enumerate(rec):
        hx, hy = r["half"]
        ex = U(*(covers + rec[i + 1:])) if (covers or rec[i + 1:]) else None
        box = [max(r["c"][0] - hx, x0), max(r["c"][1] - hy, y0), min(r["c"][0] + hx, x0 + w), min(r["c"][1] + hy, y0 + h)]
        rc = inter(r, cell)
        L.append(K.lay(rc, S.fit_bevel(key, rc, [int(box[0]), int(box[1]), int(np.ceil(box[2])), int(np.ceil(box[3]))],
                                       exclude=ex, L=4.0, of=r)))
    scene = [K.item(L, box=[x0, y0, x0 + w, y0 + h])] + items + ytab
    return {st: scene}


EB.BUILD.update({"member-panel": build_tray, "golfer-action-panel": build_tray, "employee-panel": build_tray})
