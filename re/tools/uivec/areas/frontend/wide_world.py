"""P41W: the world map (WorldBase), wide. Usage: py -3.12 wide_world.py [--refit] [--fit] [--deliver]

Read on the art (2026-11-25):
- the navy region on the left is a disc, c (-37.77, 477.0) r 541.91 (8 boundary points read where no capsule covers it, all
  within 1.4 px), soft-edged into the cloud; it runs off the left, top and bottom of the art: drawn as the disc united with the
  half plane left of its centre (it continues to any left edge);
- the cloud ground on the right (span);
- the navy blob at the bottom right holding the back button: an axis-aligned ellipse c (796.21, 528.25) r (108.17, 143.17)
  (94 edge points, within 0.9 px); it runs off the right and bottom edges: drawn as the ellipse united with the band right of
  its centre under its top tangent (y 385.08), so at any width it continues along the bottom right, the back button still on it;
- the line from the lowest right capsule: the circle c (141.89, 455.10) r 528.59 (centres of the line in 45 rows, rms 0.16 px)
  down to y 477.0, a round corner, right to a T-cap at x 728.5 (y 468..488); 1.5 px navy with a flat shadow below;
- two short plate wires, 1.3 px navy with a flat shadow 2-6 rows below: y 31.9 (x 228..252) and y 563.1 (x 362..470);
- the two chains the capsules hang on (polylines through their dark ends, seen in the gaps) and the wire between the two top
  capsules (y 32); the approved capsules (with their own shadows), plates, oval and
  buttons (world-backdrop + world-back) on top; the globe and its own shadow are art, lifted with a ground_diff matte.
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

SCREEN = "world"
OUT, CACHE = W.dirs(SCREEN)
SHEET = "Interface/WorldBase.pcx"
DISC_C, DISC_R = (-37.77, 477.0), 541.91
BLOB = (796.21, 528.25, 108.17, 143.17)
RLINE_C, RLINE_R = (141.89, 455.10), 528.59
RLINE_Y, CAP_X, CAP_Y = 477.0, 728.5, (468.0, 488.0)
WIRES = [((228.0, 31.9), (252.0, 31.9)), ((362.0, 563.1), (470.0, 563.1)), ((418.0, 32.0), (470.0, 32.0))]
NAVY_LINE = [33, 43, 79]
GLOBE_CUT = [4, 108, 424, 424]
GLOBE_MATTE = {"kind": "ground_diff", "t0": 9.0, "t1": 30.0, "feather": 4.0}


def art():
    return W.sheet(SHEET)[0]


def refs():
    return {"world-backdrop": wide.load_ref("world-backdrop"), "world-back": wide.load_ref("world-back")}


def items():
    r = refs()
    return r["world-backdrop"] + r["world-back"]


def disc_shape():
    return {"t": "union", "of": [{"t": "circle", "c": list(DISC_C), "r": DISC_R}, {"t": "half", "p": [DISC_C[0], 0.0], "n": [1.0, 0.0]}]}


def blob_shape():
    cx, cy, rx, ry = BLOB
    return {"t": "union", "of": [{"t": "ellipse", "c": [cx, cy], "r": [rx, ry], "rot": 0.0},
                                 {"t": "inter", "of": [{"t": "half", "p": [cx, 0.0], "n": [-1.0, 0.0]},
                                                       {"t": "half", "p": [0.0, cy - ry], "n": [0.0, -1.0]}]}]}


def rline_shape(w=1.5, rf=5.0):
    """Down the circle's right side from under the capsule to a round corner, then right onto the blob to the T-cap (drawn
    over the blob, as in the art)."""
    cx, cy = RLINE_C
    R = RLINE_R
    fy = RLINE_Y - rf
    fx = cx + math.sqrt((R + rf) ** 2 - (fy - cy) ** 2)
    v = (cx - fx, cy - fy)
    n = math.hypot(*v)
    tc = (fx + rf * v[0] / n, fy + rf * v[1] / n)
    a_end = math.degrees(math.atan2(tc[1] - cy, tc[0] - cx))
    y0 = 318.0
    a_start = math.degrees(math.atan2(y0 - cy, math.sqrt(R * R - (y0 - cy) ** 2)))
    parts = [W.arc_short((cx, cy), R, a_start, a_end, w),
             W.arc_short((fx, fy), rf, math.degrees(math.atan2(v[1], v[0])), 90.0, w),
             {"t": "seg", "a": [round(fx, 3), RLINE_Y], "b": [CAP_X, RLINE_Y], "w": w},
             {"t": "seg", "a": [CAP_X, CAP_Y[0]], "b": [CAP_X, CAP_Y[1]], "w": 2.0}]
    return {"t": "union", "of": parts}


def wire_shapes():
    return [{"t": "seg", "a": list(a), "b": list(b), "w": 1.3} for a, b in WIRES]


def spine_shapes():
    """The two chains the capsules hang on: polylines through the centres of the capsules' dark rounded ends (the end's
    rrect c, half 32 x 22, r 22: its left round's centre is c - (10, 0)), visible only in the gaps between capsules. Checked
    on the gap under 'capsule 511,338': the chain predicts x 481.5 at y 392, the art's connector is at x 481-482."""
    chains = {"left": [], "right": []}
    for it in refs()["world-backdrop"]:
        nm = it.get("name") or ""
        if not nm.startswith("capsule"):
            continue
        S = it["layers"][0]["shape"]["of"]
        cx, cy = S["c"]
        chains["right" if cx > 520 or (cx > 490 and cy < 60) else "left"].append((cx - 10.0, cy))
    out = []
    for k, pts in chains.items():
        pts = sorted(pts, key=lambda p: p[1])
        out.append({"t": "stroke", "pts": [[round(p[0], 2), round(p[1], 2)] for p in pts], "w": 1.4, "cap": "butt"})
    return out


