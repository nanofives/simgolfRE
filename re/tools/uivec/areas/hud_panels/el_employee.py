"""P41 hud_panels: the Employee panel's three action bubbles (hire, fire, details) on the note box's top edge.
Glyphs read from hex dumps of the idle cells (common.hexdump, 2026-11-24). Registered into el_bubbles.BUILD.
"""
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import common as C  # noqa: E402
import bubble as B  # noqa: E402
import kit as K  # noqa: E402
import el_bubbles as EB  # noqa: E402
from kit import U, line, poly, rect, circle, ellipse, inter  # noqa: E402

MID_EDGE = {"employee-hire": (24, 19), "employee-fire": (21, 21), "employee-resume": (19, 26)}   # the note box's dark top
# line, row in the cell left / right of the bubble (read on the outer columns: light line above it, gradient to white below)


def ground_mid(el, rct, c, key):
    """The note box under the three action bubbles: a lavender plate, then the box's top edge (a light line over a dark line)
    and the box's white inside with a soft inner shadow under that edge. The edge is lower on one side of the hire and
    details bubbles (the box top steps up between them, see the Employee tray); the step is hidden under the bubble."""
    x0, y0, w, h = rct
    el_, er_ = MID_EDGE[el]
    L = []
    lav = K.sample(key, rect(x0, y0 + 6, x0 + 3, y0 + 12))
    L.append(K.lay(rect(x0, y0, x0 + w, y0 + h), lav))
    white = K.sample(key, rect(x0, y0 + h - 4, x0 + 3, y0 + h))
    edges = MID_LINES.get(el, {"l": [(0, el_), (26, el_)], "r": [(26, er_), (w, er_)]})
    for side in ("l", "r"):
        pts = [(x0 + a, y0 + b + 1) for a, b in edges[side]]
        L.append(K.lay(poly(*(pts + [(pts[-1][0], y0 + h), (pts[0][0], y0 + h)])), white))
    if el == "employee-fire":       # the fire cell's top rows are the light lower edge of the plate above
        L.append(K.lay(rect(x0, y0, x0 + w, y0 + 3), {"linear": {"a": [0, y0 + 0.5], "b": [0, y0 + 2.5], "stops": [
            [0.0, K.sample(key, rect(x0, y0, x0 + 3, y0 + 1))], [0.5, K.sample(key, rect(x0, y0 + 1, x0 + 3, y0 + 2))],
            [1.0, K.sample(key, rect(x0, y0 + 2, x0 + 3, y0 + 3))]]}}))
    else:
        L.append(K.lay(line((x0, y0 + 0.5), (x0 + w, y0 + 0.5), w=1.0), K.sample(key, rect(x0, y0, x0 + 3, y0 + 1))))
    for side, e in (("l", el_), ("r", er_)):
        ey = y0 + e
        col = (x0, x0 + 2) if side == "l" else (x0 + w - 2, x0 + w)
        S = lambda r: K.sample(key, rect(col[0], r, col[1], r + 1))  # noqa: E731
        stops = [[k / 10.0, S(ey + 1 + k)] for k in range(0, 11)]
        pts = [(x0 + a, y0 + b) for a, b in edges[side]]
        my = float(np.mean([p[1] for p in pts]))
        region = poly(*([(p[0], p[1] + 1) for p in pts] + [(pts[-1][0], y0 + h), (pts[0][0], y0 + h)]))
        L.append(K.lay(region, {"linear": {"a": [0, my + 1.5], "b": [0, my + 11.5], "stops": stops}}))
        for off, r in ((-1.5, ey - 2), (-0.5, ey - 1), (0.5, ey)):
            L.append(K.lay(line(*[(p[0], p[1] + off) for p in pts], w=1.0), S(r)))
    return L, rect(x0, y0, x0 + w, y0 + h)


# the edge as a polyline where it is not level: (x in the cell, row of the dark line); the details cell's right side runs
# down to the right (darkest rows per column, x 437..451: 124, 124, 124.6, 125.6, 126.8)
MID_LINES = {"employee-resume": {"l": [(0, 19), (26, 19)], "r": [(23, 24.0), (44, 24.6), (48, 25.6), (52, 26.8)]}}


