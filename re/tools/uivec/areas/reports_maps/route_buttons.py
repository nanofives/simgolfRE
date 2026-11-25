"""Routing map mode buttons (route screens_bottom.pcx cells, idle row 0, selected row 114 / 118), read 2026-11-24.

The bubble IS a ringed glass bubble (radial profile, median over 120 directions, course and aura cells):
  a navy outer rim (rim circle fit r 35.15 idle / 35.02 selected, residual 0.3), a lit glass band inside it (light at r 30),
  a thin navy inner ring (r 27.3, ~1.6 px), the body inside (r < 26.5) lit, the navy glyph. Every paint (band, body) is solved
  by least squares per colour (blue idle, yellow selected) over the four buttons; the glyph pixels are left out of the body.
The arch piece around it in the cell is drawn by the bottom panel (route-bottom); here the cell's own pixels are kept round
the bubble (wiped under it).
The clover (aura) IS four heart leaves pointing at the centre (top, left, right, bottom; each two round lobes over a
triangle) and a stem curving down to the lower right.
"""
import json
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import common as C      # noqa: E402
import fit              # noqa: E402
import glyphs           # noqa: E402
import lit              # noqa: E402

RB = "route screens_bottom.pcx"
NAVY = [33, 41, 82]
# centres: the inner ring's circle fit (dark pixels at r 25.8..29, residual 0.4 px); the outer rim's own fit is pulled by the
# dark arch beside it. Outer rim: its line centre at r 34.4, outer edge 35.4.
CENTRES = {("staff", "idle"): (42.21, 41.77, 35.4), ("staff", "selected"): (42.21, 156.77, 35.4),
           ("value", "idle"): (216.01, 41.99, 35.4), ("value", "selected"): (216.0, 156.97, 35.4),
           ("course", "idle"): (303.94, 40.5, 35.4), ("course", "selected"): (303.94, 159.5, 35.4),
           ("aura", "idle"): (487.99, 40.99, 35.4), ("aura", "selected"): (488.0, 160.0, 35.4)}
CELLS = {"staff": (0, 0, 129, 113), "value": (130, 0, 130, 113), "course": (261, 0, 134, 117), "aura": (396, 0, 139, 117)}
SEL_DY = {"staff": 114, "value": 114, "course": 118, "aura": 118}
RING = {"band": 33.3, "ring": 27.6, "ring_w": 2.0, "body": 26.6}
STYLE_FILE = os.path.join(C.OUT, "read", "route_buttons_style.json")


def circ(c, r):
    return {"t": "circle", "c": [c[0], c[1]], "r": r}


def heart(l1, l2, tip):
    """Two round lobes (x, y, r) and the point they run down to."""
    import math
    (x1, y1, r1), (x2, y2, r2) = l1, l2
    # the triangle from the lobes' far sides to the tip
    dx, dy = x2 - x1, y2 - y1
    n = math.hypot(dx, dy) or 1.0
    px, py = -dy / n, dx / n
    mx, my = (x1 + x2) / 2, (y1 + y2) / 2
    if (tip[0] - mx) * px + (tip[1] - my) * py < 0:
        px, py = -px, -py
    a = [x1 - dx / n * r1 * 0.95, y1 - dy / n * r1 * 0.95]
    b = [x2 + dx / n * r2 * 0.95, y2 + dy / n * r2 * 0.95]
    return {"t": "union", "of": [circ((x1, y1), r1), circ((x2, y2), r2), {"t": "poly", "rings": [[a, b, list(tip)]]}]}


def clover():
    """aura idle coordinates (the glyph coverage dump of 2026-11-24)."""
    return {"t": "union", "of": [
        # (read with the bubble centre at (488.57, 41.53); the glyph is moved with the centre below)
        # the leaves meet in a solid middle (a first reading let their points miss each other: white slits the art has not)
        circ((489.0, 41.0), 6.5),
        heart((484.0, 26.8, 5.6), (493.5, 26.8, 5.6), (489.0, 41.0)),        # top leaf, notch at (488.8, 21)
        heart((474.0, 35.2, 5.6), (473.6, 42.3, 5.8), (489.0, 41.0)),        # left leaf
        heart((503.8, 35.4, 5.6), (504.0, 42.3, 5.8), (489.0, 41.0)),        # right leaf
        heart((481.4, 52.6, 5.6), (486.2, 55.8, 5.1), (489.0, 41.0)),        # bottom leaf
        {"t": "qstroke", "p": [[490.5, 44.0], [496.4, 50.0], [496.6, 61.5]], "w": [6.0, 7.0]},   # stem, down to the right
    ]}