def capsule_shapes():
    out = []
    for it in refs()["world-backdrop"]:
        if (it.get("name") or "").startswith("capsule"):
            S = it["layers"][0]["shape"]
            out.append(S["a"] if S["t"] == "sub" else S)
    return out


def fit_all(refit_ground=False):
    a = art()
    x, y = W.grid()
    E = W.coverage(items(), 0)
    dd = sdf.sd(disc_shape(), x, y)
    bd = sdf.sd(blob_shape(), x, y)
    lines = [rline_shape()] + wire_shapes() + spine_shapes()
    ld = np.min([sdf.sd(S, x, y) for S in lines], 0)
    globe = np.zeros((600, 800), bool)
    gx, gy, gw, gh = GLOBE_CUT
    globe[gy:gy + gh, gx:gx + gw] = True
    ground = W.cloud_pixels(a) & ~W.dilate(E, 12) & (dd > 5) & (bd > 5) & (ld > 7) & ~globe
    g = W.fit_ground(SCREEN, a, ground, refit=refit_ground)
    G = wide.ground_1x({"draw": W.ground_entries(g)})
    res = {}
    navy_disc = (dd < -12) & ~W.dilate(E, 8) & ~globe & (ld > 6)
    res["navy"] = [round(float(v), 1) for v in np.median(a[navy_disc], 0)]
    nb = (bd < -12) & ~W.dilate(E, 8) & (ld > 6)
    res["blob_navy"] = [round(float(v), 1) for v in np.median(a[nb], 0)]
    print("navy", res["navy"], "blob", res["blob_navy"])
    # the soft edges: the navy shapes' soft width, by least squares over a few values
    U = G.copy()
    best = None
    for soft in (0.8, 1.2, 1.6, 2.0, 2.5, 3.0, 3.5):
        img = G.copy()
        sdf.render(navy_entries(res, soft), img, 1.0)
        band = (np.abs(dd) < 6) & ~W.dilate(E, 4) & ~globe & (ld > 5) | (np.abs(bd) < 6) & ~W.dilate(E, 4) & (ld > 5)
        e = float(((img - a)[band] ** 2).mean())
        if best is None or e < best[0]:
            best = (e, soft)
    res["navy_soft"] = best[1]
    print("navy soft", best)
    under = G.copy()
    sdf.render(navy_entries(res, res["navy_soft"]), under, 1.0)
    over_l = W.overlay([{"box": [-10, -10, 810, 610], "layers": [{"shape": S, "fill": NAVY_LINE} for S in lines]}])
    lb = (ld > -1.0) & (ld < 9) & ~W.dilate(E, 3)
    res["line_shadow"] = W.fit_shadow(lines, lb, under, a, {"dx": 0.0, "dy": 3.5, "g": 1.0, "soft": 1.0, "alpha": 0.2}, over_l, name="lines")
    caps = capsule_shapes()
    cd = np.min([sdf.sd(S, x, y) for S in caps], 0)
    full = under.copy()
    sdf.render([{"box": [-10, -10, 810, 610], "layers": [W.shadow_layer(S, res["line_shadow"]) for S in lines] +
                 [{"shape": S, "fill": NAVY_LINE} for S in lines]}], full, 1.0)
    over = W.overlay(items())
    others = W.coverage([it for it in items() if not (it.get("name") or "").startswith("capsule")], 2)
    cb = (cd > -0.5) & (cd < 16) & ~others & (ld > 4) & ~globe
    res["capsule_shadow"] = W.fit_shadow(caps, cb, full, a, {"dx": 3.0, "dy": 4.0, "g": 0.0, "soft": 4.0, "alpha": 0.4}, over, name="capsules")
    res["capsules"] = caps
    json.dump(res, open(os.path.join(CACHE, "fit.json"), "w"), indent=1)
    return res


