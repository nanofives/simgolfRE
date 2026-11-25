"""P41 fix round: the small glass bubbles of the editor (CGButtons.pcx / CustGolfBckgrnd.pcx) and the world map
(WorldButton.pcx / WorldBase.pcx), and the world map's Back disc.

Mariano's notes and what was re-read:
- editor-load "the arrow has the point on the right part", world-load "missing arrow on the end of the line": the lid line over
  the folder is an arrow. Read off the dumps: it leaves the folder's tab at the left, arches over and comes down on the right,
  where it widens to 3 px and closes to a point (row 12 in the editor cell, row 428 on the world sheet). Drawn as ONE round-
  capped curve (quadratic sampled densely) plus a triangular head on its end along the curve's end tangent.
- editor-save "floppy disk not quite there", world-save "same as the other floppy disk button": the floppy is a dark square
  whose parts are separated by 1 px LIGHT lines, not separate blocks with 2 px gaps: a light line down each side of the
  shutter, one under it and one over the label, the label split by a light window on its right, a notch at the top right of
  the right side, the bottom corners cut. Drawn as the dark body minus those lines (each read off the dump).
- editor-reset "arrow should be continuous until the end": one line round a circle (centre 520.5, 19.0, radius 11, 3 px)
  from its start at the left (172 deg) over the top and down the right, bending into a straight run along row 26 that runs
  into the arrow head (tip 516.8, 26; base x 524.5 from row 20.5 to 31.5). Drawn as one round-capped path through the arc and
  the straight run, with the head on its end, so the line never breaks before the head.
- world-back "underneath lightning of the arrow is wrong": the disc's lit paint was fitted with the arrow's pixels masked
  out, so under the arrow the fit was free and painted a blue and a yellow blob on the hover disc. Now the arrow is inpainted
  out of the art first and the paint is fitted over the whole disc, so the surface under the arrow follows its surroundings.
  The hover disc is also fitted on its own pixels (it sits 1-2 px up and left of the idle one, as the art draws it) and the
  arrow moves with it by the measured shift.
"""
import copy
import json

import numpy as np

import common as c
import editor_screen as E
import fix2 as F
import parts
from common import fit, sdf

CB = "Interface/CGButtons.pcx"
BG_F = "Interface/CustGolfBckgrnd.pcx"
BASE = "Interface/WorldBase.pcx"
BTN = "Interface/WorldButton.pcx"


def head(p_end, ctrl, length, half_w, back=0.6):
    """A triangular arrow head on the end of a curve: its axis along the end tangent (p_end - ctrl), the base `back` px
    behind the end point, the tip `length` px ahead of the base."""
    dx, dy = p_end[0] - ctrl[0], p_end[1] - ctrl[1]
    n = np.hypot(dx, dy)
    ux, uy = dx / n, dy / n
    bx, by = p_end[0] - ux * back, p_end[1] - uy * back
    tip = [bx + ux * length, by + uy * length]
    return F.poly([bx - uy * half_w, by + ux * half_w], tip, [bx + uy * half_w, by - ux * half_w])


def lid_arrow(p0, p1, p2, w, length, half_w):
    line = F.path([F.quad_pts(p0, p1, p2, 20)], w)
    return F.U(line, head(p2, F.quad_pts(p0, p1, p2, 20)[-3], length, half_w))


# --------------------------------------------------------------------------------------------------------- editor glyphs
def g_load():
    """CGButtons #0: an open folder (the back: tab, top bar, left side; the flap leaning left; a 1 px light gap between
    them) and the lid arrow: from the tab's top up over the folder and down on the right to a point."""
    flap = F.poly((12.6, 14.0), (22.4, 14.0), (20.0, 22.0), (9.3, 22.0))
    back = F.sub(F.U(F.box(9.5, 10.0, 14.2, 11.0), F.box(8.4, 11.0, 19.5, 13.0), F.box(8.0, 13.0, 10.2, 22.0)),
                 {"t": "grow", "of": flap, "d": 0.9})
    arrow = lid_arrow([15.0, 7.6], [19.0, 3.0], [22.5, 9.8], 2.0, 3.6, 1.95)
    return F.U(back, flap, arrow)


