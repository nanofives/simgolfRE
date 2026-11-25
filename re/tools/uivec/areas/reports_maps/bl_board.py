"""Buy Land lot board: an iso diamond (2:1) frame lit from the left, a black band, a 3 x 3 board of lots (the four corner
lots and ... cream, the four side lots tan, the centre black), over the marbled ball (kept art).

Read 2026-11-24 from buy_land.pcx (edge points per row, line fits):
  black diamond   centre (400, 439.5), half 312 x 156: its four edges are exact 2:1 pixel staircases (fit residual 0.00)
  lots            centre (400, 439.5), half 288 x 144: 9 lots of 192 x 96 (each 9216 px: 4 cream + 4 tan + the black centre)
  frame outer     the four outer edges fit lines of slope 2.00 (res 0.3 px): top (400.2, 256.2), left (34.3, 439.1),
                  right (763.7, 437.9), bottom (397.9, 620.9) -> about centre (399.2, 438.6), half 364.7 x 182.35
  frame paint     a bevel by edge direction: edges facing left / up-left are lit (a pale line at the outer upper-left
                  edge, light at the inner right edge), edges facing right / down-right dark (a 10 px dark band along the
                  outer lower right, a soft shade at the inner left edge); measured by least squares (accept.solve_bevel).
"""
import copy
import json
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import common as C      # noqa: E402
import accept           # noqa: E402
import sdf              # noqa: E402
import tunefit as T     # noqa: E402

CREAM, TAN, BLACK = [255, 247, 231], [222, 198, 156], [0, 0, 0]
GEOM = {"outer": [399.2, 438.6, 364.7, 182.35], "black": [400.0, 439.5, 312.0, 156.0], "lots": [400.0, 439.5, 288.0, 144.0]}
STOPS = [0.0, 0.05, 0.12, 0.22, 0.35, 0.5, 0.7, 1.0]
L_FRAME = 11.6      # the band's half width across its edges (52 px wide along a row = 23.3 px across)
STYLE_FILE = os.path.join(C.OUT, "read", "board_style.json")


def diamond(cx, cy, hx, hy):
    return [[cx, cy - hy], [cx + hx, cy], [cx, cy + hy], [cx - hx, cy]]


def lot_poly(g, i, j):
    """Lot (i, j): i along the lower-right axis (from the top vertex), j along the lower-left axis."""
    cx, cy, hx, hy = g["lots"]
    top = np.array([cx, cy - hy])
    ex, ey = np.array([hx, hy]) / 3, np.array([-hx, hy]) / 3     # one lot along each axis
    p = top + i * ex + j * ey
    return [list(p), list(p + ex), list(p + ex + ey), list(p + ey)]


KIND = {(0, 0): CREAM, (2, 0): CREAM, (0, 2): CREAM, (2, 2): CREAM, (1, 1): BLACK,
        (1, 0): TAN, (0, 1): TAN, (2, 1): TAN, (1, 2): TAN}


def scene(g, st):
    outer, black = diamond(*g["outer"]), diamond(*g["black"])
    band = {"t": "poly", "rings": [outer, black]}
    dr = st.get("drop", {"d": [2.0, 2.0], "g": 0.0, "soft": 3.0, "a": 0.35, "rgb": [10, 14, 30]})
    lay = [{"shape": {"t": "move", "of": {"t": "grow", "of": {"t": "poly", "rings": [outer]}, "d": dr["g"]}, "d": dr["d"]},
            "fill": dr["rgb"], "alpha": dr["a"], "soft": dr["soft"]},
           {"shape": band, "fill": {"bevel": {"L": L_FRAME, "stops": STOPS, "coef": st["frame"]}}},
           {"shape": {"t": "poly", "rings": [black]}, "fill": BLACK}]
    for (i, j), col in KIND.items():
        if col != BLACK:
            lay.append({"shape": {"t": "poly", "rings": [lot_poly(g, i, j)]}, "fill": col})
    cx, cy, hx, hy = g["outer"]
    return [{"name": "lot board", "box": [cx - hx - 2, max(cy - hy - 2, 0), cx + hx + 2, min(cy + hy + 2, 600)], "layers": lay}]


def measure_frame(g):
    rgb, m = C.sheet("buy_land.pcx")
    x0, y0, x1, y1 = 30, 252, 770, 600
    jj, ii = np.mgrid[y0:y1, x0:x1]
    x, y = ii + 0.5, jj + 0.5
    band = {"t": "poly", "rings": [diamond(*g["outer"]), diamond(*g["black"])]}
    d = sdf.sd(band, x, y)
    nx, ny = sdf._normal(d)
    mask = d < -0.5
    art = rgb[y0:y1, x0:x1]
    coef, n = accept.solve_bevel([((-d / L_FRAME)[mask], nx[mask], ny[mask], art[mask])], STOPS)
    return coef, n


def fit(g, st, passes=3):
    rgb, m = C.sheet("buy_land.pcx")
    # the board's edges only (its flat inside cannot tell a number apart)
    x0, y0, x1, y1 = 30, 252, 770, 600
    art = rgb[y0:y1, x0:x1]
    jj, ii = np.mgrid[y0:y1, x0:x1]
    outer = {"t": "poly", "rings": [diamond(*g["outer"])]}
    d = sdf.sd(outer, ii + 0.5, jj + 0.5)
    mask = d < 6.0
    base = art.copy()
    base[d < 0.6] = [33, 49, 82]
    st.setdefault("drop", {"d": [2.0, 2.0], "g": 0.0, "soft": 3.0, "a": 0.35, "rgb": [10, 14, 30]})

    def loss():
        st["frame"] = measure_frame(g)[0]
        low = T.render_low(scene(g, st), base, (x0, y0), 2)
        return float(((low - art) ** 2)[mask].mean())
    dr = st["drop"]
    params = [[g["outer"], k, 0.2, None, None, 1.5] for k in range(4)] + [
        [dr["d"], 0, 0.3, -2, 6, 3], [dr["d"], 1, 0.3, -2, 6, 3], [dr, "g", 0.3, -4, 4, 3], [dr, "soft", 0.3, 0.5, 8, 4],
        [dr, "a", 0.05, 0, 1, 0.6]]
    return T.descend(loss, params, passes=passes, verbose=True, label="board")


def load():
    g = copy.deepcopy(GEOM)
    if os.path.exists(STYLE_FILE):
        with open(STYLE_FILE) as f:
            j = json.load(f)
        return j["geom"], j["style"]
    return g, {"frame": measure_frame(g)[0]}


if __name__ == "__main__":
    g = copy.deepcopy(GEOM)
    st = {"frame": measure_frame(g)[0]}
    fit(g, st)
    st["frame"] = measure_frame(g)[0]
    C.save_json({"geom": g, "style": st}, STYLE_FILE)
    print(json.dumps(g))
