"""P41 frontend area: the character editor (CustGolfBckgrnd.pcx / CustGlfBckMale.pcx, buttons and pills in CGButtons.pcx).

Glyphs are authored from the text dumps of each cell (log/roadmap/P41/work/frontend/_view/cg_dump.txt, 2026-11-24), in
coordinates local to their cell (the idle column of CGButtons at x 300, one cell per 50 rows) and moved to every place the
button is drawn (the selected column at x 350; the backdrop, found by matching the cell's dark pixels). The bubbles' paint is
measured per state on their own pixels (accept.solve_bevel over the body minus the glyph). The flat boxes are boxes.py's.

Usage: py -3.12 re/tools/uivec/areas/frontend/editor_screen.py
"""
import copy
import json

import numpy as np

import common as c
import parts
from common import fit, sdf

BG_F = "Interface/CustGolfBckgrnd.pcx"
BG_M = "Interface/CustGlfBckMale.pcx"
CB = "Interface/CGButtons.pcx"
NAVY = [33, 41, 80]
BUBBLE_STOPS = [0.0, 0.05, 0.12, 0.22, 0.36, 0.55, 0.8, 1.0]


def U(*s):
    return {"t": "union", "of": list(s)}


def box(x0, y0, x1, y1, r=0.0):
    return parts.rr(x0, y0, x1, y1, r)


def poly(*pts):
    return {"t": "poly", "rings": [[list(p) for p in pts]]}


def line(*pts, w=1.0, cap="round"):
    return {"t": "stroke", "pts": [list(p) for p in pts], "w": w, "cap": cap}


def circ(cx, cy, r):
    return {"t": "circle", "c": [cx, cy], "r": r}


def ell(cx, cy, rx, ry):
    return {"t": "ellipse", "c": [cx, cy], "r": [rx, ry], "rot": 0.0}


def mv(s, dx, dy):
    return {"t": "move", "of": s, "d": [dx, dy]}


# ------------------------------------------------------------------------------------------- glyphs, cell-local coordinates
def g_outfit():
    """A jumpsuit seen from the front: a yoke with a collar notch, two short sleeves standing off the torso, a torso that
    narrows at the waist and widens into two splayed legs."""
    yoke = poly((13.0, 7.0), (16.8, 7.0), (18.3, 9.2), (19.8, 7.0), (24.6, 7.0), (27.6, 10.0), (28.8, 11.8), (10.0, 11.8), (11.0, 9.4))
    sl_l = poly((10.0, 11.0), (14.2, 11.0), (13.6, 14.0), (10.8, 14.0))
    sl_r = poly((24.0, 11.0), (28.8, 11.0), (27.2, 14.0), (24.4, 14.0))
    torso = poly((15.0, 11.0), (23.0, 11.0), (23.0, 14.5), (22.0, 15.2), (22.0, 17.6), (23.2, 19.0), (24.6, 23.2),
                 (13.4, 23.2), (14.8, 19.0), (16.0, 17.6), (16.0, 15.2), (15.0, 14.5))
    leg_l = poly((14.0, 22.8), (18.2, 22.8), (14.8, 30.4), (10.8, 30.4))
    leg_r = poly((20.0, 22.8), (25.0, 22.8), (27.2, 30.4), (23.2, 30.4))
    return U(yoke, sl_l, sl_r, torso, leg_l, leg_r)


def g_family():
    """Two people side by side: an adult (round head, shoulders, arms angled out, a torso, two splayed legs with feet) and a
    smaller child to the right drawn the same way."""
    adult = U(circ(15.3, 9.8, 2.7), box(10.6, 13.0, 18.6, 15.0, 1.0), box(13.0, 14.0, 17.0, 23.0),
              line((11.2, 14.4), (7.8, 18.6), w=1.8), line((18.0, 14.4), (20.2, 18.6), w=1.8),
              line((13.4, 22.6), (9.6, 29.4), w=2.0, cap="butt"), line((16.4, 22.6), (18.6, 29.4), w=2.0, cap="butt"),
              box(8.0, 28.0, 11.6, 30.0), box(17.0, 28.0, 20.6, 30.0))
    child = U(circ(25.6, 15.6, 2.1), box(22.0, 19.0, 29.0, 20.6, 0.8), box(24.0, 19.6, 27.0, 24.0),
              line((22.6, 20.0), (20.8, 23.2), w=1.4), line((28.4, 20.0), (29.6, 23.2), w=1.4),
              line((24.4, 23.6), (22.6, 29.4), w=1.6, cap="butt"), line((26.4, 23.6), (28.0, 29.4), w=1.6, cap="butt"))
    return U(adult, child)