def g_save():
    """CGButtons #1: the floppy: dark body x 107.2-121, rows 8.3-22.6, minus the light lines: x 109 beside the shutter (rows 8-14)
    and beside the label (rows 18-22), x 119 beside the shutter, row 14 under the shutter and row 17 over the label, the window
    x 116-118 in the label, the notch at the top of the right side."""
    body = F.U(F.box(107.2, 8.3, 121.0, 20.0, 0.5), F.box(107.2, 10.0, 121.0, 22.6, 1.2))
    lines = F.U(F.box(109.0, 8.0, 110.0, 14.0), F.box(119.0, 8.0, 120.0, 14.0), F.box(109.0, 14.0, 120.0, 15.0),
                F.box(109.0, 17.0, 120.0, 18.0), F.box(109.0, 18.0, 110.0, 23.0), F.box(116.0, 18.0, 118.0, 23.0),
                F.box(120.0, 8.0, 121.5, 9.8))
    return F.sub(body, lines)


def g_reset():
    """CGButtons #3: one arrow round a circle: the arc from the start at the left (172 deg) over the top and down the right
    (to 25 deg), bending into the straight run along row 26 that runs into the head pointing left; the dot at the lower left."""
    cx, cy, r = 520.5, 19.0, 11.0
    arc = F.arc_pts(cx, cy, r, 172.0, 385.0)
    end = arc[-1]
    bend = F.quad_pts(end, [529.6, 26.4], [526.0, 26.6], 8)
    line = F.path([arc, bend, [[524.2, 26.6]]], 3.0)
    tip = F.poly((524.6, 20.6), (516.8, 26.4), (524.6, 31.6))
    return F.U(line, tip, F.circ(511.6, 26.0, 2.9))


# ---------------------------------------------------------------------------------------------------------- world glyphs
def w_folder():
    """The world map's open folder (bubble 596.5, 429.5): back, flap, 1 px light gap, and the lid arrow from the tab over to
    a point at the lower right (603.5, 429)."""
    flap = F.poly((592.0, 430.4), (603.0, 430.4), (599.6, 438.0), (588.6, 438.0))
    back = F.sub(F.U(F.box(589.0, 426.0, 595.0, 427.0), F.box(589.0, 427.0, 599.0, 429.0), F.box(588.0, 429.0, 591.0, 438.0)),
                 {"t": "grow", "of": flap, "d": 0.9})
    arrow = lid_arrow([594.8, 423.6], [598.8, 419.0], [602.4, 425.8], 2.0, 3.6, 1.95)
    return F.U(back, flap, arrow)


def w_floppy():
    """The world map's floppy (bubble 576.5, 487.5): dark body x 566-586, rows 477-497 with the bottom corners cut, minus the
    light lines: x 569 beside the shutter, x 582 beside the shutter and the label, row 485 under the shutter, row 490 over the
    label, x 569-570.6 left of the label, the window x 578-581 (rows 492-497), the notch x 584-585 rows 478-480."""
    body = F.U(F.box(566.0, 477.0, 586.0, 494.0), F.box(566.0, 480.0, 586.0, 497.0, 1.6))
    lines = F.U(F.box(569.0, 476.0, 570.0, 486.0), F.box(582.0, 476.0, 583.0, 486.0), F.box(569.0, 485.0, 583.0, 486.0),
                F.box(569.0, 490.0, 583.0, 491.0), F.box(582.0, 490.0, 583.0, 498.0), F.box(569.0, 491.0, 570.7, 498.0),
                F.box(577.8, 492.0, 580.9, 498.0), F.box(583.9, 478.0, 585.1, 480.0))
    return F.sub(body, lines)