def navy_entries(res, soft):
    return [{"name": "navy disc", "anchor": "centre", "box": [wide.SPAN_X[0], -40.0, DISC_C[0] + DISC_R + 8, 640.0],
             "layers": [{"shape": disc_shape(), "fill": res["navy"], "soft": soft}]},
            {"name": "navy blob", "anchor": "centre", "box": [BLOB[0] - BLOB[2] - 8, BLOB[1] - BLOB[3] - 8, wide.SPAN_X[1], 640.0],
             "layers": [{"shape": blob_shape(), "fill": res["blob_navy"], "soft": soft}]}]


def build_scene():
    res = json.load(open(os.path.join(CACHE, "fit.json")))
    g = W.fit_ground(SCREEN, art(), None)
    draw = W.ground_entries(g)
    draw += navy_entries(res, res["navy_soft"])
    # the approved capsules carry their own shadow (layers 0-1, moved 2.5, 3.5): no extra capsule shadow here
    lines = [rline_shape()] + wire_shapes() + spine_shapes()
    draw.append({"name": "globe (art)", "anchor": "centre",
                 "picture": {"sheet": SHEET, "rect": GLOBE_CUT, "at": GLOBE_CUT[:2], "matte": dict(GLOBE_MATTE)}})
    draw.append({"name": "lines: capsule chains, right capsule to the blob (T-cap), plate wires", "anchor": "centre", "box": [200.0, 10.0, 800.0, 580.0],
                 "layers": [W.shadow_layer(S, res["line_shadow"]) for S in lines] + [{"shape": S, "fill": NAVY_LINE} for S in lines]})
    draw.append({"name": "capsules, plates, oval", "anchor": "centre", "ref": "world-backdrop"})
    draw.append({"name": "back button", "anchor": "centre", "ref": "world-back"})
    return {"id": "world-wide", "screen": "Routing map / world", "design_x": list(wide.DESIGN_X), "frame": [800, 600],
            "ground": W.ground_meta(g), "draw": draw}


if __name__ == "__main__":
    args = sys.argv[1:]
    t0 = time.time()
    if "--fit" in args or "--refit" in args:
        fit_all("--refit" in args)
    if "--deliver" in args:
        sc = build_scene()
        json.dump(sc, open(os.path.join(OUT, "world-wide.json"), "w"))
        print(f"scene: {len(sc['draw'])} entries")
        wide.deliver(sc, OUT, "world", art(), refs(), zoom_rows=(100, 560))
    print(f"{time.time() - t0:.0f}s")
