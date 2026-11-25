"""P41W: Theme Packs (Title_ThemePacks), wide. Usage: py -3.12 wide_theme.py [--fit] [--deliver]

Read on the art (2026-11-25):
- the whole screen is flat navy (103, 103, 159) (every sample off the elements is that value): the field is one span rectangle;
- the frame line: 2 px navy with a soft shadow below, from under the title plate at y 44, turning down a very flat arc (its
  circle fitted on the line's pixel centres between y 100 and 500), turning right at the bottom into the OK button;
- the header wires are in the approved theme-header-pills scene, but not their shadow (rows 44-47 under each: 86, 86, 95, 106
  lum on the 109 field): drawn under them, the same shadow as the frame line's;
- the table sits in a recess: dark along its top and left, measured as a shadow family; the approved table, plate, pills,
  scroll bar and buttons (theme-backdrop) on top.
Nothing is anchored left or right: wider screens show more flat navy on both sides of the unchanged composition.
"""
import json
import math
import os
import sys
import time

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import wide_screen as W   # noqa: E402
from wide_screen import fit, sdf, wide   # noqa: E402

SCREEN = "theme"
OUT, CACHE = W.dirs(SCREEN)
SHEET = "Interface/Title_ThemePacks.pcx"
NAVY = [103.0, 103.0, 159.0]
NAVY_LINE = [33, 43, 79]
TOP_Y = 44.0
LIP_RGB = [150.0, 150.0, 200.0]


def art():
    return W.sheet(SHEET)[0]


def refs():
    return {"theme-backdrop": wide.load_ref("theme-backdrop")}


def measure_line():
    """Read 2026-11-25: the line is about 1.6 px wide (x = 20 at y 298: 69, 45 lum, then its shadow 86, 95, 106 to the right;
    y = 547: 83, 41 then 88, 86, 95, 105 below). The arc: circle fitted on the weighted centre of the line's first dark run in
    140 rows between y 90 and 510 (rms 0.18 px); the runs' centres from column profiles."""
    return {"arc": [1228.617, 296.144, 1208.417], "top_y": 44.2, "bot_y": 547.3, "w": 1.6, "rf": 8.0}


def line_shape(geo):
    cx, cy, R = geo["arc"]
    return W.path_hline_circle_hline(80.0, geo["top_y"], (cx, cy), R, geo["bot_y"], 600.0, geo["w"], geo["rf"])


def wire_shapes():
    out = []
    for it in refs()["theme-backdrop"]:
        if it.get("name") == "header wires":
            for L in it["layers"]:
                S = L["shape"]
                if S["t"] == "seg" and S["a"][1] == S["b"][1] and S["w"] <= 1.0:
                    out.append(S)
    return out


def table_casters():
    items = refs()["theme-backdrop"]
    name_col = [it for it in items if it.get("name") == "name column"][0]["layers"][0]["shape"]
    block = {"t": "rrect", "c": [526.5, 298.5], "half": [227.5, 210.5], "r": 2.0}    # the cell grid's outer edge (cells 299..754 x 88..509)
    return [name_col, block]


def fit_all():
    a = art()
    x, y = W.grid()
    geo = measure_line()
    print("line", geo)
    res = {"line": geo}
    items = refs()["theme-backdrop"]
    E = W.coverage(items, 0)
    U = np.zeros((600, 800, 3))
    U[:] = NAVY
    ls = line_shape(geo)
    over = W.overlay(items + [{"box": [-10, -10, 810, 610], "layers": [{"shape": ls, "fill": NAVY_LINE}]}])
    ld = sdf.sd(ls, x, y)
    band = (ld > -1.5) & (ld < 8) & ~W.dilate(E, 3)
    res["line_shadow"] = W.fit_shadow([ls], band, U, a, {"dx": 0.5, "dy": 1.5, "g": 0.0, "soft": 2.0, "alpha": 0.4}, over, name="frame line")
    tc = table_casters()
    d = np.min([sdf.sd(S, x, y) for S in tc], 0)
    others = W.coverage([it for it in items if it["layers"][0]["shape"] not in tc and (it["box"][2] - it["box"][0]) * (it["box"][3] - it["box"][1]) > 400 and not (88 <= it["box"][1] < 509 and 299 <= it["box"][0] < 754)], 2)
    band = (d > -0.5) & (d < 14) & ~others & (ld > 4)
    res["table_shadow"] = W.fit_shadow(tc, band, U, a, {"dx": -1.0, "dy": -2.0, "g": 0.0, "soft": 3.0, "alpha": 0.4}, over, name="table recess")
    withdark = U.copy()
    sdf.render([{"box": [-10, -10, 810, 610], "layers": [W.shadow_layer(S, res["table_shadow"]) for S in tc]}], withdark, 1.0)
    lip = W.fit_shadow(tc, band, withdark, a, {"dx": 1.0, "dy": 2.0, "g": 0.0, "soft": 2.0, "alpha": 0.3}, over, name="table lip", rgb=LIP_RGB)
    if lip["alpha"] > 0.05:
        res["table_lip"] = lip
    res["table_casters"] = tc
    json.dump(res, open(os.path.join(CACHE, "fit.json"), "w"), indent=1)
    return res


def build_scene():
    res = json.load(open(os.path.join(CACHE, "fit.json")))
    ls = line_shape(res["line"])
    draw = [{"name": "navy field", "anchor": "span", "box": [wide.SPAN_X[0], -20.0, wide.SPAN_X[1], 620.0],
             "layers": [{"shape": {"t": "rrect", "c": [400.0, 300.0], "half": [3000.0, 400.0], "r": 0.0}, "fill": NAVY}]}]
    lay = [W.shadow_layer(S, res["table_shadow"]) for S in res["table_casters"]]
    if "table_lip" in res:
        lay += [W.shadow_layer(S, res["table_lip"], LIP_RGB) for S in res["table_casters"]]
    draw.append({"name": "table recess", "anchor": "centre", "box": [10.0, 60.0, 790.0, 540.0], "layers": lay})
    draw.append({"name": "frame line", "anchor": "centre", "box": [0.0, 30.0, 620.0, 560.0],
                 "layers": [W.shadow_layer(ls, res["line_shadow"]), {"shape": ls, "fill": NAVY_LINE}]})
    draw.append({"name": "header wire shadows", "anchor": "centre", "box": [255.0, 30.0, 770.0, 85.0],
                 "layers": [W.shadow_layer(S, res["line_shadow"]) for S in wire_shapes()]})
    draw.append({"name": "table, plate, header pills and wires, scroll bar, buttons", "anchor": "centre", "ref": "theme-backdrop"})
    return {"id": "theme-wide", "screen": "Theme Packs", "design_x": list(wide.DESIGN_X), "frame": [800, 600],
            "ground": {"flat": NAVY}, "draw": draw}


if __name__ == "__main__":
    args = sys.argv[1:]
    t0 = time.time()
    if "--fit" in args:
        fit_all()
    if "--deliver" in args:
        sc = build_scene()
        json.dump(sc, open(os.path.join(OUT, "theme-wide.json"), "w"))
        print(f"scene: {len(sc['draw'])} entries")
        wide.deliver(sc, OUT, "theme", art(), refs(), zoom_rows=(60, 530))
    print(f"{time.time() - t0:.0f}s")