def g_head():
    """A bust: an oval head, a short neck below a one-pixel gap, a wide shoulder blob."""
    return U(ell(19.0, 14.6, 7.0, 7.9), box(15.8, 23.8, 22.4, 26.6), ell(19.2, 28.6, 12.0, 3.3))


def g_shirt():
    """A T-shirt: a yoke with short sleeves and a V-neck notch, the body tapering a little toward the hem."""
    yoke = {"t": "sub", "a": box(9.0, 11.0, 29.0, 15.9), "b": poly((16.6, 10.0), (20.2, 10.0), (18.4, 13.8))}
    body = poly((12.0, 15.6), (25.0, 15.6), (23.2, 27.9), (14.0, 27.9))
    return U(yoke, body)


def g_pants():
    """Trousers: a waistband block and two legs splaying out, the crotch notch opening downward."""
    return poly((15.0, 8.0), (24.0, 8.0), (25.8, 14.0), (27.8, 28.6), (23.2, 28.6), (19.4, 15.2), (18.4, 15.2), (16.4, 28.6),
                (11.0, 28.6), (12.8, 14.0))


def g_hair():
    """A face framed by hair: two hair lumps on top parted in the middle, strands down both sides to the jaw, the face
    inside (a 1 px gap round it), a neck and shoulders."""
    face = ell(18.8, 18.6, 4.6, 6.0)
    hair = {"t": "sub", "a": {"t": "sub", "a": ell(18.9, 18.8, 8.1, 9.3), "b": {"t": "grow", "of": face, "d": 1.0}},
            "b": U(box(14.0, 23.4, 23.8, 32.0), poly((18.0, 8.0), (19.6, 8.0), (19.2, 11.0), (18.4, 11.0)))}
    return U(hair, face, box(16.0, 24.6, 22.0, 26.8), ell(19.6, 28.6, 8.0, 2.4))


def g_body():
    """A body figure for the body-type choice: a small head, the torso and arms in outline with the right half of the
    torso filled, the left leg filled and the right leg in outline, the feet below."""
    head = U({"t": "outline", "of": circ(19.0, 8.2, 3.1), "w": 1.3}, {"t": "inter", "of": [circ(19.0, 8.2, 3.1),
                                                                                         {"t": "half", "p": [19.0, 0], "n": [1, 0]}]})
    torso = U(line((14.0, 12.4), (25.0, 12.4), (24.4, 21.0), (15.0, 21.0), (14.0, 12.4), w=1.3, cap="butt"),
              poly((19.0, 12.0), (25.4, 12.0), (24.4, 20.6), (19.0, 20.6)))
    arms = U(line((12.6, 13.4), (10.0, 22.6), w=2.0), line((25.8, 13.6), (28.0, 22.4), w=1.2))
    legs = U(poly((15.0, 20.8), (19.6, 20.8), (19.4, 31.2), (16.4, 31.2)), line((23.4, 21.0), (23.0, 30.4), w=1.3),
             box(13.0, 30.6, 19.6, 32.0), box(20.4, 30.6, 25.0, 31.8))
    return U(head, torso, arms, legs)


