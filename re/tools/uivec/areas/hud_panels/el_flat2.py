"""P41 hud_panels fix round (2026-11-24): the flat parts Mariano sent back.

member-bar (redo), member-slider ("rounded corners"), member-page-left / -right ("better shade"), match-plate ("rounded
top corners"), golfer-story-band ("better shading"). Read from luminance dumps (common.dump), sheet coordinates.

Bars (member-bar, the slider's two bars, the match plate's two bars): a navy one-pixel outline round a white glossy
inside, its corners r 2.5 at the left end and r 4 at the right end (slider: (99, 2) curving to (101, 6)); the inside white at the top, grey at the foot (the art's own column), the right end darkening toward its lower
right corner: the bar's own top-to-bottom colours plus a measured linear darkening over the last 6 px of the right end); a one-pixel drop shadow under each slider bar (row 17
/ 38, 83). The member-bar cells are the bars with the ball knob's circle cut out of their left end (transparent in the
art; circle fitted to the cut's edge pixels); round 1 drew the whole bar there.

Page arrows: each cream triangle has a one-pixel shadow along its upper edge that is darkest near the tip (77-84) and
fades toward the top vertex (124), a darker middle on its upright edge (158 against 236), and soft (antialiased) lower
edges; the tab's two top rows lit. Left tabs: tip (0.9, 156.3), upright edge x 14.9; right tabs: upright edge x 3.4 on
the left (itself in shadow, 93-116), tip (17.4, 256.6).

Match plate: TWO parts on top of each other, which round 1 had merged: a strip on top (rows 301-313, x 307-800,
brightening from left (127) to right (169-177), lit along its top rows, its top right corner rounded r 4, its right end
a dark bevel) and the plate below (from row 313; top left corner r 13, top right r 4; lit top edge 191-183-159-128;
dark right bevel 104-78-58-34 over x 796-799). Both corners now rounded.

Story band: the cream box is a sunken well, not a box with a navy line: its walls are dark on the left and top (154 ->
59 over x 68-72) and lit on the right (170-180 over x 457-459) and bottom (213 -> 162 over rows 412-416); painted as one
lit surface measured on the band's pixels whose edge is the well's opening (the walls), on the band's own lit plate
(light left edge, dark right and bottom bevels). Well opening x 73-457, rows 303-412, corners r 8 (read on its top left
corner rows 304-309).
"""
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import common as C  # noqa: E402
import kit as K  # noqa: E402
import el_bubbles as EB  # noqa: E402
import el_marks as M  # noqa: E402
import el_slabs as S  # noqa: E402
import el_plate as P  # noqa: E402
import figs  # noqa: E402
import fit  # noqa: E402
import sdf  # noqa: E402
from kit import U, rect, circle, ellipse, poly, sub, inter, outline  # noqa: E402,F401

limb = figs.limb
BAR_STOPS = [0.0, 0.15, 0.3, 0.5, 0.75, 1.0]


def half(x, keep_left):
    return {"t": "half", "p": [x, 0], "n": [1, 0] if keep_left else [-1, 0]}


def bar_shape(x0, y0, x1, y1, rl, rr):
    return figs.rrect4(x0, y0, x1, y1, rl, rr, rr, rl)


def bar_layers(key, x0, y0, x1, y1, rl=2.5, rr=4.0, cut=None, shadow=False):
    """A white glossy bar (see the module note). cut: a shape removed from the whole bar (the knob's circle)."""
    outer = bar_shape(x0, y0, x1, y1, rl, rr)
    inner = bar_shape(x0 + 1, y0 + 1, x1 - 1, y1 - 1, max(rl - 1, 0.4), max(rr - 1, 0.4))
    c = (lambda s: s) if cut is None else (lambda s: sub(s, cut))
    L = []
    if shadow:
        L.append(K.lay(c(rect(x0 + 2, y1, x1 - 1.5, y1 + 1)), K.sample(key, rect(int(x1) - 6, int(y1), int(x1) - 2, int(y1) + 1),
                                                                         depth=0.0)))
    navy = K.sample(key, rect(int(x0) + 6, int(y0), int(x1) - 6, int(y0) + 1), depth=0.0)     # the top line's own row
    L.append(K.lay(c(outer), navy))
    xm = int((x0 + x1) / 2)
    L.append(K.lay(c(inner), M.vgrad(key, xm, int(y0) + 1, int(y1) - 1)))
    # the right end darkens toward its edge (row by row the art drops ~30-50 levels over its last 6 px): the same paint
    # again over the end with a linear tone that is 0 six pixels in and -D at the edge
    rgb, _ = C.sheet(key)
    ym = int((y0 + y1) / 2)
    D = float(np.mean(rgb[ym - 2:ym + 3, xm]) - np.mean(rgb[ym - 2:ym + 3, int(x1) - 2]))
    if D > 4:
        end = inter(c(inner), rect(x1 - 7.0, y0, x1, y1))
        L.append({"shape": end, "fill": M.vgrad(key, xm, int(y0) + 1, int(y1) - 1),
                  "fx": [{"t": "tone", "a": [x1 - 13.0, ym], "b": [x1 - 1.0, ym], "add": round(D, 1)}]})
    return L


