"""P41 hud_panels: the Golfer action panel's match scoreboard plate (J 297,300; 503 x 107).

Read off its navy runs (row 340, columns 600 / 780) and the outlines of its bars (2026-11-24): a lavender plate whose
top-left corner is rounded (r 13) and whose edge is lit (one measured lit surface, el_slabs.fit_bevel); two white glossy
bars (outlines y 356..370 and 377..391, x 307..433) with the ball knob over their left ends (round (307, 373) r 8); a navy
grid (left border x 434..436, 18 cells 18 px apart, a double line at 759..762, a wide cell to 794, the right border from
797; rows split at y 336, 352, 374, top 320, foot 396..398) of cream cells, each sunken (a darker band inside its top and
left edge).
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
from kit import U, line, rect, circle, inter  # noqa: E402,F401


def grid_layers(key, xspans, yspans, block):
    """A navy block with sunken cream cells cut into it (spans = the cells' inside extents in x and in y)."""
    L = [K.lay(rect(*block), K.sample(key, rect(block[0], block[1] + 8, block[0] + 2, block[1] + 14), depth=0.0))]
    a0, b0 = xspans[1]
    c0, d0 = yspans[1]
    cream = K.sample(key, rect(a0 + 5, c0 + 6, b0 - 2, d0 - 2), depth=0.0)
    s1 = K.sample(key, rect(a0 + 6, c0, a0 + 8, c0 + 1), depth=0.0)
    s2 = K.sample(key, rect(a0 + 6, c0 + 1, a0 + 8, c0 + 2), depth=0.0)
    for a, b in xspans:
        for c, d in yspans:
            L += [K.lay(rect(a, c, b, d, 1.2), s1), K.lay(rect(a + 0.8, c + 1, b, d, 1.0), s2),
                  K.lay(rect(a + 1.6, c + 2, b, d, 0.8), cream)]
    return L


def build_plate(el):
    st, key, rct = C.CELLS[el][0]
    x0, y0, w, h = rct
    rgb, m = C.sheet(key)
    body = rect(297.0, 301.0, 830.0, 407.0, 13.0)          # runs on past the cell's right end
    plate = inter(body, rect(x0, y0, x0 + w, y0 + h))
    xl = [436] + list(range(454, 761, 18))
    xspans = [(a, b - 1) for a, b in zip(xl, xl[1:])] + [(762, 794)]
    xspans[0] = (436, 453)
    yspans = [(321, 336), (337, 352), (353, 374), (375, 396)]
    block = (434.0, 320.0, 800.0, 398.0)
    bars = U(rect(306.0, 355.0, 434.0, 372.0), rect(306.0, 376.0, 434.0, 393.0), circle(307.0, 373.0, 9.0))
    L = [K.lay(plate, S.fit_bevel(key, plate, [x0, y0, x0 + w, y0 + h], exclude=U(rect(*block), bars), L=8.0, of=body))]
    L += grid_layers(key, xspans, yspans, block)
    L.append(K.lay(rect(795.0, 321.0, 797.0, 396.0), K.sample(key, rect(795, 330, 797, 340), depth=0.0)))
    L += M.bar_layers(key, 306.6, 355.6, 433.6, 371.0, 3.0)
    L += M.bar_layers(key, 306.6, 376.6, 433.6, 392.0, 3.0)
    L += M.ball_layers(key, 307.0, 373.0, 8.0)
    return {st: [K.item(L, box=[x0, y0, x0 + w, y0 + h])]}


EB.BUILD["match-plate"] = build_plate
