"""P41 hud_panels round 3 (2026-11-24): match-plate, Mariano's note "Rounded edges".

Re-read at 14x against the round-2 scene (fix3.side). The round-2 plate's silhouette was wrong where it matters for
"edges": its strip met the plate in a straight diagonal plus a convex arc (a kink), its right-hand corners were rounded in
the silhouette with a hard diagonal seam in the bevel, its two bars had lost their navy outline (sampled half a pixel
off, so the outline took the plate colour) and its grid cells had rounded corners the art does not have.

What the art is (sheet J, luminance dumps):
- silhouette: one rectangle x 297..800, rows 301..407 with a CONCAVE cove cut out of its top left: the edge runs straight
  down x 309 to row 303 and then sweeps out to x 297 at row 319 (circle fitted to the 20 edge pixels: (292.62, 302.79)
  r 16.45, residual 0.24 px). The right-hand corners are square in the silhouette; they read rounded because the lit top
  edge rolls over into the dark right edge (rows 301-304 x 796-799: 200 191 177 / 191 177 144 / 177 151 108 / 151 108 80).
- strip (rows 301..312): body 127 at x 310-360 brightening to 168 at x 790 (rows 306-312); its top four rows lit
  (215 215 200 177 at x 790, 200 191 175 151 at x 330); its right four columns dark (152 119 99 68).
- plate (from row 313): body 112; a rounded edge on three sides: top rows 191 183 159 128, left columns 144 144 136 120,
  right columns 104 78 58 34; its top left corner a convex r 13 corner (the lit band runs round it, 215 where it faces up
  left) inside the cove, the strip colour filling the sliver between the two.
- grid: navy separators (one dark column plus a 99 half-tone column on its left), cells x 436 + 18k (16 wide), a triple
  line 758..761, a wide cell 762..792, border 793..794; rows 321..335, 337..351, 353..373, 375..395; navy foot rows
  396..397; a dark row 320 and a dithered row 319 above the cells; each cell square-cornered and sunken (top 122 188 205
  232 246, left 188 215 232 246, then cream 248).
- bars: navy one-pixel outlines x 307..433, rows 356..370 and 377..391, left corners r 2.5, right corners r 3.5; white
  inside fading to 188 at the foot; one shadow row under each (83); the knob a navy ring round (307.0, 373.5) r 8 over
  both left ends.

Paint: the strip and the plate are each ONE fitted lit surface (el_slabs.fit_bevel / accept.solve_bevel) whose edges are a
rounded box (r 4 at the rolled corners), so the corner shading turns smoothly like the art's instead of meeting in a
seam. The strip's left-to-right brightening is taken out before the fit and put back as one linear tone.
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
import el_flat2 as F2  # noqa: E402
import figs  # noqa: E402
import fit  # noqa: E402
import sdf  # noqa: E402
import accept  # noqa: E402
from kit import U, rect, circle, sub, inter  # noqa: E402

COVE = (292.62, 302.79, 16.45)
PX4 = [0.125, 0.375, 0.625, 0.875, 1.0]          # one stop per pixel column of a 4 px edge (L = 4)
PX5 = [0.1, 0.3, 0.5, 0.7, 0.9, 1.0]


def silhouette():
    cx, cy, r = COVE
    return sub(rect(297.0, 301.0, 800.0, 407.0), U(circle(cx, cy, r), rect(280.0, 290.0, 309.0, cy)))


def strip_paint(key, shape, of, box, L=4.0, stops=tuple(PX4)):
    """The strip's lit surface: a bevel fitted on the art after removing its linear left-to-right brightening, plus that
    brightening as a tone fx. Returns (paint, fx)."""
    rgb, m = C.sheet(key)
    x0, y0, x1, y1 = box
    jj, ii = np.mgrid[y0:y1, x0:x1]
    x, y = ii + 0.5, jj + 0.5
    lum = fit.lum(rgb)
    # the trend: rows 306-312 (below the lit top, above the plate), x 330..790
    xs = np.arange(330, 791, 10)
    ys = np.array([lum[306:313, xx - 1:xx + 2].mean() for xx in xs])
    k, c0 = np.polyfit(xs, ys, 1)
    trend = np.where(x < 330, k * 330 + c0, np.where(x > 790, k * 790 + c0, k * x + c0))
    mid = k * 560 + c0
    d = sdf.sd(of, x, y)
    nx, ny = sdf.normal(of, x, y, d)
    ok = (sdf.sd(shape, x, y) < -0.4) & m[y0:y1, x0:x1] & (d < -0.3)
    a = rgb[y0:y1, x0:x1] - (trend - mid)[..., None]
    near = ok & (-d <= L * 1.05)
    coef, _ = accept.solve_bevel([((-d / L)[near], nx[near], ny[near], a[near])], list(stops))
    deep = ok & (-d > L * 1.05)
    coef[-1] = [[round(float(v), 2) for v in np.median(a[deep], axis=0)]] + [[0.0, 0.0, 0.0]] * 4
    paint = {"bevel": {"L": L, "stops": list(stops), "coef": coef, "of": of}}
    half = float(k * (790 - 330) / 2)
    fx = [{"t": "tone", "a": [790.0, 0.0], "b": [330.0, 0.0], "add": round(half, 2)}]
    return paint, fx


def grid_layers(key):
    rgb, m = C.sheet(key)
    xs = [436 + 18 * i for i in range(18)]            # 16-wide cells 436..451, 454..469, ... 742..757
    xspans = [(a, a + 16) for a in xs] + [(762, 793)]
    yspans = [(321, 336), (337, 352), (353, 374), (375, 396)]
    navy = K.sample(key, rect(453, 325, 454, 333), depth=0.0)
    half = K.sample(key, rect(452, 325, 453, 333), depth=0.0)
    L = [K.lay(rect(434.0, 320.0, 795.0, 398.0), navy),
         K.lay(rect(436.0, 320.0, 793.0, 321.0), K.sample(key, rect(440, 320, 448, 321), depth=0.0)),   # the dark row
         K.lay(rect(436.0, 319.0, 793.0, 320.0), K.sample(key, rect(440, 319, 448, 320), depth=0.0)),   # the dither
         K.lay(rect(434.0, 396.0, 795.0, 397.0), K.sample(key, rect(440, 396, 448, 397), depth=0.0)),
         K.lay(rect(435.0, 321.0, 436.0, 396.0), K.sample(key, rect(435, 326, 436, 334), depth=0.0))]
    # the half-tone column left of every separator (cell's right neighbour column)
    for a, b in xspans:
        L.append(K.lay(rect(b, 321.0, b + 1, 396.0), half))
    L.append(K.lay(rect(759.0, 321.0, 762.0, 396.0), K.sample(key, rect(759, 325, 760, 333), depth=0.0)))
    L.append(K.lay(rect(760.0, 321.0, 761.0, 396.0), K.sample(key, rect(760, 325, 761, 333), depth=0.0)))
    L.append(K.lay(rect(761.0, 321.0, 762.0, 396.0), K.sample(key, rect(761, 325, 762, 333), depth=0.0)))
    # every cell sunken, its own pixels measured: cream face, a 4 px shadow inside its left edge (one colour per column,
    # sampled on the cell's middle row) and a 5 px shadow inside its top edge (one colour per row, sampled on the cell's
    # column a + 8). The top row of cells lies under the plate's edge: its top shadow is deep (122 188 205 232 246) and
    # darkest in the corner (99 at the corner pixel, a radial patch); the lower rows' top shadow is light (221 215 232 246).
    px = lambda x, y: [round(float(v), 1) for v in rgb[y, x]]     # noqa: E731
    for a, b in xspans:
        for c, d in yspans:
            ym = (c + d) // 2
            left = K.lay(rect(a, c, a + 4, d), {"linear": {"a": [a, 0.0], "b": [a + 4.0, 0.0],
                                                           "stops": [[(i + .5) / 4, px(a + i, ym)] for i in range(4)]}})
            top = K.lay(rect(a, c, b, c + 5), {"linear": {"a": [0.0, c], "b": [0.0, c + 5.0],
                                                          "stops": [[(i + .5) / 5, px(a + 8, c + i)] for i in range(5)]}})
            L.append(K.lay(rect(a, c, b, d), px(a + 9, ym)))
            L += [left, top] if c == 321 else [top, left]
            if c == 321:
                L.append(K.lay(inter(circle(a, c, 3.6), rect(a, c, b, d)),
                               {"radial": {"c": [a, c], "r": 3.6, "stops": [[0.2, px(a, c)], [0.6, px(a + 1, c + 1)],
                                                                            [1.0, px(a + 2, c + 2)]]}}))
    return L


def bar(key, x0, y0, x1, y1):
    """A white glossy bar: navy outline box [x0, x1) x [y0, y1) (pixel edges), corners r 2.5 left, 3.5 right."""
    outer = figs.rrect4(x0, y0, x1, y1, 2.5, 3.5, 3.5, 2.5)
    inner = figs.rrect4(x0 + 1, y0 + 1, x1 - 1, y1 - 1, 1.5, 2.5, 2.5, 1.5)
    navy = K.sample(key, rect(x0 + 20, y0, x1 - 20, y0 + 1), depth=0.0)
    shadow = K.sample(key, rect(x0 + 30, y1, x1 - 20, y1 + 1), depth=0.0)
    xm = int((x0 + x1) / 2)
    L = [K.lay(rect(x0 + 6, y1, x1 - 2, y1 + 1), shadow),
         K.lay(outer, navy),
         K.lay(inner, M.vgrad(key, xm, int(y0) + 1, int(y1) - 1))]
    rgb, _ = C.sheet(key)
    ym = int((y0 + y1) / 2)
    D = float(np.mean(rgb[ym - 2:ym + 3, xm]) - np.mean(rgb[ym - 2:ym + 3, int(x1) - 2]))
    if D > 4:
        end = inter(inner, rect(x1 - 9.0, y0, x1, y1))
        L.append({"shape": end, "fill": M.vgrad(key, xm, int(y0) + 1, int(y1) - 1),
                  "fx": [{"t": "tone", "a": [x1 - 17.0, ym], "b": [x1 - 1.0, ym], "add": round(D / 2, 1)}]})
    return L


def build_plate(el):
    st, key, rct = C.CELLS[el][0]
    x0, y0, w, h = rct
    box = [x0, y0, x0 + w, y0 + h]
    sil = silhouette()
    # the plate's silhouette has a square top right corner; its paint's edge is rounded there (r 4) so the lit top edge
    # rolls into the dark right edge as in the art (rows 313-316, x 796-799) instead of meeting in a seam
    plate = inter(figs.rrect4(297.0, 313.0, 800.0, 470.0, 13.0, 0.0, 0.0, 0.0), rect(297.0, 300.0, 800.0, 407.0))
    plate_of = figs.rrect4(297.0, 313.0, 800.0, 470.0, 13.0, 4.0, 0.0, 0.0)
    strip = sub(sil, plate)
    strip_of = figs.rrect4(230.0, 301.0, 800.0, 360.0, 0.0, 4.0, 0.0, 0.0)
    sp, sfx = strip_paint(key, strip, strip_of, [297, 300, 800, 313])
    grid_ex = rect(433.0, 318.0, 800.0, 399.0)
    bars_ex = U(rect(305.0, 355.0, 435.0, 372.0), rect(305.0, 376.0, 435.0, 393.0), circle(307.0, 373.5, 9.0))
    L = [K.lay(strip, sp, fx=sfx),
         K.lay(plate, S.fit_bevel(key, plate, [297, 312, 800, 407], exclude=U(grid_ex, bars_ex), L=4.0,
                                  stops=PX4, of=plate_of))]
    L += grid_layers(key)
    L += bar(key, 307.0, 356.0, 434.0, 371.0)
    L += bar(key, 307.0, 377.0, 434.0, 392.0)
    L += M.ball_layers(key, 307.0, 373.5, 8.0)
    return {st: [K.item(L, box=box)]}


EB.BUILD["match-plate"] = build_plate
