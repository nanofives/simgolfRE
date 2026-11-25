"""P41W: the character editor (CustGolfBckgrnd female, CustGlfBckMale male), wide.
Usage: py -3.12 wide_editor.py [--refit] [--fit] [--deliver]

Read on the art (2026-11-25):
- the cloud ground above (span);
- the navy hill below: its top edge is the circle c (417.71, 1356.04) r 1190.46 (7 edge points on both sides of the screen,
  within 0.42 px; it predicts the edge at x 798 within 1.5 px), a soft lit rim along it; outside the frame the hill goes on
  curving down (at x -300 / 1100 its edge is at y 406 / 381, past 32:9 it drops below the screen);
- the cream sheet behind the left pills: x 63..245 (the right outline at x 245; the left one hidden by the pills, taken by
  symmetry about the bottom arc's middle), bottom arc through (70, 249.6), (154, 256.8), (240, 247.2), navy outline, cast
  shadow; its top under the name card;
- the portrait window (sky and grass, its navy frame): art, opaque cut x 327..447, y 16..245 (its right side under the
  icons), with a fitted cast shadow;
- the thin line from the page's top right to the reset button (y 18.2) and the arc from the reset button down to the OK button
  (circle c (460.95, 137.39) r 314.31, line centres in 37 rows, rms 0.12 px), 1.5 px navy with a soft shadow;
- the approved list, name card, pills, page, icons and buttons (editor-backdrop, state female / male) on top.
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

SCREEN = "editor"
OUT, CACHE = W.dirs(SCREEN)
SHEETS = {"female": "Interface/CustGolfBckgrnd.pcx", "male": "Interface/CustGlfBckMale.pcx"}
HILL_C, HILL_R = (417.71, 1356.04), 1190.46
SHEET_X = (63.0, 245.0)
SHEET_ARC_PTS = [(70.0, 249.6), (154.0, 256.8), (240.0, 247.2)]
WINDOW_CUT = [327, 16, 121, 229]
ARC_C, ARC_R = (460.95, 137.39), 314.31
TOPLINE_Y = 18.2
NAVY_LINE = [33, 43, 79]
CREAM = [255.0, 247.0, 231.0]
LIP_RGB = [150.0, 150.0, 205.0]
HILL_STOPS = [0.0, 1.0, 2.0, 3.5, 5.0, 7.0, 9.5, 12.5, 16.0, 20.0, 26.0]


def art(state="female"):
    return W.sheet(SHEETS[state])[0]


def ref_items(state="female"):
    d = json.load(open(os.path.join(W.UIVEC, "scenes", "frontend", "editor-backdrop.json")))
    for st in d["states"]:
        if st["name"] == state:
            return st["scene"]
    raise KeyError(state)


def refs(state="female"):
    return {f"editor-backdrop:{state}": ref_items(state)}


def sheet_arc():
    return fit.fit_circle(np.array(SHEET_ARC_PTS), trim=99, rounds=1)[:3]


def sheet_shape():
    cx, cy, R = sheet_arc()
    return {"t": "inter", "of": [{"t": "rrect", "c": [(SHEET_X[0] + SHEET_X[1]) / 2, 170.0], "half": [(SHEET_X[1] - SHEET_X[0]) / 2, 110.0], "r": 0.0},
                                 {"t": "circle", "c": [round(float(cx), 3), round(float(cy), 3)], "r": round(float(R), 3)}]}


def hill_circle():
    return {"t": "circle", "c": list(HILL_C), "r": HILL_R}


def line_shapes():
    a0 = math.degrees(math.atan2(50.0 - ARC_C[1], math.sqrt(ARC_R ** 2 - (50.0 - ARC_C[1]) ** 2)))
    a1 = math.degrees(math.atan2(205.0 - ARC_C[1], math.sqrt(ARC_R ** 2 - (205.0 - ARC_C[1]) ** 2)))
    return [{"t": "seg", "a": [705.0, TOPLINE_Y], "b": [752.0, TOPLINE_Y], "w": 1.5}, W.arc_short(ARC_C, ARC_R, a0, a1, 1.5)]


def fit_all(refit_ground=False):
    a = art("female")
    x, y = W.grid()
    items = ref_items("female")
    E = W.coverage(items, 0)
    hd = sdf.sd(hill_circle(), x, y)
    sd_ = sdf.sd(sheet_shape(), x, y)
    win = np.zeros((600, 800), bool)
    gx, gy, gw, gh = WINDOW_CUT
    win[gy - 4:gy + gh + 10, gx - 4:gx + gw + 10] = True
    ld = np.min([sdf.sd(S, x, y) for S in line_shapes()], 0)
    ground = W.cloud_pixels(a) & ~W.dilate(E, 12) & (hd > 6) & (sd_ > 10) & ~win & (ld > 7)
    g = W.fit_ground(SCREEN, a, ground, refit=refit_ground)
    G = wide.ground_1x({"draw": W.ground_entries(g)})
    res = {}
    body = (hd < -30) & ~W.dilate(E, 6) & ~win & (sd_ > 6)
    res["navy"] = [round(float(v), 1) for v in np.median(a[body], 0)]
    band = (hd < -0.3) & (hd > -HILL_STOPS[-1] - 2) & ~W.dilate(E, 6) & ~win & (sd_ > 6)
    coef = W.fit_bevel(hill_circle(), a, band, HILL_STOPS)
    coef[-1] = [list(res["navy"]), [0, 0, 0], [0, 0, 0], [0, 0, 0], [0, 0, 0]]
    res["hill_bevel"] = coef
    under = G.copy()
    sdf.render(hill_entries(res), under, 1.0)
    # the sheet: cream with a navy outline, its cast shadow fitted on the band outside it
    sheet_items = sheet_entries()
    over_s = W.overlay(sheet_items + items)
    sb = (sd_ > -0.5) & (sd_ < 14) & ~W.dilate(E, 2)
    res["sheet_shadow"] = W.fit_shadow([sheet_shape()], sb, under, a, {"dx": 3.0, "dy": 3.0, "g": 0.0, "soft": 3.0, "alpha": 0.4}, over_s, name="sheet")
    # the window's cast shadow (its cut drawn opaque on top)
    wrect = {"t": "rrect", "c": [gx + gw / 2, gy + gh / 2], "half": [gw / 2, gh / 2], "r": 0.0}
    wd = sdf.sd(wrect, x, y)
    over_w = W.overlay(items + [{"box": [gx, gy, gx + gw, gy + gh], "layers": [{"shape": wrect, "fill": [0, 0, 0]}]}])
    wb = (wd > -0.5) & (wd < 12) & ~W.dilate(E, 2)
    res["window_shadow"] = W.fit_shadow([wrect], wb, under, a, {"dx": 2.0, "dy": 3.0, "g": 0.0, "soft": 3.0, "alpha": 0.4}, over_w, name="window")
    res["window_rect"] = wrect
    over_l = W.overlay([{"box": [-10, -10, 810, 610], "layers": [{"shape": S, "fill": NAVY_LINE} for S in line_shapes()]}])
    lb = (ld > -1.0) & (ld < 8) & ~W.dilate(E, 3)
    res["line_shadow"] = W.fit_shadow(line_shapes(), lb, under, a, {"dx": 2.0, "dy": 2.0, "g": 0.0, "soft": 2.0, "alpha": 0.3}, over_l, name="lines")
    # around the list the art has a soft light halo on the hill (no dark shadow: a dark fit came out as a shifted rectangle
    # above-left of the panel), fitted as one light layer
    lst = [it for it in items if it.get("name") == "list"][0]["layers"][0]["shape"]
    lst = lst["a"] if lst["t"] == "sub" else lst
    dl = sdf.sd(lst, x, y)
    over_all = W.overlay(items)
    lb2 = (dl > -0.5) & (dl < 16) & ~W.coverage([it for it in items if it.get("name") != "list"], 2)
    res["list_shape"] = lst
    res.pop("list_shadow", None)
    res["list_lip"] = W.fit_shadow([lst], lb2, under, a, {"dx": 0.0, "dy": 0.0, "g": 2.0, "soft": 4.0, "alpha": 0.3}, over_all,
                                   name="list halo", rgb=LIP_RGB)
    json.dump(res, open(os.path.join(CACHE, "fit.json"), "w"), indent=1)
    return res


def hill_entries(res):
    S = hill_circle()
    return [{"name": "navy hill", "anchor": "centre", "box": [wide.SPAN_X[0], HILL_C[1] - HILL_R - 2, wide.SPAN_X[1], 640.0],
             "layers": [{"shape": S, "fill": {"bevel": {"L": 1.0, "stops": HILL_STOPS, "coef": res["hill_bevel"]}}}]}]


def sheet_entries():
    S = sheet_shape()
    return [{"name": "cream sheet", "anchor": "centre", "box": [55.0, 50.0, 255.0, 266.0],
             "layers": [{"shape": {"t": "grow", "of": S, "d": 0.9}, "fill": NAVY_LINE}, {"shape": {"t": "grow", "of": S, "d": -0.9}, "fill": CREAM}]}]


def list_rim(lst):
    """The list sits in a recess (read on the art, 2026-11-25): outside its top and left edges a dark ramp 8 px wide (109 down
    to 81 lum at the panel: col 400 rows 259-267, row 400 cols 31-39), outside its right and bottom edges a light lip 9 px
    wide (169 at the panel down to 109: row 400 cols 763-771, col 400 rows 587-595). Both are the panel grown by 4 px with a
    4 px soft edge (alpha ramps over 8 px outside), split along the panel's diagonal from its bottom-left to its top-right."""
    x0, x1 = lst["c"][0] - lst["half"][0], lst["c"][0] + lst["half"][0]
    y0, y1 = lst["c"][1] - lst["half"][1], lst["c"][1] + lst["half"][1]
    dx, dy = x1 - x0, y0 - y1
    n = math.hypot(dx, dy)
    nrm = [round(-dy / n, 5), round(dx / n, 5)]        # points down-right
    grown = {"t": "grow", "of": lst, "d": 4.0}
    return {"name": "list recess rim", "anchor": "centre", "box": [x0 - 12, y0 - 12, x1 + 12, y1 + 12],
            "layers": [{"shape": grown, "soft": 4.0, "fill": W.SHADOW_RGB, "alpha": 0.35,
                        "clip": {"t": "half", "p": [x0, y1], "n": nrm}},
                       {"shape": grown, "soft": 4.0, "fill": [178.0, 178.0, 218.0], "alpha": 0.86,
                        "clip": {"t": "half", "p": [x0, y1], "n": [-nrm[0], -nrm[1]]}}]}


