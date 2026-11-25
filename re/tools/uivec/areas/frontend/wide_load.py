"""P41W: Load Game (Title_LoadGame) and Pick a Pro (Title_Pickapro), wide. They share the right half of the screen.
Usage: py -3.12 wide_load.py load|pro [--refit] [--fit] [--deliver]

Read on the art (2026-11-25):
- left: the lilac cloud ground (span); Load's left column and Pick a Pro's ring, list and bottom bar are approved scenes;
- the navy slab: left edge the circle c (784.47, 250.30) r 527.13 (300 ground->slab crossings on Load, inlier rms 0.27 px;
  Pick a Pro's crossings lie on it, median -0.4 px), lit from the upper left, flat navy (103, 103, 159) inside; it runs off the
  top, bottom and right of the art: drawn as that circle united with the half plane right of its centre (continues to any
  right edge), the lit edge only along the circle's left half;
- the inset line: 2 px navy with a soft shadow, from under the title plate at y 60.6, along the circle c (783.11, 253.84)
  r 511.02, then right at y 529.5 into the OK button;
- the faint ball behind the page: circle c (544.17, 252.55) r 230.51 (16 side points of the sliver between plate and page and
  7 points of its dome above the plate, all within 2 px), lit from the upper left, soft dimples; it lies inside the frame;
- Pick a Pro only: the thin line from the ring's stub down x = 19.6 into the bottom bar, in the approved stub's own style
  (1.2 px navy over a 1.0-grown shadow moved 1.6, 1.6); the cream ball in the ring is art (opaque cut under the ring).
"""
import glob
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

SCREENS = {"load": ("Interface/Title_LoadGame.pcx", "load-", "Load Game"),
           "pro": ("Interface/Title_Pickapro.pcx", "pro-", "Pick a Pro")}
SLAB_C, SLAB_R = (784.47, 250.30), 527.13
LINE_C, LINE_R = (783.11, 253.84), 511.02
LINE_TOP_Y, LINE_BOT_Y = 60.6, 529.5
BALL_C, BALL_R = (544.17, 252.55), 230.51
NAVY_LINE = [33, 43, 79]
LIP_RGB = [214.0, 214.0, 240.0]       # the light lip under the page (rows 411-414 at x 500: 208, 203, 197, 192 lum)
SLAB_STOPS = [0.0, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.5, 9.0, 11.0, 14.0]
PRO_BALL_CUT = [78, 69, 125, 124]          # inside the ring's inner outline (r 61.45 about (140.1, 130.6)), corners under the ring
PRO_VLINE = [[19.6, 134.0], [19.6, 559.2], [21.6, 562.8], [25.6, 564.7], [32.0, 564.7]]


def ref_ids(screen):
    """load-backdrop / pro-backdrop each hold every approved element of the screen (the other load-* / pro-* files are the
    same elements one by one), so each screen composes just its backdrop scene: nothing is drawn twice."""
    return [SCREENS[screen][1] + "backdrop"]


def refs(screen):
    return W.load_refs(ref_ids(screen))


def art(screen):
    return W.sheet(SCREENS[screen][0])[0]


def slab_shape():
    return {"t": "union", "of": [{"t": "circle", "c": list(SLAB_C), "r": SLAB_R}, {"t": "half", "p": [SLAB_C[0], 0.0], "n": [-1.0, 0.0]}]}


def line_points(step=3.0):
    a0 = math.atan2(LINE_TOP_Y - LINE_C[1], -math.sqrt(LINE_R ** 2 - (LINE_TOP_Y - LINE_C[1]) ** 2))
    a1 = math.atan2(LINE_BOT_Y - LINE_C[1], -math.sqrt(LINE_R ** 2 - (LINE_BOT_Y - LINE_C[1]) ** 2))
    p_top = (LINE_C[0] + LINE_R * math.cos(a0), LINE_C[1] + LINE_R * math.sin(a0))

    def seg(p, q):
        n = max(1, int(math.hypot(q[0] - p[0], q[1] - p[1]) / step))
        return [(p[0] + (q[0] - p[0]) * k / n, p[1] + (q[1] - p[1]) * k / n) for k in range(1, n + 1)]

    pts = [(345.0, LINE_TOP_Y)]
    pts += seg(pts[-1], p_top)
    da = a0 - a1
    if da < 0:
        da += 2 * math.pi
    n = int(LINE_R * da / step)
    for k in range(1, n + 1):
        t = a0 - da * k / n
        pts.append((LINE_C[0] + LINE_R * math.cos(t), LINE_C[1] + LINE_R * math.sin(t)))
    pts += seg(pts[-1], (615.0, LINE_BOT_Y))
    return [[round(p[0], 2), round(p[1], 2)] for p in pts]