def cut_circle(key, rct, box):
    """The knob's circle cut out of a member-bar cell: fitted to the opaque pixels next to the transparent cut."""
    rgb, m = C.sheet(key)
    x0, y0, x1, y1 = box
    pts = []
    for y in range(y0, y1):
        for x in range(x0, min(x0 + 16, x1)):
            if m[y, x]:
                nb = [(x + dx, y + dy) for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1))]
                if any(x0 <= a < x1 and y0 <= b < y1 and not m[b, a] for a, b in nb):
                    pts.append((x + 0.5, y + 0.5))
    cx, cy, r, _ = fit.fit_circle(np.array(pts), trim=0.8)
    return circle(cx, cy, r)


def build_bars(el):
    scenes = {}
    for st, key, rct in C.CELLS[el]:
        x0, y0, w, h = rct
        bx = M.opaque_box(key, rct)
        cut = cut_circle(key, rct, bx)
        scenes[st] = [K.item(bar_layers(key, *bx, cut=cut), box=[x0, y0, x0 + w, y0 + h])]
    return scenes


def build_slider(el):
    st, key, rct = C.CELLS[el][0]
    x0, y0, w, h = rct
    L = [K.lay(rect(x0, y0, x0 + w, y0 + h), M.cell_bg(key, rct)),
         K.lay(rect(x0 + w - 2, y0, x0 + w - 1, y0 + h), K.sample(key, rect(119, 10, 120, 30), depth=0.0)),
         K.lay(rect(x0 + w - 1, y0, x0 + w, y0 + h), K.sample(key, rect(120, 10, 121, 30), depth=0.0))]
    L += bar_layers(key, 10.0, 2.0, 102.0, 17.0, shadow=True)
    L += bar_layers(key, 10.0, 23.0, 102.0, 38.0, shadow=True)
    L += M.ball_layers(key, 10.0, 19.5, 8.0)
    return {st: [K.item(L, box=[x0, y0, x0 + w, y0 + h])]}


# ------------------------------------------------------------------------------------------------------------ arrows
def arrow_layers(key, x0, y0, right, k):
    """One cream triangle of a page tab (cell origin x0, y0; k = 0 / 1 for the first / second triangle, 18 px apart)."""
    o = 18 * k
    if not right:
        tip, top, bot = (x0 + o + 0.9, y0 + 6.3), (x0 + o + 14.9, y0 + 1.0), (x0 + o + 14.9, y0 + 11.8)
        sh = [(x0 + o + 2.3, y0 + 5.3), (x0 + o + 8.0, y0 + 3.2), (x0 + o + 13.6, y0 + 1.6)]
        edge = rect(x0 + o + 13.9, y0 + 3.6, x0 + o + 15.0, y0 + 9.4, 0.5)
    else:
        tip, top, bot = (x0 + o + 17.4, y0 + 6.6), (x0 + o + 3.4, y0 + 1.6), (x0 + o + 3.4, y0 + 11.7)
        sh = [(x0 + o + 4.6, y0 + 1.85), (x0 + o + 10.0, y0 + 3.75), (x0 + o + 15.4, y0 + 5.75)]
        edge = rect(x0 + o + 2.4, y0 + 2.2, x0 + o + 3.45, y0 + 10.8, 0.3)
    tri = poly(tip, top, bot)
    rgb, m = C.sheet(key)
    cx = int((tip[0] + top[0]) / 2)
    cream = K.sample(key, rect(cx, int(tip[1]), cx + 2, int(tip[1]) + 1), depth=0.0)
    d0 = [round(float(v), 1) for v in rgb[int(sh[0][1] - 0.45), int(sh[0][0])]]
    d1 = [round(float(v), 1) for v in rgb[int(sh[-1][1] - 0.45), int(sh[-1][0])]]
    shadow = limb([(sh[0][0], sh[0][1], 0.85), (sh[1][0], sh[1][1], 0.85), (sh[2][0], sh[2][1], 0.75)])
    ec = K.sample(key, edge, depth=0.0)
    return [K.lay(shadow, {"linear": {"a": list(sh[0]), "b": list(sh[-1]), "stops": [[0.0, d0], [0.6, d0], [1.0, d1]]}}),
            K.lay(tri, cream),
            K.lay(inter(edge, tri), ec, soft=1.0) if not right else K.lay(edge, ec)]


def build_member_arrows(el):
    scenes = {}
    right = el.endswith("right")
    for st, key, rct in C.CELLS[el]:
        x0, y0, w, h = rct
        L = [K.lay(rect(x0, y0, x0 + w, y0 + h), M.vgrad(key, x0 + 17, y0, y0 + h))]
        for k in range(2):
            L += arrow_layers(key, x0, y0, right, k)
        scenes[st] = [K.item(L, box=[x0, y0, x0 + w, y0 + h])]
    return scenes


