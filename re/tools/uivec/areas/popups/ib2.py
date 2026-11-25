"""P41 popups fix round: the info buttons rebuilt with the lit disc body (disc2.py) and the fix-round glyphs (glyphs2.py,
falling back to the first round's glyphs_ib.py). Writes log/roadmap/P41/work2/popups/info-button-<key>.png / .json.

py -3.12 ib2.py [key ...]
"""
import json
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import w2               # noqa: E402
import common as c      # noqa: E402
import infobuttons as ib  # noqa: E402
import disc2            # noqa: E402
import glyphs_ib        # noqa: E402
import glyphs2          # noqa: E402

FIX_KEYS = ("flag", "globe", "money", "pin", "tiles", "trophy")
with open(disc2.STYLE_PATH) as _f:
    DSTYLE = json.load(_f)


def glyph_layers(k):
    fn = glyphs2.GLYPHS.get(k) or glyphs_ib.GLYPHS[k]
    return fn()


def base(r, col, disc, state):
    box = ib.cell(r, col)
    cx, cy, R = disc
    Ls = [ib.tab_layer(box)]
    if state == "hover":
        s = ib.SHADOW
        Ls.append({"shape": c.circ(cx + s[0], cy + s[1], R + s[2]), "fill": [40, 40, 80], "alpha": s[3], "soft": s[4],
                   "clip": c.rr(box[0], box[1] + ib.TAB_Y0, box[2], box[1] + ib.TAB_Y1, 0.0)})
    rim = RIM[state]
    Ls.append({"shape": c.circ(cx, cy, R), "fill": rim})
    Ls += disc2.layers(cx, cy, R, DSTYLE[state])
    return c.item(box, Ls), c.circ(cx, cy, R - disc2.RIM_W)


RIM = {"idle": [44, 48, 88], "hover": [44, 48, 88], "selected": [44, 48, 88]}


def build(k, z=8):
    r = ib.KEYS.index(k)
    rows, scenes, stats = [], {}, []
    disc0 = ib.measure_disc(r, 0)
    g0 = [ib.mv_layer(S, col_, kw, 0.0, 50 * r) for S, col_, kw in glyph_layers(k)]
    for col, st in enumerate(ib.STATES):
        box = ib.cell(r, col)
        disc = ib.measure_disc(r, col)
        dx, dy = disc[0] - disc0[0], disc[1] - disc0[1]
        it, body = base(r, col, disc, st)
        for S, col_, kw in g0:
            S2, f2, kw2 = ib.mv_layer(S, col_, kw, dx, dy)
            Lr = {"shape": S2, "fill": f2 if isinstance(f2, dict) else list(f2), "clip": body}
            Lr.update(kw2)
            it["layers"].append(Lr)
        it["name"] = f"info-button-{k} {st}"
        scene = [it]
        mad, w16 = c.score(scene, ib.SHEET, box)
        stats.append((st, mad))
        scenes[st] = scene
        rows.append((scene, ib.SHEET, box, c.FLAT))
    p = c.compare(rows, f"info-button-{k}", zoom_k=z)
    c.save_scene(f"info-button-{k}", {"id": f"info-button-{k}", "sheet": "Interface/" + ib.SHEET,
                                      "states": {st: {"box": list(ib.cell(r, i)), "scene": scenes[st]}
                                                 for i, st in enumerate(ib.STATES)}})
    return p, stats


if __name__ == "__main__":
    keys = sys.argv[1:] or list(FIX_KEYS)
    for i, k in enumerate(keys):
        p, st = build(k)
        print(f"[{i + 1}/{len(keys)}] info-button-{k} " + " ".join(f"{s} {m:.2f}" for s, m in st) + f"  {p}")