# ---------------------------------------------------------------------------------------------------------------- build
def editor_bubbles():
    cg, _ = c.sheet(CB)
    art, _ = c.sheet(BG_F)
    specs = [("editor-load", (0, 0, 36, 38), g_load(), (20, 0, 80, 50), None,
              "an open folder with the lid arrow: one round-capped curve from the tab over the folder to a triangular head "
              "on its right end", "the lid line now ends in an arrow head pointing down at the right (its end tangent), "
              "drawn as one continuous round-capped curve"),
             ("editor-save", (99, 0, 37, 38), g_save(), (225, 0, 285, 50), None,
              "a floppy disk: one dark square cut by 1 px light lines (beside and under the shutter, over the label), a "
              "light window in the label, a notch at the top right", "re-read: the parts are one dark body split by 1 px "
              "light lines (the first round drew separate blocks with 2 px gaps that read as an H); the label's window and "
              "the notch added"),
             ("editor-reset", (500, 0, 45, 46), g_reset(), (730, 5, 795, 70), (519.6, 20.0, 19.4),
              "one arrow round a circle: a single round-capped 3 px path from the start at the left over the top and down "
              "the right, bending into the straight run along row 26 that runs into the head pointing left; the dot",
              "the arc, the bend and the straight run are one path that runs into the head (the first round's arc stopped "
              "short and the head floated apart, pointing down-left)")]
    for eid, rect, glyph, window, hint, how, changed in specs:
        if not F._only(eid):
            continue
        cx, cy, r, res = E.rim_circle(cg, rect, hint=hint)
        rim_w = 2.2 if r > 20 else 1.3
        it_h = E.bubble(cg, cx, cy, r, glyph, rim_w=rim_w)
        v, dx, dy = E.locate(cg, rect, art, window)
        it_i = E.bubble(art, cx + dx, cy + dy, r, F.mv(glyph, dx, dy), rim_w=rim_w)
        for it, a in ((it_h, cg), (it_i, art)):
            gl = it["layers"][-1]
            gl["fill"] = F.glyph_paint(a, gl["shape"], it["box"], fallback=gl["fill"])
        x, y, w, h = rect
        F.deliver(eid, [{"name": "idle (backdrop, blue)", "art": art, "scene": [it_i], "rect": (x + dx - 2, y + dy - 2, w + 4, h + 4)},
                        {"name": "hover (CGButtons, yellow)", "art": cg, "scene": [it_h], "rect": (max(x - 2, 0), max(y - 2, 0), w + 4, h + 4)}],
                  f"glass bubble in a navy rim (circle fitted, paint measured per state) with {how}; blue where the backdrop "
                  f"draws it ({x + dx}, {y + dy}), yellow on the CGButtons cell", changed, z=8)
        BACKDROP[eid] = it_i


def world_bubbles():
    import load
    btn, _ = c.sheet(BTN)
    widx, wpal = fit.load_sheet(BTN)
    load.setup()
    specs = [("world-save", (0x226, 0x1ca, 0x4c, 0x40), (0x190, 0x1ca), (576.5, 487.5, 17.5), w_floppy(),
              "a floppy disk: one dark square cut by 1 px light lines (beside the shutter, under it, over the label, left of "
              "the label), the label's light window, the notch at the top right, bottom corners cut",
              "same correction as the editor's floppy: one dark body split by 1 px light lines read off the dump, instead of "
              "separate blocks"),
             ("world-load", (0x226, 0x198, 0x4c, 0x32), (0x190, 0x198), (596.5, 429.5, 16.5), w_folder(),
              "an open folder with the lid arrow: one round-capped curve from the tab over to a triangular head at its "
              "lower right end", "the arrow head on the end of the lid line added (along the curve's end tangent); the line "
              "is one round-capped curve")]
    Lb = fit.lum(btn)
    for eid, cell, hcell, hint, glyph, how, changed in specs:
        if not F._only(eid):
            continue
        x, y, w, h = cell
        dx = hcell[0] - x
        it_i, info_i = parts.bubble_button(widx, wpal, btn, cell, hint, glyph, "blue", socket_art=btn)
        di = Lb[y:y + h, x:x + w] < 70
        best = None
        for sy in range(-3, 4):
            for sx in range(-3, 4):
                dh = Lb[y + sy:y + sy + h, hcell[0] + sx:hcell[0] + sx + w] < 70
                if dh.shape != di.shape:
                    continue
                v = (di & dh).sum() / max((di | dh).sum(), 1)
                if best is None or v > best[0]:
                    best = (v, sx, sy)
        it_h = parts.moved_bubble(it_i, btn, info_i, dx + best[1], best[2], "yellow", glyph)
        hc = (hcell[0], hcell[1], w, h)
        F.deliver(eid, [{"name": "idle", "art": btn, "scene": [it_i], "rect": cell},
                        {"name": "hover", "art": btn, "scene": [it_h], "rect": hc}],
                  f"glass bubble in its socket ring (circle fitted to the rim, body and socket measured, Load Game's glass "
                  f"paint) with {how}; hover the same shapes in the yellow paint", changed, z=8)
        BACKDROP[eid] = it_i