def glyph_hire():
    """A navy running man pulling a green arrow (E 400,0): head round (416.8, 9.8) r 1.8; body leaning from the neck
    (416.6, 12.6) to the hip (417.2, 20.4), 3 px wide; back arm from the shoulder down to a hand at (412, 17.5); front arm
    reaching right to (421.8, 14.6); legs: back one bent down-left to the foot (411.6, 26.8), front one down to (419.2, 25)
    then the foot along y 26.6 to x 422.4. The arrow points right: a triangle with its upright base at x 425 (y 4.6 to
    23.6) and its tip at (432, 15), dark-green outline, a light streak inside along the base, and a short shaft x 421.5..425
    at y 12 that the hand holds."""
    return [
        ("arrow shaft", rect(421.4, 11.2, 425.6, 13.0), ("sample_in", rect(422, 12, 425, 13))),
        ("arrow outline", poly((424.4, 4.4), (432.6, 15.2), (424.4, 24.2)), ("sample_in", circle(425.5, 22.5, 0.4))),
        ("arrow", poly((425.3, 5.9), (431.7, 15.2), (425.3, 22.9)), ("sample_in", rect(426.5, 14, 428.5, 17))),
        ("arrow light", line((426.5, 7.4), (426.5, 12.4), w=1.0), ("sample_in", circle(426.5, 9.5, 0.3))),
        ("head", circle(416.9, 10.2, 1.9), ("sample_in", circle(417, 10.5, 0.6))),
        ("cap", inter(ellipse(417.2, 9.0, 2.2, 1.6), rect(413, 6, 421, 9.0)), ("sample_in", circle(416.0, 7.6, 0.3))),
        ("cap peak", line((414.0, 8.9), (416.4, 8.9), w=0.9), ("sample_in", circle(414.5, 8.5, 0.3))),
        ("body", line((416.4, 13.6), (417.0, 20.6), w=3.8, cap="round"), ("sample_in", circle(417, 17, 0.6))),
        ("back arm", line((415.6, 14.4), (413.4, 16.2), (411.6, 17.4), w=2.0, cap="round"), ("as", "body")),
        ("front arm", line((417.8, 15.2), (419.8, 16.0), (421.8, 14.9), w=1.9, cap="round"), ("as", "body")),
        ("back leg", line((415.8, 20.6), (414.6, 23.2), (412.8, 25.2), (411.6, 26.8), w=2.2, cap="round"), ("as", "body")),
        ("front leg", line((418.0, 20.6), (418.8, 23.4), (419.6, 26.2), w=2.2, cap="round"), ("as", "body")),
        ("front foot", line((419.2, 26.9), (422.4, 27.1), w=1.6, cap="round"), ("as", "body")),
    ]


def thumb_down(dx=0.0, dy=0.0):
    """The thumbs-down hand: fist outline, thumb outline (also used mirrored for the thumbs-up)."""
    fist = poly((419.0, 58.2), (423.2, 57.0), (428.0, 57.2), (430.4, 59.4), (430.6, 66.4), (428.2, 67.6), (419.2, 67.2),
                smooth=1)
    thumb = poly((419.4, 66.0), (425.6, 66.2), (425.8, 73.6), (425.2, 77.0), (423.6, 76.4), (421.0, 71.0), (419.6, 68.4),
                 smooth=1)
    return fist, thumb


def glyph_fire():
    """A red thumbs-down (E 400,50): a green cuff (x 417..419, y 57.5..65.5), the fist (x 418.6..430.4, y 57..67.6) with a
    darker left side and three dark finger lines across its right part (y 60.5, 62.9, 65.4, x 424..430.4), and the thumb
    hanging from its lower left down to a point at (424.6, 76.8)."""
    fist, thumb = thumb_down()
    return [
        ("cuff", rect(416.9, 57.6, 419.1, 65.6), ("sample_in", rect(417, 59, 418, 62))),
        ("fist", fist, ("sample_in", rect(421, 59, 423, 62))),
        ("thumb", thumb, ("as", "fist")),
        ("fist shade", line((419.6, 59.0), (419.6, 67.0), w=1.1), ("sample_in", rect(419, 60, 420, 63))),
        ("thumb shade", line((420.4, 67.6), (421.6, 71.6), (423.4, 75.6), w=0.9), ("sample_in", circle(420.5, 69.5, 0.3))),
        ("finger line 1", line((424.0, 60.5), (430.4, 60.5), w=1.0), ("sample_in", circle(425.5, 60.5, 0.3))),
        ("finger line 2", line((425.0, 62.9), (430.4, 62.9), w=1.6), ("sample_in", circle(427.5, 62.5, 0.3))),
        ("finger line 3", line((425.0, 65.4), (430.4, 65.4), w=1.0), ("as", "finger line 1")),
    ]