def curve(pts, w, rounds=2):
    """A curved stroke with flat ends square to it, as one polygon (digits.band): sdf's flat cap cuts a whole half plane,
    which takes bites out of a curve that turns back (the S of the dollar)."""
    import digits
    for _ in range(rounds):
        pts = [pts[0]] + [[a[0] * (1 - f) + b[0] * f, a[1] * (1 - f) + b[1] * f] for a, b in zip(pts, pts[1:])
                          for f in (0.25, 0.75)] + [pts[-1]]
    return digits.band(pts, w)


def house():
    """value idle coordinates (coverage runs of 2026-11-24): a solid navy house (roof from the apex (214.8, 20.2) down to the
    eaves at y 44.6, x 192.9 and 235.2; a chimney x 223.3..227.7 from y 21.7; walls x 197.3..230.4 down to y 62.4 (a white gap shows below them, above the
    inner ring) with a white dollar cut out of it: a vertical bar x 211.9..215.2, y 27.6..57.6, and an S stroke
    (4.5 px) from its upper-right terminal over the top, down the left, across the middle, down the right, under the
    bottom to its lower-left terminal."""
    body = {"t": "union", "of": [
        {"t": "poly", "rings": [[[214.8, 20.2], [235.2, 44.6], [230.4, 44.6], [230.4, 62.4], [197.3, 62.4], [197.3, 44.6],
                                 [192.9, 44.6]]]},
        {"t": "rrect", "c": [225.5, 27.0], "half": [2.2, 5.3], "r": 0.0, "rot": 0.0}]}
    dollar = {"t": "union", "of": [
        {"t": "rrect", "c": [213.55, 42.6], "half": [1.65, 15.0], "r": 0.0, "rot": 0.0},
        curve([[222.0, 36.2], [219.5, 33.6], [213.5, 33.2], [208.0, 34.8], [207.0, 38.5], [209.5, 41.8], [214.0, 43.3],
               [218.5, 44.8], [220.6, 47.5], [219.5, 50.8], [213.5, 52.2], [208.0, 51.6], [205.5, 49.6]], 5.0)]}
    return {"t": "sub", "a": body, "b": dollar}


def R_(x0, y0, x1, y1, r=0.0):
    return {"t": "rrect", "c": [(x0 + x1) / 2, (y0 + y1) / 2], "half": [(x1 - x0) / 2, (y1 - y0) / 2], "r": r, "rot": 0.0}


def poly(*p):
    return {"t": "poly", "rings": [[list(q) for q in p]]}


def line(*p, w=3.0):
    return {"t": "stroke", "pts": [list(q) for q in p], "w": w, "cap": "butt"}


def flag():
    """course idle coordinates (read on a 10x grid, 2026-11-24). What it IS, as drawn (the meaning of the two side marks is
    not certain): a pole (x 300.2..304.5, y 18.5..59.5) with a light core line and a small foot; a pennant from the pole's
    top pointing right (tip (322.8, 27.5)); at the left an arc rising from (282.7, 43) to a small hook at the top and a short
    stroke from it up to the pole; at the right a stroke from the pole down to the lower right ending on a block."""
    pole = {"t": "sub", "a": R_(300.2, 18.5, 304.5, 59.5), "b": R_(301.9, 21.0, 302.8, 57.0)}     # the light core line
    return {"t": "union", "of": [
        pole,
        poly((297.6, 62.2), (299.2, 59.0), (305.6, 59.0), (307.2, 62.2)),
        poly((304.3, 19.4), (312.0, 21.6), (322.8, 27.4), (314.0, 29.2), (304.3, 31.8)),
        curve([[282.6, 43.0], [283.4, 37.0], [286.0, 31.6], [290.2, 27.6], [294.6, 25.4], [297.4, 26.6]], 2.9),
        line((287.6, 39.4), (299.6, 33.6), w=3.0),
        line((305.0, 34.0), (321.6, 48.2), w=3.4),
        poly((305.4, 48.0), (318.6, 46.6), (317.6, 58.6), (305.4, 58.6)),
        poly((305.4, 47.4), (308.8, 47.4), (309.8, 50.2), (305.4, 50.2)),       # the block's left horn
    ]}