def g_gender():
    """Male and female symbols interlocked: two rings side by side sharing their middle, the male one with an arrow
    pointing up and left, the female one with a cross below."""
    ring_l = {"t": "outline", "of": circ(16.0, 18.4, 5.0), "w": 2.6}
    ring_r = {"t": "outline", "of": circ(24.4, 18.4, 5.0), "w": 2.6}
    arrow = U(line((12.6, 14.4), (9.0, 10.6), w=2.2, cap="butt"), poly((5.8, 7.6), (11.8, 8.8), (7.2, 13.4)))
    cross = U(box(23.0, 23.6, 25.8, 35.0), box(19.2, 28.2, 29.6, 31.0))
    return U(ring_l, ring_r, arrow, cross)


def g_load():
    """CGButtons #0 (bubble at its cell's 15.5, 15): an open folder, the lid's hooked line above it."""
    flap = poly((13.0, 14.2), (23.0, 14.2), (20.4, 22.0), (8.8, 22.0))
    back = {"t": "sub", "a": U(box(10.0, 10.0, 14.0, 11.0), box(8.6, 11.0, 19.0, 13.2), box(8.0, 13.0, 10.6, 22.0)),
            "b": {"t": "grow", "of": flap, "d": 0.9}}
    lid = {"t": "qstroke", "p": [[14.4, 7.4], [18.8, 2.8], [23.4, 10.8]], "w": [2.0, 2.0]}
    return U(back, flap, lid)


def g_save():
    """CGButtons #1 (cell at x 99): a floppy disk: two sides, the shutter block, the middle band, the label block."""
    return U(box(107.0, 9.0, 109.0, 22.0), box(120.0, 10.0, 122.0, 22.0), box(110.0, 9.0, 119.0, 14.0),
             box(107.0, 14.8, 122.0, 17.0), box(111.0, 18.0, 116.0, 22.0))


def g_reset():
    """CGButtons #3 (cell at x 500): one arrow round a circle (centre 520.2, 18.3, radius 10.2, line 3) from the left over
    the top and down the right to a head at the bottom pointing left, and a dot at the lower left where it started."""
    arc = {"t": "arc", "c": [520.2, 18.3], "r": 10.2, "a0": 168.0, "sweep": 250.0, "w": 3.0}
    head = poly((517.3, 25.8), (524.8, 20.8), (523.6, 29.8))
    return U(arc, head, circ(511.8, 25.8, 2.8))


def g_ok():
    """CGButtons #8 (cell at 500, 100): a check mark as one outline (Load Game's reading): flat top on the long stroke, the
    V notch, flat top on the short stroke, one flat foot."""
    return poly((530.6, 113.6), (539.4, 113.6), (534.8, 120.0), (531.4, 125.0), (529.8, 131.0), (528.8, 138.6), (518.8, 138.6),
                (517.6, 132.0), (511.4, 125.8), (520.2, 125.8), (523.6, 130.8), (525.4, 125.0), (527.6, 120.0))


GLYPHS = {"outfit": g_outfit, "family": g_family, "head": g_head, "shirt": g_shirt, "pants": g_pants, "hair": g_hair,
          "body": g_body, "gender": g_gender}
ORDER = ["outfit", "family", "head", "shirt", "pants", "hair", "body", "gender"]


# ------------------------------------------------------------------------------------------------------- measuring bubbles
def rim_circle(art, rect, thr=90, hint=None):
    """The bubble's rim: the outermost dark pixels of the cell on rays from its middle, fitted to a circle. hint (cx, cy,
    rmax) in frame coordinates keeps only the dark pixels within rmax of the hinted centre (the reset and check bubbles
    have a second, shadow ring to their lower right)."""
    x, y, w, h = rect
    d = fit.lum(art[y:y + h, x:x + w]) < thr
    cx, cy, r, res = None, None, None, None
    c0 = (w / 2, h / 2)
    if hint is not None:
        jj, ii = np.mgrid[0:h, 0:w]
        d &= np.hypot(ii + 0.5 + x - hint[0], jj + 0.5 + y - hint[1]) < hint[2]
        c0 = (hint[0] - x, hint[1] - y)
    for _ in range(3):
        pts = fit.outermost(d, c0)
        cx, cy, r, res = fit.fit_circle(pts, trim=0.8)
        c0 = (cx, cy)
    return cx + x, cy + y, r + 0.15, res


