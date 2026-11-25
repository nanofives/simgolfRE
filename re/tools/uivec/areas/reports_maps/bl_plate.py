"""Buy Land / Routing map / Member Roster title plate: read, tuned, written out."""
import copy
import json
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import common as C      # noqa: E402
import parts as P       # noqa: E402
import tunefit as T     # noqa: E402

# Read off the pixel rows (rows.py, dark_centre) of buy_land.pcx: line centre x 190.6 / 610.7, bottom 42.0 (the darkest pixel
# row; the weighted centre is pulled down by the drop shadow), top an ellipse arc (fit on 390 columns).
READ = {
    "buyland": {"sheet": "buy_land.pcx", "crop": (182, 0, 438, 52),
                "g": {"box": [190.6, 610.7, 42.0], "rb": 2.0, "drop": 1.5, "lift": 0.0, "prof": [[0, 7.61], [25, 7.64], [50, 7.81], [75, 8.09], [100, 8.86], [125, 9.79], [150, 10.9], [170, 12.09], [185, 13.44], [195, 14.83], [200, 15.65], [204, 16.4], [207.5, 17.0], [209.6, 17.8]]}},
}


def base_style(bg, line, fill):
    return {"ow": 2.0, "fill": list(fill), "line": list(line),
            "inner": {"w": 3.0, "pow": 1.0, "dir": [-0.4, -0.9], "rgb": [120, 116, 130], "k": 0.5},
            "drop": {"g": 0.0, "d": [1.0, 1.0], "soft": 1.5, "a": 0.45, "rgb": [40, 44, 90]}}


def build(key, fitting=True):
    R = READ[key]
    rgb, m = C.sheet(R["sheet"])
    x, y, w, h = R["crop"]
    art = rgb[y:y + h, x:x + w]
    bg = np.median(art[:4].reshape(-1, 3), axis=0)
    st = base_style(bg, (33, 41, 82), (255, 247, 231))
    g = copy.deepcopy(R["g"])
    base = np.empty_like(art)
    base[:] = bg

    def scene():
        S = P.title_plate(g)
        return [{"box": [x, y, x + w, y + h], "layers": P.plate(S, st)}]

    if fitting:
        holder = {"sc": None}

        def loss():
            low = T.render_low(scene(), base, (x, y), 3)
            return float(((low - art) ** 2).mean())
        params = P.style_params(st) + [[g["box"], 0, 0.1, None, None, 0.6], [g["box"], 1, 0.1, None, None, 0.6],
                                       [g["box"], 2, 0.1, None, None, 0.8], [g, "lift", 0.1, None, None, 0.8],
                                       [g, "rb", 0.25, 0, 6, 2]]
        f0, f1 = T.descend(loss, params, passes=4, verbose=True, label=key)
    return scene(), art, base, (x, y), st, g


if __name__ == "__main__":
    sc, art, base, org, st, g = build("buyland")
    print(json.dumps(st), json.dumps(g))