def staff():
    """staff idle coordinates (read on a 10x grid, 2026-11-24): a standing worker seen from the front, hands on his hips,
    holding a spade: a cap with a brim to the left, a round head, the torso (with a white notch at the collar), the left arm
    bent out to the elbow with the white gap inside its loop, the right arm out to the spade's handle, two legs with a white
    gap between, the ground under his feet; the spade: a straight handle and a pointed blade on top."""
    body = {"t": "union", "of": [
        R_(36.0, 17.0, 46.2, 22.6, 1.5), R_(32.8, 20.4, 38.0, 22.8, 0.8),                 # cap, brim
        {"t": "ellipse", "c": [41.2, 25.8], "r": [4.0, 3.9], "rot": 0.0},                   # head
        poly((35.0, 29.0), (47.0, 29.0), (48.6, 44.6), (33.4, 44.6)),                       # torso
        poly((35.6, 29.8), (28.2, 36.6), (34.4, 43.6), (35.8, 41.0)),                       # left arm, elbow out
        poly((46.4, 29.6), (53.2, 34.0), (53.2, 38.0), (48.2, 40.8)),                       # right arm to the handle
        R_(33.4, 44.0, 40.2, 59.6), R_(42.0, 44.0, 48.6, 59.6),                            # legs
        R_(22.0, 59.2, 62.0, 63.4),                                                         # ground
        R_(54.4, 33.0, 56.4, 59.6),                                                         # spade handle
        poly((55.4, 21.8), (58.4, 26.0), (58.2, 33.2), (52.6, 33.2), (52.4, 26.0)),         # spade blade
    ]}
    cuts = {"t": "union", "of": [poly((40.4, 29.0), (43.0, 29.0), (41.7, 31.8)),            # collar notch
                                 poly((34.0, 33.6), (31.2, 36.8), (34.2, 40.2))]}           # the arm's loop
    return {"t": "sub", "a": body, "b": cuts}


GLYPHS = {"aura": clover, "value": house, "course": flag, "staff": staff}


def glyph(name, state):
    g = GLYPHS[name]()
    if state == "idle":
        return g
    a, b = CENTRES[(name, "idle")], CENTRES[(name, state)]
    return {"t": "move", "of": g, "d": [b[0] - a[0], b[1] - a[1]]}


def layers(c, st, colour, g):
    cx, cy, R = c
    s = st[colour]
    L = [{"shape": circ((cx, cy), R), "fill": s["rim"]},
         {"shape": circ((cx, cy), RING["band"]), "fill": {"bevel": {"L": RING["band"] - RING["ring"], "stops": s["band"]["stops"],
                                                                    "coef": s["band"]["coef"]}}},
         {"shape": circ((cx, cy), RING["ring"] + RING["ring_w"] / 2), "fill": s["rim"]},
         {"shape": circ((cx, cy), RING["body"]), "fill": {"bevel": {"L": RING["body"], "stops": s["body"]["stops"],
                                                                    "coef": s["body"]["coef"]}}}]
    if g is not None:
        L.append({"shape": g, "fill": s["glyph"], "clip": circ((cx, cy), RING["body"])})
    return L


def solve(names=("aura",)):
    rgb, m = C.sheet(RB)
    st = {}
    for colour, state in (("blue", "idle"), ("yellow", "selected")):
        band_rows, body_rows, rims, gl = [], [], [], []
        for name in ("staff", "value", "course", "aura"):
            cx, cy, R = CENTRES[(name, state)]
            x0, y0, x1, y1 = int(cx - R - 2), int(cy - R - 2), int(cx + R + 3), int(cy + R + 3)
            jj, ii = np.mgrid[y0:y1, x0:x1]
            x, y = ii + 0.5, jj + 0.5
            r = np.hypot(x - cx, y - cy)
            art = rgb[y0:y1, x0:x1]
            nx, ny = (x - cx) / np.maximum(r, 1e-9), (y - cy) / np.maximum(r, 1e-9)
            mb = (r > RING["ring"] + RING["ring_w"] / 2 + 0.6) & (r < RING["band"] - 0.6)
            band_rows.append((((RING["band"] - r) / (RING["band"] - RING["ring"]))[mb], nx[mb], ny[mb], art[mb]))
            cov, lo, hi = fit.dark_coverage(art, r < RING["body"] - 0.5)
            mk = (r < RING["body"] - 0.6) & (cov < 0.05)
            # a pixel next to the glyph is left out too (its blur)
            g = cov > 0.05
            for dj, di in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                mk &= ~np.roll(np.roll(g, dj, 0), di, 1)
            body_rows.append((((RING["body"] - r) / RING["body"])[mk], nx[mk], ny[mk], art[mk]))
            rims.append(art[(r > 33.4) & (r < 34.6)])
            gl.append(art[cov > 0.9])
        bs, ds = lit.pixel_stops(RING["band"] - RING["ring"]), [0.02, 0.06, 0.12, 0.2, 0.3, 0.45, 0.65, 0.85, 1.0]
        st[colour] = {"band": {"stops": bs, "coef": C.solve_bevel_near(band_rows, bs, dir_until=1.0, lam_k=1e-4)[0]},
                      "body": {"stops": ds, "coef": C.solve_bevel_near(body_rows, ds, dir_until=1.0, lam_k=1e-4)[0]},
                      "rim": [round(float(v), 1) for v in np.median(np.concatenate(rims), axis=0)],
                      "glyph": [round(float(v), 1) for v in np.median(np.concatenate(gl), axis=0)]}
    json.dump(st, open(STYLE_FILE, "w"))
    return st