def line_shape():
    """Analytic: top run from under the plate, 6 px fillet, the circle, 6 px fillet, bottom run into the OK button."""
    return W.path_hline_circle_hline(345.0, LINE_TOP_Y, LINE_C, LINE_R, LINE_BOT_Y, 615.0, 2.0, 6.0)


def all_items(screen):
    out = []
    for v in refs(screen).values():
        out += v
    return out


def fit_all(screen, refit_ground=False):
    a = art(screen)
    x, y = W.grid()
    items = all_items(screen)
    E = W.coverage(items, 0)
    slab_d = sdf.sd(slab_shape(), x, y)
    line_d = sdf.sd(line_shape(), x, y)
    ground = W.cloud_pixels(a) & ~W.dilate(E, 10) & (slab_d > 4)
    if screen == "pro":
        vl = sdf.sd({"t": "stroke", "pts": PRO_VLINE, "w": 1.2, "cap": "round"}, x, y)
        ground &= vl > 6
    g = W.fit_ground(screen, a, ground, refit=refit_ground)
    G = wide.ground_1x({"draw": W.ground_entries(g)})
    res = {}
    ball_d = np.hypot(x - BALL_C[0], y - BALL_C[1]) - BALL_R
    plain = (slab_d < -0.5) & ~W.dilate(E, 8) & (line_d > 7) & (ball_d > 6)
    res["slab_body"] = [round(float(v), 1) for v in np.median(a[plain & (slab_d < -20)], 0)]
    circ = {"t": "circle", "c": list(SLAB_C), "r": SLAB_R}
    band = plain & (sdf.sd(circ, x, y) > -SLAB_STOPS[-1] - 2)
    coef = W.fit_bevel(circ, a, band, SLAB_STOPS)
    coef[-1] = [list(res["slab_body"]), [0, 0, 0], [0, 0, 0], [0, 0, 0], [0, 0, 0]]
    res["slab_bevel"] = coef
    bm = (ball_d < -2) & ~W.dilate(E, 7) & (line_d > 6)   # the plate's and page's own shadows lie in the sliver
    light, X, rms = W.fit_sphere(a, bm, BALL_C, BALL_R)
    res["ball_light"], res["ball_quad"], res["ball_rms"] = light, X, rms
    print(f"{screen} ball light {np.round(light, 3)} rms {rms:.2f} on {bm.sum()} px")
    stops = W.sphere_stops(BALL_R)
    res["ball_stops"], res["ball_bevel"] = stops, W.sphere_bevel(BALL_R, light, X, stops)
    # dimples by eye (2026-11-25): the visible slivers are too small to fit a lattice; cups about 34 px apart on the sphere,
    # low contrast, shaded wall toward the upper left
    res["dimples"] = {"a": 34.0, "turn": 100.0, "p0": 0.3, "p1": 0.1, "rd_k": 0.43, "soft": 3.5, "g_top": 7.0, "g_bot": 5.0,
                      "light": -125.0, "fall": 0.6, "note": "by eye: slivers too small to fit a lattice"}
    under = G.copy()
    sdf.render(slab_entries(res), under, 1.0)
    over_line = W.overlay([{"box": [-10, -10, 810, 610], "layers": [{"shape": line_shape(), "fill": NAVY_LINE}]}])
    lb = (line_d > -1.5) & (line_d < 8) & ~W.dilate(E, 4) & (slab_d < -2)
    res["line_shadow"] = W.fit_shadow([line_shape()], lb, under, a, {"dx": 1.0, "dy": 1.0, "g": 0.0, "soft": 2.0, "alpha": 0.4},
                                      over_line, name=f"{screen} line")
    # the panels' soft drop shadows (the art had them baked in under the approved panels): one family on the slab, one on
    # the ground, each fitted with every approved element composited on top as in the final screen
    fam = panel_casters(screen)
    full = under.copy()
    sdf.render(build_scene_draw_ball_line(res), full, 1.0)
    over = W.overlay(all_items(screen))
    for key, shapes in fam.items():
        if not shapes:
            continue
        d = np.min([sdf.sd(S, x, y) for S in shapes], 0)
        others = W.coverage([it for it in all_items(screen) if it["layers"][0]["shape"] not in shapes], 2)
        band = (d > -0.5) & (d < 14) & ~others & (line_d > 4)
        band &= (slab_d > 3) if key == "ground" else (slab_d < -2)
        p_dark = W.fit_shadow(shapes, band, full, a, {"dx": 2.0, "dy": 3.0, "g": 0.0, "soft": 4.0, "alpha": 0.4},
                              over, name=f"{screen} panels on {key} (dark)")
        res[f"panel_shadow_{key}"] = p_dark
        res[f"panel_casters_{key}"] = shapes
        if key not in LIP_FAMILIES:
            continue
        # the lit lip on the far side (a recessed window), fitted over the dark layer
        withdark = full.copy()
        sdf.render([{"box": [-10, -10, 810, 610], "layers": [W.shadow_layer(S, p_dark) for S in shapes]}], withdark, 1.0)
        p_lip = W.fit_shadow(shapes, band, withdark, a, {"dx": 2.0, "dy": 3.0, "g": 0.0, "soft": 2.5, "alpha": 0.4},
                             over, name=f"{screen} panels on {key} (lip)", rgb=LIP_RGB)
        if p_lip["alpha"] > 0.05:
            res[f"panel_lip_{key}"] = p_lip
    json.dump(res, open(os.path.join(W.dirs(screen)[1], "fit.json"), "w"), indent=1)
    return res


