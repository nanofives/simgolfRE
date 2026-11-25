"""Buy Land lot selection: a raised yellow iso diamond (one lot, 192 x 95) drawn over the chosen lot. Read 2026-11-24 from
buy_land_buttons.pcx (cell 1, 0x7f, 0xc0 x 0x5f): its four edges are the opaque mask's row ends (line fits); the paint is a
bevel (pale cream along the two upper edges, deeper orange-yellow along the two lower ones, bright yellow in the middle),
measured by least squares over its pixels."""
import json
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import common as C      # noqa: E402
import accept           # noqa: E402
import sdf              # noqa: E402

STOPS = [0.0, 0.03, 0.08, 0.15, 0.25, 0.4, 0.6, 0.8, 1.0]
BOX = (1, 0x7f, 0xc0, 0x5f)


def measure():
    rgb, m = C.sheet("buy_land_buttons.pcx")
    x, y, w, h = BOX
    mk = m[y:y + h, x:x + w]
    pts = {"ul": [], "ur": [], "ll": [], "lr": []}
    rows = np.nonzero(mk.any(1))[0]
    mid = (rows[0] + rows[-1] + 1) / 2
    for j in rows:
        xs = np.nonzero(mk[j])[0]
        yy = y + j + 0.5
        side = "u" if yy < y + mid - 1 else ("l" if yy > y + mid + 1 else None)
        if side is None:
            continue
        pts[side + "l"].append((x + xs.min(), yy))
        pts[side + "r"].append((x + xs.max() + 1, yy))
    lines = {}
    for k, p in pts.items():
        p = np.array(p, float)
        A = np.c_[p[:, 1], np.ones(len(p))]
        lines[k] = np.linalg.lstsq(A, p[:, 0], rcond=None)[0]

    def meet(a, b):        # x = k1 y + b1 = k2 y + b2
        (k1, b1), (k2, b2) = lines[a], lines[b]
        yy = (b2 - b1) / (k1 - k2)
        return [float(k1 * yy + b1), float(yy)]
    return [meet("ul", "ur"), meet("ur", "lr"), meet("lr", "ll"), meet("ll", "ul")], lines


def scene(poly, coef, L):
    S = {"t": "poly", "rings": [poly]}
    xs, ys = [p[0] for p in poly], [p[1] for p in poly]
    return [{"name": "selection diamond", "box": [min(xs) - 1, min(ys) - 1, max(xs) + 1, max(ys) + 1],
             "layers": [{"shape": S, "fill": {"bevel": {"L": L, "stops": STOPS, "coef": coef}}}]}]


def paint(poly):
    rgb, m = C.sheet("buy_land_buttons.pcx")
    x, y, w, h = BOX
    jj, ii = np.mgrid[y:y + h, x:x + w]
    S = {"t": "poly", "rings": [poly]}
    d = sdf.sd(S, ii + 0.5, jj + 0.5)
    nx, ny = sdf._normal(d)
    L = float(-d.min())
    mask = (d < -0.5) & m[y:y + h, x:x + w]
    coef, n = C.solve_bevel_near([((-d / L)[mask], nx[mask], ny[mask], rgb[y:y + h, x:x + w][mask])], STOPS, dir_until=0.25)
    return coef, L


if __name__ == "__main__":
    poly, lines = measure()
    print("diamond", [[round(a, 2) for a in p] for p in poly], {k: [round(float(v), 4) for v in l] for k, l in lines.items()})
    coef, L = paint(poly)
    C.save_json({"poly": poly, "coef": coef, "L": L}, os.path.join(C.OUT, "read", "sel_fit.json"))


# ---- second reading (the first, a bevel by edge direction, painted seams along the diamond's diagonals and the middle row,
# which the art does not have): the body is lit by DEPTH only (no direction), and the light comes from two rim strokes:
# a pale line along the two upper edges, a deeper orange line along the two lower edges.
def scene2(poly, st):
    S = {"t": "poly", "rings": [poly]}
    top, right, bottom, left = poly
    xs, ys = [p[0] for p in poly], [p[1] for p in poly]
    lay = [{"shape": S, "fill": {"bevel": {"L": st["L"], "stops": STOPS, "coef": st["coef"]}}}]
    for k, pts in (("hi", [left, top, right]), ("lo", [left, bottom, right])):
        r = st[k]
        lay.append({"shape": {"t": "stroke", "pts": pts, "w": r["w"], "cap": "butt"}, "fill": r["rgb"], "alpha": r["a"],
                    "soft": r["soft"], "clip": S})
    return [{"name": "selection diamond", "box": [min(xs) - 1, min(ys) - 1, max(xs) + 1, max(ys) + 1], "layers": lay}]


def fit2(poly):
    import tunefit as T
    rgb, m = C.sheet("buy_land_buttons.pcx")
    x, y, w, h = BOX
    art = rgb[y:y + h, x:x + w]
    mk = m[y:y + h, x:x + w]
    jj, ii = np.mgrid[y:y + h, x:x + w]
    S = {"t": "poly", "rings": [poly]}
    d = sdf.sd(S, ii + 0.5, jj + 0.5)
    L = float(-d.min())
    st = {"L": L, "hi": {"w": 6.0, "rgb": [255, 245, 190], "a": 0.8, "soft": 1.5},
          "lo": {"w": 6.0, "rgb": [200, 140, 0], "a": 0.6, "soft": 1.5}}
    mask = (d < -0.5) & mk

    def solve():       # the depth paint under the strokes, as they are now
        st["coef"] = STOPS and C.solve_bevel_near([((-d / L)[mask], np.zeros(mask.sum()), np.zeros(mask.sum()),
                                                    art[mask])], STOPS, dir_until=-1)[0]
    solve()
    base = np.zeros_like(art)

    def loss():
        low = T.render_low(scene2(poly, st), base, (x, y), 2)
        return float(((low - art) ** 2)[mask].mean())
    P = []
    for k in ("hi", "lo"):
        r = st[k]
        P += [[r, "w", 0.5, 1, 16, 6], [r, "a", 0.05, 0, 1, 1], [r, "soft", 0.25, 0.3, 6, 4]] + [[r["rgb"], c, 8, 0, 255, 120] for c in range(3)]
    T.descend(loss, P, passes=4, verbose=True, label="selection")
    return st