def tune_glyph(name):
    rgb, m = C.sheet(RB)
    cx, cy, R = CENTRES[(name, "idle")]
    box = (int(cx - 27), int(cy - 27), int(cx + 28), int(cy + 28))
    jj, ii = np.mgrid[box[1]:box[3], box[0]:box[2]]
    inside = np.hypot(ii + 0.5 - cx, jj + 0.5 - cy) < 25.5
    cov, lo, hi = fit.dark_coverage(rgb[box[1]:box[3], box[0]:box[2]], inside)
    g = GLYPHS[name]()
    i0 = glyphs.iou(glyphs.shape_coverage(g, box, inside, s=3), cov) if hasattr(glyphs, "iou") else None
    f0, f1, k = glyphs.fit_shape(g, cov, inside, box, passes=5, reach=1.5)
    sc = glyphs.shape_coverage(g, box, inside, s=3)
    i1 = float(np.minimum(cov, sc).sum() / max(np.maximum(cov, sc).sum(), 1e-9))
    return g, i0, i1


if __name__ == "__main__":
    import deliver as D
    st = solve()
    name = sys.argv[1] if len(sys.argv) > 1 else "aura"
    g, i0, i1 = tune_glyph(name)
    print("glyph iou", i0, "->", i1)
    GLYPHS[name] = lambda g=g: g
    rgb, m = C.sheet(RB)
    states = []
    for state, colour in (("idle", "blue"), ("selected", "yellow")):
        x, y, w, h = CELLS[name]
        if state == "selected":
            y += SEL_DY[name]
        art = rgb[y:y + h, x:x + w].copy()
        mk = m[y:y + h, x:x + w]
        art[~mk] = [70, 110, 90]
        c = CENTRES[(name, state)]
        base = C.wipe(art, circ(c[:2], c[2] + 1.5), origin=(x, y))
        base[~mk] = [70, 110, 90]
        states.append((state, art, base, [{"name": "bubble", "box": [c[0] - 37, c[1] - 37, c[0] + 37, c[1] + 37],
                                            "layers": layers(c, st, colour, glyph(name, state))}], (x, y)))
    WHAT = {"aura": "the four-leaf clover authored as four heart leaves (two round lobes over a triangle each) meeting in a solid "
                    "middle and a stem curving to the lower right",
            "value": "the house authored as one navy silhouette (roof, chimney, walls) with the dollar cut out of it (a vertical "
                     "bar and an S stroke)",
            "course": "the flag authored from its parts: pole with a light core and a foot, a pennant pointing right, an arc and a "
                      "short stroke at the left, a stroke to a block at the lower right",
            "staff": "the worker authored from his parts: cap and brim, head, torso with the collar notch, arms on the hips (the "
                     "left one's loop open), two legs, the ground, and the spade (handle, pointed blade)"}
    D.deliver(f"route-{name}-button", states,
              "the ringed glass bubble (navy outer rim r 35.4, a lit glass band, a thin navy inner ring r 27.6, the lit body; "
              "paints solved per colour over the four buttons) with " + WHAT[name] + f" (tuned under 1.5 px, iou {i1:.2f})",
              unsure="the arch piece round the bubble is the cell's own art here (it is redrawn in route-bottom)"
                     + ("; what the arc and the block beside the flag depict is not certain (drawn as read)" if name == "course" else "")
, z=3)
