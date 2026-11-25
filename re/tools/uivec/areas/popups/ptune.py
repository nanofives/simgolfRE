"""P41 popups area: the numeric fit that only TUNES a reading (rule 5 of the brief).

tune(layers, under, art, box, mask): coordinate descent over every number of the given layers' shapes (glyphs.numbers rules:
parts marked "fixed" are left alone; widths never under 0.4), each kept within `reach` px of what was written, scored as the
mean squared colour error at 1x over `mask` with the layers composited over `under` (the item's other layers, rendered once).
Returns (first loss, best loss, largest move).
"""
import copy

import numpy as np

import common as c
sdf = c.sdf
sys_path_glyphs = c.UIVEC
import glyphs as tk_glyphs  # noqa: E402  (the toolkit's numbers(): which leaves of a shape tree are fittable)


def tune(layers, under, art, box, mask, s=4, passes=6, reach=0.7, step=0.25):
    x0, y0, x1, y1 = box
    h, w = y1 - y0, x1 - x0
    params = []
    for L in layers:
        tk_glyphs.numbers(L["shape"], params)
    start = [p[0][p[1]] for p in params]
    item = {"box": list(box), "layers": layers}

    def loss():
        img = under.copy()
        sdf.render([item], img, s, origin=(x0, y0))
        low = img.reshape(h, s, w, s, 3).mean(axis=(1, 3))
        d = (low - art)[mask]
        return float((d * d).mean())

    first = best = loss()
    st = [step] * len(params)
    for _ in range(passes):
        moved = 0
        for k, p in enumerate(params):
            v0 = p[0][p[1]]
            for sign in (1, -1):
                v = round(v0 + sign * st[k], 3)
                if abs(v - start[k]) > reach or (p[3] is not None and v < p[3]):
                    continue
                p[0][p[1]] = v
                e = loss()
                if e < best - 1e-9:
                    best, moved, v0 = e, moved + 1, v
                    break
                p[0][p[1]] = v0
        if moved == 0:
            st = [q * 0.5 for q in st]
            if st[0] < 0.05:
                break
    mv = max((abs(p[0][p[1]] - s0) for p, s0 in zip(params, start)), default=0.0)
    return first, best, mv


def under_image(item_layers, box, s, ground):
    img = c.render_box([], box, s, ground)
    sdf.render([{"box": list(box), "layers": item_layers}], img, s, origin=box[:2])
    return img
