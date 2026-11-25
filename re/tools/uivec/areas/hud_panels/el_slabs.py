"""P41 hud_panels: the Golfer info panel's two slabs (the info panel and the story band under it).

A slab is a lavender rounded plate whose edges are lit: lighter along the top and left, darker along the right and bottom,
and darker again round the sunken text hole on its upper left side. All of that is ONE measured lit surface: the sdf bevel
paint of the plate-minus-hole shape, its coefficients solved by least squares on the slab's own pixels (accept.solve_bevel,
the boxes on it left out). What sits on the slab is authored: the arched title plate (one arc read off its line's rows),
the tan box, the cream box with its sunken shading (boxes.py measured it), the 2 x 18 score cells plus the wide one (their
navy lines read on rows 143 / column 250) and the hole (transparent in the art, left empty here too).
"""
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import common as C  # noqa: E402
import kit as K  # noqa: E402
import el_bubbles as EB  # noqa: E402
import sdf  # noqa: E402
sys.path.insert(0, C.UIVEC)
import accept  # noqa: E402
import boxes  # noqa: E402
from kit import U, line, curve, poly, rect, circle, ellipse, inter, sub, outline  # noqa: E402,F401

SLAB_STOPS = [0.0, 0.17, 0.34, 0.5, 0.67, 0.84, 1.0]


def fit_bevel(key, shape, box, exclude=None, L=6.0, stops=SLAB_STOPS, of=None):
    """The lit-surface paint of `shape` measured on the art: bevel coefficients over the pixels inside it (opaque, deeper
    than 0.4 px, away from `exclude` grown 1 px)."""
    rgb, m = C.sheet(key)
    x0, y0, x1, y1 = (int(v) for v in box)
    jj, ii = np.mgrid[y0:y1, x0:x1]
    x, y = ii + 0.5, jj + 0.5
    lit = of or shape      # the surface whose edges are lit (a plate cut by the cell is lit by its own edges only)
    d = sdf.sd(lit, x, y)
    nx, ny = sdf.normal(lit, x, y, d)
    ok = (d < -0.4) & m[y0:y1, x0:x1] & (sdf.sd(shape, x, y) < -0.4)
    if exclude is not None:
        ok &= sdf.sd(exclude, x, y) > 1.0
    near = ok & (-d <= L * 1.05)
    rows = [((-d / L)[near], nx[near], ny[near], rgb[y0:y1, x0:x1][near])]
    coef, n = accept.solve_bevel(rows, stops)
    # beyond the bevel the face is flat: the last stop has no direction terms, its colour the deep pixels' median
    deep = ok & (-d > L * 1.05)
    if deep.sum() > 20:
        coef[-1] = [[round(float(v), 2) for v in np.median(rgb[y0:y1, x0:x1][deep], axis=0)]] + [[0.0, 0.0, 0.0]] * 4
    out = {"bevel": {"L": L, "stops": stops, "coef": coef}}
    if of is not None:
        out["bevel"]["of"] = of
    return out


def found_boxes(key, rct):
    x0, y0, w, h = rct
    rgb, _ = C.sheet(key)
    bx, _ = boxes.measure(rgb[y0:y0 + h, x0:x0 + w].astype(np.uint8))
    out = []
    for e in bx:
        e = dict(e)
        e["box"] = [e["box"][0] + x0, e["box"][1] + y0, e["box"][2] + x0, e["box"][3] + y0]
        out.append(e)
    return out


def score_cells(key, x_lines, top, mid, bot, wide):
    """The score cells: a navy block, the cells cut out of it as cream rounded boxes, each sunken (a darker band along its
    top and left inside edge, sampled on the art)."""
    L = []
    xs = list(x_lines)
    navy = K.sample(key, rect(xs[0], top, xs[0] + 2, bot), depth=0.0)
    L.append(K.lay(rect(xs[0], top, wide[1] + 2, bot + 2), navy))
    cream = K.sample(key, rect(xs[0] + 6, top + 8, xs[0] + 12, mid - 1), depth=0.0)
    shade1 = K.sample(key, rect(xs[0] + 2, top + 5, xs[0] + 3, top + 6), depth=0.0)
    shade2 = K.sample(key, rect(xs[0] + 3, top + 5, xs[0] + 4, top + 6), depth=0.0)
    spans = [(a + 2, b) for a, b in zip(xs, xs[1:])] + [(wide[0], wide[1])]
    for a, b in spans:
        for ya, yb in ((top + 2, mid), (mid + 1, bot)):
            L.append(K.lay(rect(a, ya, b, yb, 1.0), shade1))
            L.append(K.lay(rect(a + 1, ya + 1, b, yb, 1.0), shade2))
            L.append(K.lay(rect(a + 2, ya + 2.4, b, yb, 0.8), cream))
    return L


