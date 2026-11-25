"""Routing map meters (aura, value), read 2026-11-24 from route screens_aura / _value.pcx:
a rounded BAR in a navy line (line centre x 193.5..607.4, y 194.0..229.6, corners r 6) filled with a left-to-right gradient
(aura: orange (255,131,0) -> yellow (255,248,0) at the middle -> green (129,248,0); value: black -> green (94,246,62)),
an inner shade under its top and left walls and a soft drop shadow below; two lilac END PILLS (round outer end, the inner
end tucked under the bar) and the lilac PILL over the bar's top (the same pill as the column pills: route_tops' style).
The gradient's colours are read off the bar's middle row (y 215) at 9 places; the shade and shadow are fitted.
"""
import copy
import json
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import common as C      # noqa: E402
import lit              # noqa: E402
import parts as P       # noqa: E402
import route_tops as R  # noqa: E402
import tunefit as T     # noqa: E402

BAR = [193.5, 194.0, 607.4, 229.6, 6.0]
END_PILLS = [[115, 207, 204, 219], [598, 207, 686, 219]]   # the inner ends run on under the bar (flat there)
TOP_PILL = [257, 189, 541, 201]
STYLE_FILE = os.path.join(C.OUT, "read", "meters_style.json")


def gradient(mode, n=9):
    rgb, m = C.sheet(f"route screens_{mode}.pcx")
    x0, x1 = 197.0, 604.0
    stops = []
    for k in range(n):
        x = x0 + (x1 - x0) * k / (n - 1)
        xi = int(x)
        c = rgb[213:218, xi - 1:xi + 2].reshape(-1, 3).mean(0)
        stops.append([round((x - x0) / (x1 - x0), 4), [round(float(v), 1) for v in c]])
    return {"linear": {"a": [x0, 0.0], "b": [x1, 0.0], "stops": stops}}


def bar_layers(st, mode):
    x0, y0, x1, y1, r = BAR
    S = P.rr(x0, y0, x1, y1, r)
    h = st["ow"] / 2
    d = st["drop"]
    i = st["inner"]
    return [{"shape": {"t": "move", "of": {"t": "grow", "of": S, "d": h + d["g"]}, "d": d["d"]}, "fill": d["rgb"],
             "alpha": d["a"], "soft": d["soft"]},
            {"shape": {"t": "grow", "of": S, "d": h}, "fill": st["line"]},
            {"shape": {"t": "grow", "of": S, "d": -h}, "fill": st["grad"][mode],
             "fx": [{"t": "inner_shadow", "w": i["w"], "pow": i["pow"], "dir": i["dir"], "rgb": i["rgb"], "k": i["k"]}]}]


def scene(mode, st, pstyles):
    items = [{"name": "end pill", "box": [100, 180, 700, 240], "layers": lit.layers(R.pill(f), pstyles["pill"])} for f in END_PILLS]
    items.append({"name": "bar", "box": [180, 185, 620, 240], "layers": bar_layers(st, mode)})
    items.append({"name": "pill", "box": [240, 178, 560, 210], "layers": lit.layers(R.pill(TOP_PILL), pstyles["pill"])})
    return items


def fit():
    pst = R.load_styles()
    st = {"ow": 1.8, "line": [32, 42, 76], "grad": {m: gradient(m) for m in ("aura", "value")},
          "inner": {"w": 3.0, "pow": 1.0, "dir": [-0.3, -0.95], "rgb": [60, 40, 20], "k": 0.3},
          "drop": {"d": [0.5, 1.2], "g": 0.0, "soft": 1.5, "a": 0.35, "rgb": [60, 50, 40]}}
    wins = [(186, 188, 30, 48), (590, 188, 24, 48), (300, 196, 40, 40)]
    arts = {m: C.sheet(f"route screens_{m}.pcx")[0] for m in ("aura", "value")}
    TAN = np.array([243.0, 230.0, 204.0])

    def loss():
        t = 0.0
        for mo in ("aura", "value"):
            sc = scene(mo, st, pst)
            for x, y, w, h in wins:
                base = np.empty((h, w, 3)); base[:] = TAN
                low = T.render_low(sc, base, (x, y), 3)
                t += float(((low - arts[mo][y:y + h, x:x + w]) ** 2).mean())
        return t
    i, d = st["inner"], st["drop"]
    params = [[st, "ow", 0.1, 1, 3, 0.6], [i, "w", 0.3, 0.5, 8, 3], [i, "pow", 0.1, 0.3, 4, 1.5], [i, "k", 0.04, 0, 1, 0.5],
              [i["dir"], 0, 0.1, -1, 1, 0.8], [i["dir"], 1, 0.1, -1, 1, 0.8]] + [[i["rgb"], c, 8, 0, 255, 80] for c in range(3)] + \
             [[d["d"], 0, 0.2, -2, 4, 2], [d["d"], 1, 0.2, -2, 4, 2], [d, "g", 0.2, -2, 3, 2], [d, "soft", 0.2, 0.3, 5, 3],
              [d, "a", 0.04, 0, 1, 0.5]] + [[d["rgb"], c, 8, 0, 255, 80] for c in range(3)] + \
             [[BAR, k, 0.1, None, None, 0.6] for k in range(5)]
    T.descend(loss, params, passes=3, verbose=True, label="meters")
    st["bar"] = BAR
    with open(STYLE_FILE, "w") as f:
        json.dump(st, f)
    return st


def load():
    with open(STYLE_FILE) as f:
        st = json.load(f)
    BAR[:] = st["bar"]
    return st


if __name__ == "__main__":
    fit()