# ------------------------------------------------------------------------------------------------------------ match plate
def corners(x0, y0, x1, y1, rl, rr):
    """Top corners rounded (left rl, right rr), bottom ones square (the plate runs on below the cell)."""
    return figs.rrect4(x0, y0, x1, y1, rl, rr, 0.0, 0.0)


def build_plate(el):
    st, key, rct = C.CELLS[el][0]
    x0, y0, w, h = rct
    rgb, m = C.sheet(key)
    body = corners(297.0, 313.0, 800.0, 430.0, 13.0, 4.0)     # cut by the cell (its rows below 407 are transparent)
    plate = inter(body, rect(x0, y0, x0 + w, 407.0))
    strip = inter(corners(300.0, 301.0, 800.0, 320.0, 2.0, 4.0), poly((309.3, 300.0), (801, 300.0), (801, 314.0), (306.4, 314.0)))
    xl = [436] + list(range(454, 761, 18))
    xspans = [(a, b - 1) for a, b in zip(xl, xl[1:])] + [(762, 794)]
    xspans[0] = (436, 453)
    yspans = [(321, 336), (337, 352), (353, 374), (375, 396)]
    block = (434.0, 320.0, 800.0, 398.0)
    bars = U(rect(306.0, 355.0, 434.0, 372.0), rect(306.0, 376.0, 434.0, 393.0), circle(307.0, 373.0, 9.0))
    L = [K.lay(plate, S.fit_bevel(key, plate, [x0, 313, x0 + w, y0 + h], exclude=U(rect(*block), bars), L=8.0, of=body))]
    # the strip: brightening left to right (row 309), lit along its top rows (x 600 column), a dark right bevel
    base = [[float(t), K.sample(key, rect(int(310 + t * 484), 307, int(310 + t * 484) + 2, 312), depth=0.0)]
            for t in np.linspace(0, 1, 7)]
    L.append(K.lay(strip, {"linear": {"a": [310.0, 0], "b": [794.0, 0], "stops": base}}))
    for r in range(301, 305):      # the lit top rows: each its own left -> right colours
        st_ = [[float(t), K.sample(key, rect(int(312 + t * 480), r, int(312 + t * 480) + 2, r + 1), depth=0.0)]
               for t in np.linspace(0, 1, 5)]
        L.append(K.lay(inter(strip, rect(300, r, 801, r + 1)), {"linear": {"a": [312.0, 0], "b": [792.0, 0], "stops": st_}}))
    bev = [[float(t), K.sample(key, rect(796 + k, 306, 797 + k, 312), depth=0.0)] for k, t in enumerate((0.125, 0.375, 0.625, 0.875))]
    L.append(K.lay(inter(strip, rect(796.0, 304.0, 800.0, 313.0)), {"linear": {"a": [796.0, 0], "b": [800.0, 0], "stops": bev}}))
    L += P.grid_layers(key, xspans, yspans, block)
    L.append(K.lay(rect(795.0, 321.0, 797.0, 396.0), K.sample(key, rect(795, 330, 797, 340), depth=0.0)))
    L += bar_layers(key, 306.6, 355.6, 433.6, 371.0, rl=3.0, rr=3.5)
    L += bar_layers(key, 306.6, 376.6, 433.6, 392.0, rl=3.0, rr=3.5)
    L += M.ball_layers(key, 307.0, 373.0, 8.0)
    return {st: [K.item(L, box=[x0, y0, x0 + w, y0 + h])]}


# ------------------------------------------------------------------------------------------------------------ story band
def build_story(el):
    st, key, rct = C.CELLS[el][0]
    x0, y0, w, h = rct
    plate = rect(x0 + 0.5, y0 + 0.5, x0 + w - 0.5, y0 + h - 0.5, 6.0)
    hole = rect(73.0, 303.0, 457.0, 412.0, 8.0)
    well = {"t": "grow", "of": hole, "d": 6.0}
    box = [x0, y0, x0 + w, y0 + h]
    L = [K.lay(plate, S.fit_bevel(key, plate, box, exclude=well))]
    walls = inter(sub(well, hole), plate)
    opening = sub(rect(x0 - 50, y0 - 50, x0 + w + 50, y0 + h + 50), hole)     # lit by the well's edge: walls face into it
    L.append(K.lay(walls, S.fit_bevel(key, walls, box, L=6.0, stops=[0.0, 0.2, 0.4, 0.6, 0.8, 1.0], of=opening)))
    L.append(K.lay(hole, K.sample(key, rect(200, 330, 300, 390), depth=0.0)))
    return {st: [K.item(L, box=[x0, y0, x0 + w, y0 + h])]}


EB.BUILD.update({"member-bar": build_bars, "member-slider": build_slider, "member-page-left": build_member_arrows,
                 "member-page-right": build_member_arrows, "match-plate": build_plate, "golfer-story-band": build_story})