def build_info(el):
    st, key, rct = C.CELLS[el][0]
    x0, y0, w, h = rct
    plate = rect(x0 + 0.5, y0 + 0.5, x0 + w - 0.5, y0 + h - 0.5, 6.0)
    hole = rect(73.0, 175.0, 457.0, 251.0, 7.0)
    surf = sub(plate, hole)
    # what sits on the slab
    R = 515.9
    arc = circle(244.0, 35.8 + R, R)
    title_outer = inter(arc, rect(119.0, 30.0, 370.0, 135.0))
    title_cream = inter(circle(244.0, 35.8 + R, R - 1.8), rect(121.0, 30.0, 368.0, 66.0))
    tan = rect(121.0, 67.0, 368.0, 133.0)
    x_lines = list(range(84, 409, 18))
    cells = rect(84.0, 134.0, 446.0, 172.0)
    # the cream box at the right: a navy line (top y 46, bottom y 133..135, sides x 369..370 and 458..459, corners r 6)
    # round a sunken cream inside (its shading measured as a lit surface of its own)
    cbox = rect(368.8, 45.6, 459.6, 134.8, 6.0)
    cin = rect(370.6, 46.8, 457.8, 133.0, 4.8)
    on = U(title_outer, cells, cbox)
    paint = fit_bevel(key, surf, [x0, y0, x0 + w, y0 + h], exclude=on)
    navy = K.sample(key, rect(240, 35, 248, 37), depth=0.0)
    L = [K.lay(surf, paint)]
    L.append(K.lay(outline(hole, 1.2), K.sample(key, rect(240, 172, 260, 175), depth=0.0)))
    L.append(K.lay(title_outer, navy))
    L.append(K.lay(title_cream, {"linear": {"a": [0, 37.0], "b": [0, 43.0], "stops": [
        [k / 6.0, K.sample(key, rect(248, 37 + k, 252, 38 + k), depth=0.0)] for k in range(7)]}}))
    L.append(K.lay(tan, {"linear": {"a": [121.0, 0], "b": [127.0, 0], "stops": [
        [k / 6.0, K.sample(key, rect(121 + k, 90, 122 + k, 110), depth=0.0)] for k in range(7)]}}))
    L.append(K.lay(line((121.0, 66.5), (368.0, 66.5), w=1.0), K.sample(key, rect(240, 66, 260, 67), depth=0.0)))
    L += score_cells(key, x_lines, 134, 150, 170, (412, 443))
    L.append(K.lay(cbox, K.sample(key, rect(400, 133, 420, 135), depth=0.0)))
    L.append(K.lay(cin, fit_bevel(key, cin, [368, 45, 460, 136], L=5.0)))
    return {st: [K.item(L, box=[x0, y0, x0 + w, y0 + h])]}


def build_story(el):
    st, key, rct = C.CELLS[el][0]
    x0, y0, w, h = rct
    plate = rect(x0 + 0.5, y0 + 0.5, x0 + w - 0.5, y0 + h - 0.5, 6.0)
    bx = found_boxes(key, rct)
    on = U(*[rect(e["box"][0] - 3, e["box"][1] - 3, e["box"][2] + 3, e["box"][3] + 3) for e in bx])
    L = [K.lay(plate, fit_bevel(key, plate, [x0, y0, x0 + w, y0 + h], exclude=on))]
    for e in bx:
        L += boxes.item(e)["layers"]
    return {st: [K.item(L, box=[x0, y0, x0 + w, y0 + h])]}


EB.BUILD.update({"golfer-info-panel": build_info, "golfer-story-band": build_story})
