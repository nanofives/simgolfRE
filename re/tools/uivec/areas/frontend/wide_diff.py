"""P41W: the difficulty screen, wide (TitleSelDiffUnSel). Usage: py -3.12 wide_diff.py [--refit] [--fit] [--deliver]

Read on the art (2026-11-25):
- left: the lilac cloud ground (span);
- the navy slab: a disc whose left edge is the circle c (850.2, 254.95) r 498.45 (fit of 140 ground->slab crossings, inlier rms
  0.15 px), lit from above (a light bevel along its top-left edge, flat navy inside); it runs off the top, bottom and right of the
  art, so it is drawn as that circle united with the half plane right of its centre: it continues to any right edge (span by
  construction, anchored centre);
- the inset line: 2 px navy with a soft shadow below-right, leaving the title plate at y 55, turning down along the circle
  c (883.77, 253.38) r 513.59 (18.4 px inside the slab edge), turning right at y 529.75, then a diagonal y = 0.7586 x + 99.37
  off the bottom edge; corners rounded;
- the big ball: circle c (843.2, 298.1) r 305.5 (fit of 7 read limb points, all within 2 px), lit from the upper right, soft
  dimples; cut off by the art's right edge, it is drawn whole (a lit sphere: shading quadratic in n.L), so at 16:9 and wider
  more of it shows on the slab;
- the faces, pads, title plate and back button: the approved diff-backdrop scene, unchanged.
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

SCREEN = "diff"
OUT, CACHE = W.dirs(SCREEN)
SHEET = "Interface/TitleSelDiffUnSel.pcx"
SLAB_C, SLAB_R = (850.17, 254.95), 498.45
LINE_C, LINE_R = (883.77, 253.38), 513.59
LINE_TOP_Y, LINE_BOT_Y = 55.0, 529.75
DIAG = (0.7586, 99.37)
BALL_C, BALL_R = (843.16, 298.06), 305.46
NAVY_LINE = [33, 43, 79]
SLAB_STOPS = [0.0, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.5, 9.0, 11.0, 14.0]


def art():
    return W.sheet(SHEET)[0]


def refs():
    return {"diff-backdrop": wide.load_ref("diff-backdrop")}


def slab_shape():
    return {"t": "union", "of": [{"t": "circle", "c": list(SLAB_C), "r": SLAB_R},
                                 {"t": "half", "p": [SLAB_C[0], 0.0], "n": [-1.0, 0.0]}]}


def line_points(step=3.0):
    """The inset line as one dense polyline (so the corner rounding stays local)."""
    pts = [(440.0, LINE_TOP_Y)]
    a0 = math.atan2(LINE_TOP_Y - LINE_C[1], -math.sqrt(LINE_R ** 2 - (LINE_TOP_Y - LINE_C[1]) ** 2))
    a1 = math.atan2(LINE_BOT_Y - LINE_C[1], -math.sqrt(LINE_R ** 2 - (LINE_BOT_Y - LINE_C[1]) ** 2))
    p_top = (LINE_C[0] + LINE_R * math.cos(a0), LINE_C[1] + LINE_R * math.sin(a0))
    p_bot = (LINE_C[0] + LINE_R * math.cos(a1), LINE_C[1] + LINE_R * math.sin(a1))
    x_corner = (LINE_BOT_Y - DIAG[1]) / DIAG[0]
    end = (720.0, DIAG[0] * 720.0 + DIAG[1])

    def seg(p, q):
        n = max(1, int(math.hypot(q[0] - p[0], q[1] - p[1]) / step))
        return [(p[0] + (q[0] - p[0]) * k / n, p[1] + (q[1] - p[1]) * k / n) for k in range(1, n + 1)]

    pts += seg(pts[-1], p_top)
    # the arc runs counter-clockwise on screen from the top crossing (angle a0, about 203 deg) down to a1 (about 148 deg)
    da = a0 - a1
    if da < 0:
        da += 2 * math.pi
    n = int(LINE_R * abs(da) / step)
    for k in range(1, n + 1):
        t = a0 - da * k / n
        pts.append((LINE_C[0] + LINE_R * math.cos(t), LINE_C[1] + LINE_R * math.sin(t)))
    pts += seg(pts[-1], (x_corner, LINE_BOT_Y))
    pts += seg(pts[-1], end)
    return [[round(p[0], 2), round(p[1], 2)] for p in pts]


def line_shape():
    """Analytic: the top run, a 6 px fillet, the circle, a 6 px fillet, the bottom run, an 8 px fillet, the diagonal off the
    bottom edge (wide_screen.path_hline_circle_hline; line_points() is the earlier polyline, kept for reference)."""
    return W.path_hline_circle_hline(440.0, LINE_TOP_Y, LINE_C, LINE_R, LINE_BOT_Y, None, 2.0, 6.0, tail=(DIAG[0], DIAG[1], 720.0, 8.0))


def masks(a):
    x, y = W.grid()
    E = W.coverage(refs()["diff-backdrop"], 0)
    slab_d = sdf.sd(slab_shape(), x, y)
    ground = W.cloud_pixels(a) & ~W.dilate(E, 12) & (slab_d > 4)
    return E, slab_d, ground


def fit_all(refit_ground=False):
    a = art()
    x, y = W.grid()
    E, slab_d, ground = masks(a)
    g = W.fit_ground(SCREEN, a, ground, refit=refit_ground)
    G = wide.ground_1x({"draw": W.ground_entries(g)})
    res = {}
    # slab: the bevel band along the circle; inside it the flat navy measured on the plain slab
    line_d = sdf.sd(line_shape(), x, y)
    ball_d = np.hypot(x - BALL_C[0], y - BALL_C[1]) - BALL_R
    plain = (slab_d < -0.5) & ~W.dilate(E, 14) & (line_d > 7) & (ball_d > 8)
    body = a[plain & (slab_d < -20)]
    res["slab_body"] = [round(float(v), 1) for v in np.median(body, 0)]
    circ = {"t": "circle", "c": list(SLAB_C), "r": SLAB_R}
    band = plain & (sdf.sd(circ, x, y) > -SLAB_STOPS[-1] - 2)
    coef = W.fit_bevel(circ, a, band, SLAB_STOPS)
    coef[-1] = [list(res["slab_body"]), [0, 0, 0], [0, 0, 0], [0, 0, 0], [0, 0, 0]]
    res["slab_bevel"] = coef
    # the ball: a sphere's shading on the ball pixels clear of the plate, the back button and the line
    bm = (ball_d < -3) & ~W.dilate(E, 6) & (line_d > 6)
    light, X, rms = W.fit_sphere(a, bm, BALL_C, BALL_R)
    res["ball_light"], res["ball_quad"], res["ball_rms"] = light, X, rms
    print(f"ball light {np.round(light, 3)} rms {rms:.2f}")
    stops = W.sphere_stops(BALL_R)
    bev = W.sphere_bevel(BALL_R, light, X, stops)
    res["ball_stops"], res["ball_bevel"] = stops, bev
    ball = {"t": "circle", "c": list(BALL_C), "r": BALL_R}
    U = a.copy()
    smooth = W.bevel_render(ball, bev, stops, U)
    hp = (fit.lum(a) - fit.lum(smooth)) * bm
    res["dimples"] = W.fit_dimples_fast(hp, bm & (ball_d < -10), BALL_C, BALL_R - 2, a_range=(28.0, 48.0),
                                   look={"rd_k": 0.42, "g_top": 8.0, "g_bot": 6.0, "soft": 4.0, "light": -45.0, "fall": 1.0})
    # shadows: the line's (fitted on the slab beside it), the ball's on the slab, the pads'
    S_slab = slab_entries(res)
    under = G.copy()
    sdf.render([dict(e) for e in S_slab], under, 1.0)
    over_line = W.overlay([{"box": [-10, -10, 810, 610], "layers": [{"shape": line_shape(), "fill": NAVY_LINE}]}])
    lb = (line_d > -1.5) & (line_d < 8) & plain.__or__(line_d < 2) & ~W.dilate(E, 4) & (ball_d > 4) & (slab_d < -2)
    res["line_shadow"] = W.fit_shadow([line_shape()], lb, under, a, {"dx": 1.0, "dy": 1.0, "g": 0.0, "soft": 2.0, "alpha": 0.4},
                                      over_line, name="line")
    bb = (ball_d > 0.5) & (ball_d < 12) & (slab_d < -4) & ~W.dilate(E, 4) & (line_d > 6)
    res["ball_shadow"] = W.fit_shadow([ball], bb, under, a, {"dx": -2.0, "dy": 2.0, "g": 0.0, "soft": 3.0, "alpha": 0.3}, name="ball")
    pads = [it["layers"][0]["shape"] for it in refs()["diff-backdrop"] if len(it["layers"]) == 2 and it["layers"][0]["shape"]["t"] == "ellipse"]
    res["pad_shapes"] = pads
    d_p = np.min([sdf.sd(S, x, y) for S in pads], 0)
    faces = [it for it in refs()["diff-backdrop"] if len(it["layers"]) == 6]
    F = W.coverage(faces, 6)
    over_p = W.overlay(refs()["diff-backdrop"])
    pb = (d_p > -0.5) & (d_p < 14) & ~F & (line_d > 4)
    full = under.copy()
    res["pad_shadow"] = W.fit_shadow(pads, pb, full, a, {"dx": 1.5, "dy": 3.0, "g": 0.0, "soft": 4.0, "alpha": 0.5}, over_p, name="pads")
    json.dump(res, open(os.path.join(CACHE, "fit.json"), "w"), indent=1)
    return res


def slab_entries(res):
    circ = {"t": "circle", "c": list(SLAB_C), "r": SLAB_R}
    ring = {"t": "sub", "a": circ, "b": {"t": "circle", "c": list(SLAB_C), "r": SLAB_R - SLAB_STOPS[-1]}}
    return [{"name": "slab", "anchor": "centre", "box": [SLAB_C[0] - SLAB_R - 2, -40.0, wide.SPAN_X[1], 640.0],
             "layers": [{"shape": slab_shape(), "fill": res["slab_body"]},
                        {"shape": ring, "fill": {"bevel": {"L": 1.0, "stops": SLAB_STOPS, "coef": res["slab_bevel"], "of": circ}},
                         # only the circle's left half is a real edge: right of the centre the slab goes on (the half plane)
                         "clip": {"t": "half", "p": [SLAB_C[0], 0.0], "n": [1.0, 0.0]}}]}]


def build_scene():
    res = json.load(open(os.path.join(CACHE, "fit.json")))
    g = W.fit_ground(SCREEN, art(), None)
    draw = W.ground_entries(g)
    draw += slab_entries(res)
    ball = {"t": "circle", "c": list(BALL_C), "r": BALL_R}
    R = BALL_R
    draw.append({"name": "ball shadow", "anchor": "centre", "box": [BALL_C[0] - R - 16, BALL_C[1] - R - 16, BALL_C[0] + R + 16, BALL_C[1] + R + 16],
                 "layers": [W.shadow_layer(ball, res["ball_shadow"])]})
    paint = {"bevel": {"L": 1.0, "stops": res["ball_stops"], "coef": res["ball_bevel"]}}
    draw.append({"name": "ball", "anchor": "centre", "box": [BALL_C[0] - R - 2, BALL_C[1] - R - 2, BALL_C[0] + R + 2, BALL_C[1] + R + 2],
                 "layers": [{"shape": ball, "fill": paint, "soft": 1.2}]})
    draw += W.dimple_entries(BALL_C, BALL_R - 2, res["dimples"], paint, ball, BALL_R - 1.5)
    ls = line_shape()
    draw.append({"name": "inset line", "anchor": "centre", "box": [380.0, 40.0, 740.0, 640.0],
                 "layers": [W.shadow_layer(ls, res["line_shadow"]), {"shape": ls, "fill": NAVY_LINE}]})
    for i, S in enumerate(res["pad_shapes"]):
        m = max(S["r"]) + 16
        draw.append({"name": f"pad {i} shadow", "anchor": "centre", "box": [S["c"][0] - m, S["c"][1] - m, S["c"][0] + m, S["c"][1] + m],
                     "layers": [W.shadow_layer(S, res["pad_shadow"])]})
    draw.append({"name": "faces, pads, title plate, back button", "anchor": "centre", "ref": "diff-backdrop"})
    return {"id": "diff-wide", "screen": "Select difficulty", "design_x": list(wide.DESIGN_X), "frame": [800, 600],
            "ground": W.ground_meta(g), "draw": draw}


if __name__ == "__main__":
    args = sys.argv[1:]
    t0 = time.time()
    if "--fit" in args or "--refit" in args:
        fit_all("--refit" in args)
    if "--deliver" in args:
        sc = build_scene()
        json.dump(sc, open(os.path.join(OUT, "diff-wide.json"), "w"))
        print(f"scene: {len(sc['draw'])} entries, {os.path.getsize(os.path.join(OUT, 'diff-wide.json')) // 1024} KB")
        wide.deliver(sc, OUT, "diff", art(), refs(), zoom_rows=(120, 560))
    print(f"{time.time() - t0:.0f}s")