def locate(cell_art, rect, target, window):
    """Where the cell's drawing sits in target: the shift whose dark pixels (L < 90) overlap the cell's best."""
    x, y, w, h = rect
    d = fit.lum(cell_art[y:y + h, x:x + w]) < 90
    Lt = fit.lum(target) < 90
    wx0, wy0, wx1, wy1 = window
    best = None
    for ty in range(wy0, wy1 - h):
        for tx in range(wx0, wx1 - w):
            t = Lt[ty:ty + h, tx:tx + w]
            v = (d & t).sum() / max((d | t).sum(), 1)
            if best is None or v > best[0]:
                best = (v, tx - x, ty - y)
    return best


def bubble(art, cx, cy, r, glyph, rim_w=1.6, shadow=True):
    """A glossy bubble in a navy rim, paint measured on art: the rim colour (median of the ring just inside r), the body's
    lit surface (accept.solve_bevel over the body minus the glyph), the glyph colour, the soft shadow to the lower right (its
    strength: how much darker the ring outside the rim is there than at the upper left)."""
    bx0, by0 = max(0, int(cx - r - 4)), max(0, int(cy - r - 4))
    bx1, by1 = int(cx + r + 6), int(cy + r + 6)
    X, Y = parts.grid(bx0, by0, bx1, by1)
    crop = art[by0:by1, bx0:bx1]
    rr_ = np.hypot(X - cx, Y - cy)
    body = circ(cx, cy, r - rim_w)
    gd = sdf.sd(glyph, X, Y)
    m = (rr_ < r - rim_w - 0.5) & (gd > 2.0) & (fit.lum(crop) > 100)      # not the glyph, nor any navy of the art's
    paint = parts.bevel_fit(crop, body, m, bx0, by0, r - rim_w, BUBBLE_STOPS)
    ring = crop[(rr_ > r - rim_w - 0.5) & (rr_ < r - 0.2)]
    lr_ = fit.lum(ring)
    rim = [round(float(v), 1) for v in np.median(ring[lr_ <= np.percentile(lr_, 15)], axis=0)]     # the line's own navy
    g = crop[gd < -0.8]
    grgb = [round(float(v), 1) for v in np.median(g, axis=0)] if len(g) else NAVY
    L = []
    if shadow:
        Lm = fit.lum(crop)
        ang = np.degrees(np.arctan2(Y - cy, X - cx))
        ring = (rr_ > r + 0.8) & (rr_ < r + 2.8)
        lr = ring & (ang > 20) & (ang < 70)
        ul = ring & (ang > -160) & (ang < -110)
        if lr.any() and ul.any():
            k = float(np.clip(1 - Lm[lr].mean() / max(Lm[ul].mean(), 1), 0, 0.8))
            L.append({"shape": circ(cx + 1.6, cy + 2.0, r + 0.6), "fill": [40, 40, 70], "soft": 2.0, "alpha": round(k * 1.6, 3)})
    L += [{"shape": circ(cx, cy, r), "fill": rim},
          {"shape": body, "fill": paint},
          {"shape": glyph, "fill": grgb, "clip": circ(cx, cy, r - 0.5)}]
    return {"box": [bx0, by0, bx1, by1], "layers": L}


def bx_item(b):
    import boxes as bxm
    return bxm.item(b)