def disc_state(art, cell, arrow, ground_thr=10):
    """A matte disc measured on one state's art: circle fitted to the pixels that differ from the plain ground, the arrow
    inpainted out of the art, the lit paint (accept.solve_bevel) fitted over the whole disc, the arrow's colour its median."""
    x, y, w, h = cell
    sub = art[y:y + h, x:x + w]
    ring = np.concatenate([sub[0], sub[-1], sub[:, 0], sub[:, -1]])
    ground = np.median(ring, axis=0)
    m = np.abs(sub - ground).max(axis=2) > ground_thr
    pts = fit.outermost(m, (w / 2, h / 2))
    cx, cy, r, res = fit.fit_circle(pts, trim=0.8)
    cx, cy = cx + x, cy + y
    X, Y = parts.grid(x, y, x + w, y + h)
    am = sdf.sd(arrow, X, Y) < 1.6
    clean = c.inpaint(sub, am)
    disc = F.circ(round(cx, 2), round(cy, 2), round(r, 2))
    dm = sdf.sd(disc, X, Y) < -0.6
    paint = parts.bevel_fit(clean, disc, dm, x, y, r, [0.0, 0.05, 0.12, 0.25, 0.45, 0.7, 1.0])
    g = sub[sdf.sd(arrow, X, Y) < -1.0]
    grgb = [round(float(v), 1) for v in np.median(g, axis=0)]
    item = {"box": [x, y, x + w, y + h], "layers": [{"shape": disc, "fill": paint, "soft": 0.6},
                                                    {"shape": arrow, "fill": grgb, "clip": disc}]}
    return item, (round(cx, 2), round(cy, 2), round(r, 2), round(res, 3))


def world_back():
    if not F._only("world-back"):
        return
    base, _ = c.sheet(BASE)
    btn, _ = c.sheet(BTN)
    cell = (0x2dc, 0x214, 0x44, 0x44)
    x, y, w, h = cell
    arrow = {"t": "arrow", "a": [781.0, 557.8], "b": [755.8, 557.8], "w": 10.0, "hl": 12.0, "hw": 18.0}
    # the hover arrow's shift: the yellow pixels against the idle arrow's navy ones, best overlap within 3 px
    ia = fit.lum(base[y:y + h, x:x + w]) < 60
    hb = btn[y:y + h, x:x + w]
    ha = (hb[..., 0] > 190) & (hb[..., 2] < 120)
    best = None
    for sy in range(-4, 5):
        for sx in range(-4, 5):
            sh = np.roll(np.roll(ia, sy, 0), sx, 1)
            v = (sh & ha).sum() / max((sh | ha).sum(), 1)
            if best is None or v > best[0]:
                best = (v, sx, sy)
    harrow = F.mv(arrow, best[1], best[2])
    it_i, ci = disc_state(base, cell, arrow)
    it_h, ch = disc_state(btn, cell, harrow)
    F.deliver("world-back", [{"name": "idle", "art": base, "scene": [it_i], "rect": cell},
                             {"name": "hover", "art": btn, "scene": [it_h], "rect": cell}],
              "matte lilac disc (circle fitted per state, lit paint fitted per state over the whole disc with the arrow "
              "inpainted out of the art first) with the block arrow pointing left (shaft 10 px, head 12 long and 18 tall); "
              f"navy idle; on hover the disc as the art draws it (circle {ch[:3]} against idle {ci[:3]}) and the yellow arrow "
              f"moved with it by ({best[1]}, {best[2]}) px",
              "the surface under the arrow is now fitted with the arrow inpainted away, so no blue or yellow blobs appear "
              "under it; the hover disc and arrow sit where the hover art has them", z=5,
              extra={"idle_circle": ci, "hover_circle": ch, "hover_shift": [best[1], best[2]]})
    BACKDROP["world-back"] = it_i


BACKDROP = {}


def main():
    editor_bubbles()
    world_bubbles()
    world_back()


if __name__ == "__main__":
    main()