def build_scene(state):
    res = json.load(open(os.path.join(CACHE, "fit.json")))
    g = W.fit_ground(SCREEN, art("female"), None)
    draw = W.ground_entries(g)
    draw += hill_entries(res)
    # the whole list panel's outline read on the profiles (its first approved layer is only the left column)
    draw.append(list_rim({"t": "rrect", "c": [401.0, 427.25], "half": [361.0, 159.25], "r": 0.0}))
    draw.append({"name": "cream sheet shadow", "anchor": "centre", "box": [50.0, 50.0, 270.0, 280.0],
                 "layers": [W.shadow_layer(sheet_shape(), res["sheet_shadow"])]})
    draw += sheet_entries()
    draw.append({"name": "portrait window shadow", "anchor": "centre", "box": [315.0, 5.0, 470.0, 265.0],
                 "layers": [W.shadow_layer(res["window_rect"], res["window_shadow"])]})
    draw.append({"name": "portrait window (art)", "anchor": "centre",
                 "picture": {"sheet": SHEETS[state], "rect": WINDOW_CUT, "at": WINDOW_CUT[:2], "matte": {"kind": "opaque"}}})
    ls = line_shapes()
    draw.append({"name": "lines: page to reset, reset to OK", "anchor": "centre", "box": [695.0, 5.0, 795.0, 215.0],
                 "layers": [W.shadow_layer(S, res["line_shadow"]) for S in ls] + [{"shape": S, "fill": NAVY_LINE} for S in ls]})
    draw.append({"name": "list, name card, pills, page, icons, buttons", "anchor": "centre", "ref": "editor-backdrop", "state": state})
    return {"id": f"editor-wide-{state}", "screen": "Character editor", "state": state, "design_x": list(wide.DESIGN_X),
            "frame": [800, 600], "ground": W.ground_meta(g), "draw": draw}


if __name__ == "__main__":
    args = sys.argv[1:]
    t0 = time.time()
    if "--fit" in args or "--refit" in args:
        fit_all("--refit" in args)
    if "--deliver" in args:
        for state, tag in (("female", "editor"), ("male", "editor_male")):
            sc = build_scene(state)
            json.dump(sc, open(os.path.join(OUT, f"{tag}-wide.json"), "w"))
            print(f"{state}: {len(sc['draw'])} entries")
            wide.deliver(sc, OUT, tag, art(state), {"editor-backdrop": ref_items(state)}, zoom_rows=(120, 400))
    print(f"{time.time() - t0:.0f}s")