# families: "slab" = the page and info bar, recessed windows in the slab (dark above / left, a lit lip below); "plate" = the
# title plate, raised (a plain drop shadow below it); "ground" = the left-hand panels on the cloud ground (drop shadow)
PANEL_BOXES = {"load": {"slab": ([303, 107, 773, 415], [372, 431, 776, 496]), "plate": ([326, 35, 684, 69],),
                        "ground": ([14, 0, 238, 120], [17, 125, 233, 279], [17, 270, 233, 597])},
               "pro": {"slab": ([303, 107, 773, 415], [373, 432, 775, 495]), "plate": ([326, 35, 684, 70],),
                       "ground": ([20, 14, 260, 250], [26, 255, 251, 526], [25, 546, 560, 584])}}
LIP_FAMILIES = ("slab",)


def panel_casters(screen):
    """The outer silhouettes of the screen's big panels (each approved item's first layer, a sub taken by its outer part)."""
    out = {}
    items = all_items(screen)
    for key, boxes in PANEL_BOXES[screen].items():
        shapes = []
        for it in items:
            if [round(v) for v in it["box"]] in [list(b) for b in boxes]:
                S = it["layers"][0]["shape"]
                shapes.append(S["a"] if S["t"] == "sub" else S)
        out[key] = shapes
    return out


def build_scene_draw_ball_line(res):
    """The faint ball and the inset line as plain items (for fitting the panel shadows over them)."""
    ball = {"t": "circle", "c": list(BALL_C), "r": BALL_R}
    paint = {"bevel": {"L": 1.0, "stops": res["ball_stops"], "coef": res["ball_bevel"]}}
    ents = [{"box": [BALL_C[0] - BALL_R - 2, BALL_C[1] - BALL_R - 2, BALL_C[0] + BALL_R + 2, BALL_C[1] + BALL_R + 2],
             "layers": [{"shape": ball, "fill": paint, "soft": 1.5}]}]
    ls = line_shape()
    ents.append({"box": [240.0, 40.0, 640.0, 560.0], "layers": [W.shadow_layer(ls, res["line_shadow"]), {"shape": ls, "fill": NAVY_LINE}]})
    return ents