# ------------------------------------------------------------------------------------------------------------ pills etc.
def pill(art, rect, tail=False, step=False):
    """A tan pill: a rounded rectangle in a navy line (thin on top and left, 1.5 px at the bottom and right) whose fill is a
    lit surface measured on its pixels, a soft shadow under it; the tail variant has a 2 px wire down from its lower left,
    the tall variant sits on a lilac step at its lower right."""
    x, y, w, h = rect
    Lm = fit.lum(art)
    sub = Lm[y:y + h, x:x + w]
    # the outline's outer extent, read down the middle column: the first line row (the top line is a light navy, under 165)
    # and the last row of the bottom line before the light shadow (a tail or a step below starts after a light gap)
    col = sub[:, w // 2]
    dark = col < 165
    k0 = int(np.nonzero(dark[:6])[0][0])
    k = k0 + 1
    while k < h and not dark[k]:
        k += 1
    while k < h and dark[k]:
        k += 1
    y0, y1 = y + k0, y + k
    mid = (y0 + y1) // 2
    row = sub[mid - y]
    xs = np.nonzero(row < 165)[0]
    x0, x1 = x + xs[0], x + xs[-1] + 1
    r = (y1 - y0) / 2 * 0.5
    outer = box(x0, y0, x1, y1, r)
    inner = box(x0 + 1.0, y0 + 1.0, x1 - 1.5, y1 - 1.5, max(r - 1, 0.5))
    X, Y = parts.grid(x0, y0, x1, y1)
    m = sdf.sd(inner, X, Y) < -0.8
    paint = parts.bevel_fit(art[y0:y1, x0:x1], inner, m, x0, y0, (y1 - y0) / 2, [0.0, 0.1, 0.25, 0.45, 0.7, 1.0])
    line_rgb = [round(float(v), 1) for v in np.median(art[y0:y1, x0:x1][(sdf.sd(outer, X, Y) < -0.2) & (sdf.sd(inner, X, Y) > 0.3)], axis=0)]
    L = [{"shape": mv(outer, 1.5, 2.6), "fill": [60, 60, 80], "soft": 1.8, "alpha": 0.45}]
    if tail:
        L.append({"shape": box(x0 + 22.0, y1 - 1.0, x0 + 24.0, y + h), "fill": NAVY})
    if step:
        L.append({"shape": box(x0 + 84.0, y1 - 1.0, x + w, y + h), "fill": [165, 166, 210]})
        L.append({"shape": box(x0 + 84.0, y1 - 1.0, x0 + 86.0, y + h), "fill": NAVY})
    L += [{"shape": outer, "fill": line_rgb}, {"shape": inner, "fill": paint}]
    return {"box": [x0 - 3, y0 - 3, x + w + 1, y + h + 1], "layers": L}, (x0, y0, x1, y1)


def main():
    import boxes as bxm
    art, _ = c.sheet(BG_F)
    male, _ = c.sheet(BG_M)
    cg, _ = c.sheet(CB)
    idx, pal = fit.load_sheet(BG_F)
    boxes, _ = bxm.measure(pal[idx])
    bscene = []

    # ---- the eight tan glyph buttons: idle column x 300, selected column x 350, one cell per 50 rows
    windows = {"outfit": (420, 0, 500, 70), "family": (420, 50, 500, 120), "head": (420, 100, 500, 170),
               "shirt": (420, 150, 500, 220), "pants": (420, 200, 500, 270), "hair": (295, 20, 370, 100),
               "body": (295, 70, 370, 150), "gender": (295, 120, 370, 200)}
    for k, name in enumerate(ORDER):
        y = 50 * k
        g_local = GLYPHS[name]()
        st = []
        for state, x0 in (("idle", 300), ("selected", 350)):
            rect = (x0, y, 50, 50)
            cx, cy, r, res = rim_circle(cg, (x0, y, 42, 42))
            gl = mv(g_local, x0, y)
            it = bubble(cg, cx, cy, r, gl)
            st.append({"name": state, "art": cg, "scene": [it], "rect": (x0, y, 50, 46)})
        # where the backdrop draws it (idle), found by overlapping the dark pixels
        v, dx, dy = locate(cg, (300, y, 42, 42), art if name != "hair" else art, windows[name])
        cx, cy, r, _ = rim_circle(cg, (300, y, 42, 42))
        bit = bubble(art, cx + dx, cy + dy, r, mv(g_local, 300 + dx, y + dy))
        bscene.append(bit)
        st.append({"name": "on the backdrop", "art": art, "scene": [bit], "rect": (300 + dx, y + dy, 46, 46)})
        sc = c.element(f"editor-{name}", st, f"tan glass bubble in a navy rim (circle fitted to the rim, paint measured "
                       f"per state on its own pixels) with an authored glyph: {GLYPHS[name].__doc__.strip().split('.')[0].lower()}; "
                       "selected is the same shapes in the yellow paint; the third row is the button where the backdrop "
                       f"draws it (found at {300 + dx}, {y + dy}, dark-pixel overlap {v:.2f})",
                       unsure="glyph read from a 30 px drawing" + (": a detailed figure, look at its parts" if name in ("body", "family", "hair", "gender") else ""))
        print(f"editor-{name}", sc, (dx, dy, round(v, 3)))

    # ---- load / save / reset / ok: yellow on CGButtons (hover), blue where the backdrop draws them
    # rim hints read off the dumps: the bubble's left rim column and its centre row (the reset and check cells also hold a
    # shadow ring to the lower right that is not the rim)
    specs = [("editor-load", (0, 0, 36, 38), g_load(), (20, 0, 80, 50), "an open folder with the lid's hooked line above", None),
             ("editor-save", (99, 0, 37, 38), g_save(), (225, 0, 285, 50), "a floppy disk: sides, shutter, middle band, label", None),
             ("editor-reset", (500, 0, 45, 46), g_reset(), (730, 5, 795, 70), "one arrow round a circle with its head at the "
              "bottom pointing left and a dot where it starts", (519.6, 20.0, 19.4)),
             ("editor-ok", (500, 100, 58, 57), g_ok(), (725, 180, 800, 260), "a check mark as one outline", (526.0, 126.5, 27.0))]
    for eid, rect, glyph, window, how, hint in specs:
        cx, cy, r, res = rim_circle(cg, rect, hint=hint)
        print(eid, "rim", round(cx, 2), round(cy, 2), round(r, 2), round(res, 3))
        it_h = bubble(cg, cx, cy, r, glyph, rim_w=2.2 if r > 20 else 1.3)
        v, dx, dy = locate(cg, rect, art, window)
        it_i = bubble(art, cx + dx, cy + dy, r, mv(glyph, dx, dy), rim_w=2.2 if r > 20 else 1.3)
        bscene.append(it_i)
        x, y, w, h = rect
        sc = c.element(eid, [{"name": "idle (backdrop, blue)", "art": art, "scene": [it_i], "rect": (x + dx - 2, y + dy - 2, w + 4, h + 4)},
                             {"name": "hover (CGButtons, yellow)", "art": cg, "scene": [it_h], "rect": (max(x - 2, 0), max(y - 2, 0), w + 4, h + 4)}],
                       f"glass bubble in a navy rim (circle fitted, paint measured per state) with {how} (read off a dump); "
                       f"blue where the backdrop draws it (found at {x + dx}, {y + dy}, overlap {v:.2f}), yellow on the "
                       "CGButtons cell")
        print(eid, sc, (dx, dy, round(v, 3)))

    # ---- pills
    pl = []
    for nm, rect, tail in (("idle", (0, 50, 112, 28), False), ("selected", (150, 50, 112, 28), False),
                           ("tail idle", (0, 250, 112, 28), True), ("tail selected", (150, 250, 112, 28), True)):
        it, b = pill(cg, rect, tail=tail)
        pl.append({"name": nm, "art": cg, "scene": [it], "rect": rect})
    sc = c.element("editor-pill", pl, "a rounded tan pill in a navy line, its fill a lit surface measured on its own pixels "
                   "(light along the top, darker toward the bottom), a soft shadow under it; selected the same in the yellow "
                   "paint measured on that cell; the tail rows add a 2 px navy wire down from the lower left", z=3)
    print("editor-pill", sc)
    pt = []
    for nm, rect in (("idle", (0, 300, 112, 35)), ("selected", (150, 300, 112, 35)), ("idle 3", (0, 400, 112, 35)),
                     ("selected 3", (150, 400, 112, 35))):
        it, b = pill(cg, rect, step=True)
        pt.append({"name": nm, "art": cg, "scene": [it], "rect": rect})
    sc = c.element("editor-pill-tall", pt, "the taller pill: the same rounded tan pill (measured paint, navy line, soft "
                   "shadow) on a lilac step at its lower right edged by a 2 px navy line", z=3,
                   unsure="the step's extent is read on the first cell only")
    print("editor-pill-tall", sc)

    # ---- the oval
    x, y, w, h = 300, 500, 57, 38
    sub = cg[y:y + h, x:x + w]
    lil = (sub[..., 2] > sub[..., 0] + 25) & (fit.lum(sub) > 120)
    pts = fit.edge_points(lil) + [x, y]
    E = parts.ellipse_of(pts, rot=0.0, tol=0.06)
    o = ell(E[0], E[1], E[2], E[3])
    X, Y = parts.grid(x, y, x + w, y + h)
    paint = parts.bevel_fit(sub, o, sdf.sd(o, X, Y) < -0.6, x, y, E[3], [0.0, 0.1, 0.25, 0.5, 1.0])
    oval = {"box": [x, y, x + w, y + h], "layers": [
        {"shape": mv({"t": "grow", "of": o, "d": 1.4}, 1.5, 2.0), "fill": [50, 50, 80], "soft": 1.6, "alpha": 0.4},
        {"shape": {"t": "grow", "of": o, "d": 1.4}, "fill": NAVY}, {"shape": o, "fill": paint}]}
    sc = c.element("editor-oval", [{"name": "idle", "art": cg, "scene": [oval], "rect": (x, y, w, h)}],
                   "a lilac ellipse (fitted to the lilac's edge) in a 1.4 px navy line, lit paint measured, a soft shadow to "
                   "the lower right", extra={"ellipse": E})
    print("editor-oval", sc)

    # ---- name card, list, page (boxes.py where they are flat)
    tan = [bx_item(b) for b in boxes if b["fill"] == [223, 199, 151]]
    Lm = fit.lum(art)
    top = []
    for xx in range(60, 250, 3):
        col = Lm[2:40, xx]
        k = np.nonzero(col < 90)[0]
        if len(k):
            top.append((xx + 0.5, 2 + k[0] + 0.5))
    top = np.array(top)
    cxa, cya, ra, _ = fit.fit_circle(top, trim=0.8)
    cream = [round(float(v), 1) for v in np.median(art[25:38, 80:220].reshape(-1, 3), axis=0)]
    arch = circ(cxa, cya, ra)
    card_shape = {"t": "inter", "of": [arch, box(41.0, 0.0, 265.0, 46.0)]}
    card = {"name": "name card", "box": [36, 2, 270, 110], "layers": [
        {"shape": {"t": "inter", "of": [{"t": "grow", "of": arch, "d": 2.0}, box(41.0, 0.0, 265.0, 46.0)]}, "fill": NAVY},
        {"shape": card_shape, "fill": cream}]}
    sc = c.element("editor-name-card", [{"name": "idle", "art": art, "scene": [card] + tan, "rect": (30, 6, 248, 106)}],
                   "a cream card whose top is one arc (circle fitted to the navy line along it) under a 2 px navy line, "
                   "and the three tan stripes in navy lines (boxes.py)", z=2,
                   unsure="the load and save bubbles on its corners are their own elements; the cream backing below the "
                          "stripes is texture", extra={"arch": [round(cxa, 2), round(cya, 2), round(ra, 2)]})
    print("editor-name-card", sc)
    bscene += [card] + tan

    # the list: a dark column with an inner shadow along its top and left and navy hairlines, a cream ruled column
    hl_l = [yy for yy in range(270, 586) if Lm[yy, 200] < 60]
    hl_r = [yy for yy in range(270, 586) if Lm[yy, 600] < 200]
    fillL = [round(float(v), 1) for v in art[400, 200]]
    rows = [bx_item(b) for b in boxes if b["fill"] == [247, 239, 223]]
    L = [{"shape": box(40.0, 268.0, 400.0, 586.0), "fill": [20, 30, 46]},
         {"shape": box(41.0, 269.0, 399.0, 586.0), "fill": [68, 66, 96]},
         {"shape": mv(box(45.0, 273.0, 420.0, 600.0, 2.0), 0, 0), "fill": fillL, "soft": 2.4},
         {"shape": box(400.0, 268.0, 762.0, 586.0), "fill": [130, 136, 148]}]
    L += [{"shape": {"t": "seg", "a": [41.0, yy + 0.5], "b": [399.0, yy + 0.5], "w": 1.0}, "fill": [20, 33, 49]} for yy in hl_l]
    L.append({"shape": {"t": "seg", "a": [399.5, 268.0], "b": [399.5, 586.0], "w": 1.0}, "fill": [20, 33, 49]})
    lst = [{"name": "list", "box": [36, 264, 766, 590], "layers": L}] + rows
    sc = c.element("editor-list", [{"name": "idle", "art": art, "scene": lst, "rect": (36, 262, 732, 330)}],
                   f"a dark column (1 px navy line on top and left, a soft inner shadow along them, {len(hl_l)} navy hairlines "
                   "read per row, a navy divider) beside a cream column of flat rows (boxes.py) on the grey of its hairlines",
                   z=1, extra={"hairlines_left": hl_l, "hairlines_right": hl_r})
    print("editor-list", sc)
    bscene += lst

    # the page: cream, a straight left, top and bottom, a right edge that bulges (an ellipse fitted to it), a navy line
    pts = []
    for yy in range(24, 238, 2):
        row = Lm[yy, 600:760]
        k = np.nonzero(row < 90)[0]
        if len(k):
            pts.append((600 + k[0] + 0.5, yy + 0.5))
    pts = np.array(pts)
    ecx, ecy, er, _ = fit.fit_circle(pts, trim=0.8)
    Er = [round(ecx, 2), round(ecy, 2), round(er, 2), round(er, 2), 0.0]
    pc = [round(float(v), 1) for v in np.median(art[60:200, 500:680].reshape(-1, 3), axis=0)]
    rows_t = [yy for yy in range(5, 40) if Lm[yy, 600] < 90]
    rows_b = [yy for yy in range(220, 260) if Lm[yy, 600] < 90]
    t0, b1 = rows_t[0], rows_b[-1] + 1
    pg = {"t": "inter", "of": [box(440.0, t0, 800.0, b1), {"t": "union", "of": [ell(Er[0], Er[1], Er[2], Er[3]),
                                                                                  box(440.0, 0.0, Er[0], 300.0)]}]}
    lw = 2.0
    page = {"name": "page", "box": [440, t0 - 2, 740, b1 + 2], "layers": [
        {"shape": pg, "fill": NAVY}, {"shape": {"t": "grow", "of": pg, "d": -lw}, "fill": pc}]}
    # the right column of buttons is drawn over the page's left edge: they go on top here as in the art
    over_page = bscene[:5]
    sc = c.element("editor-page", [{"name": "idle", "art": art, "scene": [page] + over_page, "rect": (446, 14, 284, 236)}],
                   "a cream page in a 2 px navy line: straight top and bottom (rows read off the art), a right edge that "
                   "bulges out (a circle fitted to it row by row); its left side runs behind the button column", z=2,
                   extra={"right_edge": Er, "rows": [t0, b1]})
    print("editor-page", sc)
    bscene.append(page)

    # ---- the backdrop, both versions
    sc = c.element("editor-backdrop", [{"name": "female", "art": art, "scene": bscene, "rect": (0, 0, 800, 600)},
                                       {"name": "male", "art": male, "scene": [it for it in bscene if it is not bscene[5]],
                                        "rect": (0, 0, 800, 600)}],
                   "the editor screen: the cloud, the portrait window and the cream backing stay the texture, the card, "
                   "pills' strip, page, list and every button drawn over it; the male version lacks the hair button", z=1,
                   unsure="the tan pills on the backdrop are drawn by the game from CGButtons and are not in this picture")
    print("editor-backdrop", sc)
    c.kept("editor-portrait-window", "the arched sky-and-grass window behind the golfer stays the picture", art, (334, 16, 116, 230))


if __name__ == "__main__":
    main()