def glyph_resume():
    """A purple figure holding up a clipboard (E 400,100). Figure: hat (crown x 417.5..420, y 105.8..107.6, brim x 416..421
    at y 107.5), head round (418.6, 109.2) r 1.7, body x 415..420 from the shoulders (y 112) to the hip (y 120.4), legs
    x 417.6 and 420.5 down to y 126.6, a foot line y 127.5 x 416..423. Clipboard: white board x 421..433, y 111.3..120.7
    with a dark olive edge, three dark text lines (y 113.5, 116.5, 118.5) and two light grey ones (115.5, 119.5); a cyan
    clip on top (a dome round (426.6, 111.4), 3 px high) with a dark edge."""
    clip = inter(ellipse(426.6, 111.6, 3.0, 3.4), rect(422, 106, 431, 111.6))
    return [
        ("board edge", rect(420.4, 111.0, 433.8, 121.0, 0.4), ("sample_in", rect(433, 113, 434, 118))),
        ("board", rect(421.2, 112.0, 432.9, 120.0), ("sample_in", rect(422, 114, 423, 115))),
        ("text 1", line((423.2, 113.5), (431.8, 113.5), w=1.0), ("sample_in", rect(424, 113, 430, 114))),
        ("text 2", line((423.2, 116.5), (432.0, 116.5), w=1.0), ("as", "text 1")),
        ("text 3", line((423.2, 118.5), (432.0, 118.5), w=1.0), ("as", "text 1")),
        ("grey 1", line((423.4, 115.5), (426.6, 115.5), w=1.0), ("sample_in", rect(424, 115, 426, 116))),
        ("grey 2", line((423.6, 119.5), (429.6, 119.5), w=1.0), ("as", "grey 1")),
        ("clip edge", clip, ("sample_in", circle(424.4, 109.5, 0.3))),
        ("clip", inter(ellipse(426.6, 111.8, 2.0, 2.6), rect(422, 106, 431, 111.2)), ("sample_in", rect(426, 109, 428, 110))),
        ("hat", rect(417.4, 105.8, 420.2, 107.8, 0.6), ("sample_in", circle(418.5, 106.5, 0.4))),
        ("brim", line((415.8, 107.6), (421.0, 107.4), w=1.0), ("sample_in", circle(420.5, 107.5, 0.3))),
        ("head", circle(418.7, 109.2, 1.7), ("sample_in", circle(418.5, 109, 0.5))),
        ("body", rect(415.0, 112.0, 420.0, 120.4, 1.0), ("sample_in", rect(417, 114, 419, 117))),
        ("shoulder light", line((416.2, 112.5), (419.2, 112.5), w=1.0), ("sample_in", circle(416.5, 112.5, 0.3))),
        ("leg 1", line((417.6, 120.0), (417.6, 126.8), w=1.8), ("as", "body")),
        ("leg 2", line((420.5, 120.0), (420.6, 126.8), w=1.6), ("as", "body")),
        ("feet", line((416.0, 127.5), (423.0, 127.5), w=1.0), ("as", "body")),
    ]


GLYPHS = {"employee-hire": glyph_hire, "employee-fire": glyph_fire, "employee-resume": glyph_resume}


def build_mid(el):
    scenes = {}
    for st, key, rct in C.CELLS[el]:
        g = EB.GEOM[el][st]
        dx, dy = EB.offset(el, st)
        ground, gshape = ground_mid(el, rct, g["c"], key)
        lay = EB.glyph_layers(GLYPHS[el](), key, dx, dy)
        EB.GU[(el, st)] = U(*[L["shape"] for L in lay])
        fam = g["family"]
        it = B.item(g["c"], g["body"], EB.PAINT[fam], glyph=lay, shadow=EB.SHADOWS.get(fam), ground=gshape)
        scenes[st] = [K.item(ground, box=[rct[0], rct[1], rct[0] + rct[2], rct[1] + rct[3]]), it]
    return scenes


def ground_profile(x, y):
    """The art's colour at (x, y) without the bubble: the outer column of the same side, same row (E sheet)."""
    rgb, _ = C.sheet("E")
    cx0 = (np.floor(x / 100) * 100).astype(int)
    col = np.where((x - cx0) < 26, cx0, cx0 + 51)
    return rgb[np.floor(y).astype(int), col]


def fit_shadows():
    out = {}
    for st in ("idle", "selected", "disabled", "hover"):
        insts = []
        for el in MID_EDGE:
            _, key, rct = [c for c in C.CELLS[el] if c[0] == st][0]
            # the ground without the shadow: the plate and the box as the outer columns show them, row by row
            ok = (lambda r: (lambda x, y: (y > r[1] + 1) & (x > r[0] + 2) & (x < r[0] + r[2] - 2)))(rct)
            insts.append((key, EB.GEOM[el][st]["c"], ok))
        out["E:mid:" + st] = B.fit_shadow(insts, ground_profile)
        print("shadow E:mid:" + st, out["E:mid:" + st])
    return out


EB.BUILD.update({k: build_mid for k in MID_EDGE})
EB.SHADOW_FITS.append(fit_shadows)