def slab_entries(res):
    circ = {"t": "circle", "c": list(SLAB_C), "r": SLAB_R}
    ring = {"t": "sub", "a": circ, "b": {"t": "circle", "c": list(SLAB_C), "r": SLAB_R - SLAB_STOPS[-1]}}
    return [{"name": "slab", "anchor": "centre", "box": [SLAB_C[0] - SLAB_R - 2, -40.0, wide.SPAN_X[1], 640.0],
             "layers": [{"shape": slab_shape(), "fill": res["slab_body"]},
                        {"shape": ring, "fill": {"bevel": {"L": 1.0, "stops": SLAB_STOPS, "coef": res["slab_bevel"], "of": circ}},
                         "clip": {"t": "half", "p": [SLAB_C[0], 0.0], "n": [1.0, 0.0]}}]}]


def build_scene(screen):
    out, cache = W.dirs(screen)
    res = json.load(open(os.path.join(cache, "fit.json")))
    g = W.fit_ground(screen, art(screen), None)
    draw = W.ground_entries(g)
    draw += slab_entries(res)
    ball = {"t": "circle", "c": list(BALL_C), "r": BALL_R}
    R = BALL_R
    paint = {"bevel": {"L": 1.0, "stops": res["ball_stops"], "coef": res["ball_bevel"]}}
    draw.append({"name": "faint ball", "anchor": "centre", "box": [BALL_C[0] - R - 2, BALL_C[1] - R - 2, BALL_C[0] + R + 2, BALL_C[1] + R + 2],
                 "layers": [{"shape": ball, "fill": paint, "soft": 1.5}]})
    draw += W.dimple_entries(BALL_C, BALL_R - 1, res["dimples"], paint, ball, BALL_R - 1.0, name="faint ball dimple")
    ls = line_shape()
    draw.append({"name": "inset line", "anchor": "centre", "box": [240.0, 40.0, 640.0, 560.0],
                 "layers": [W.shadow_layer(ls, res["line_shadow"]), {"shape": ls, "fill": NAVY_LINE}]})
    for key in ("slab", "plate", "ground"):
        if f"panel_shadow_{key}" in res:
            for i, S in enumerate(res[f"panel_casters_{key}"]):
                bx = [-10.0, -10.0, 810.0, 610.0]
                lay = [W.shadow_layer(S, res[f"panel_shadow_{key}"])]
                if f"panel_lip_{key}" in res:
                    lay.append(W.shadow_layer(S, res[f"panel_lip_{key}"], LIP_RGB))
                draw.append({"name": f"panel shadow and lip ({key}) {i}", "anchor": "centre", "box": bx, "layers": lay})
    if screen == "pro":
        vl = {"t": "stroke", "pts": PRO_VLINE, "w": 1.2, "cap": "butt"}
        draw.append({"name": "left line (ring stub to bottom bar)", "anchor": "centre", "box": [10.0, 125.0, 40.0, 575.0],
                     "layers": [{"shape": {"t": "move", "of": {"t": "grow", "of": vl, "d": 1.0}, "d": [1.6, 1.6]}, "fill": [70, 72, 110]},
                                {"shape": vl, "fill": NAVY_LINE}]})
        draw.append({"name": "pro ball (art)", "anchor": "centre",
                     "picture": {"sheet": SCREENS[screen][0], "rect": PRO_BALL_CUT, "at": PRO_BALL_CUT[:2], "matte": {"kind": "opaque"}}})
        draw.append({"name": "ring, list, bottom bar, page, buttons", "anchor": "centre", "ref": "pro-backdrop"})
    else:
        draw.append({"name": "left column, page, plate, info bar, scroll, buttons", "anchor": "centre", "ref": "load-backdrop"})
    return {"id": f"{screen}-wide", "screen": SCREENS[screen][2], "design_x": list(wide.DESIGN_X), "frame": [800, 600],
            "ground": W.ground_meta(g), "draw": draw}


if __name__ == "__main__":
    args = sys.argv[1:]
    screen = args[0]
    t0 = time.time()
    if "--fit" in args or "--refit" in args:
        fit_all(screen, "--refit" in args)
    if "--deliver" in args:
        out, _ = W.dirs(screen)
        sc = build_scene(screen)
        json.dump(sc, open(os.path.join(out, f"{screen}-wide.json"), "w"))
        print(f"scene: {len(sc['draw'])} entries, {os.path.getsize(os.path.join(out, f'{screen}-wide.json')) // 1024} KB")
        wide.deliver(sc, out, screen, art(screen), refs(screen), zoom_rows=(150, 560))
    print(f"{time.time() - t0:.0f}s")
