"""Buy Land lot capsules: nine flat rounded boxes (cream / tan alternating) in a 2 px navy line, an inner shadow along the
top and left walls, a drop shadow down-right; each one's left end tucks under its number ball.

Read 2026-11-24 from the pixel rows of buy_land.pcx (navy rows/columns: the line pairs 52-53 / 116-117, 120-121 / 184-185,
188-189 / 252-253; right walls 257-258, 515-516, 781-782; the left end's corner arc visible above each ball at x 37-44):
line centres y 53 / 117, 121 / 185, 189 / 253; x right 258 / 516 / 782; left wall ~36.5 / 294.5 / 560.5 (hidden by the
ball but for its corner); corner radius about 7.
"""
import copy
import json
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import common as C      # noqa: E402
import parts as P       # noqa: E402
import tunefit as T     # noqa: E402

CREAM, TAN = [255, 247, 231], [222, 198, 156]
NAVY = [33, 41, 82]
GEOM = {"cols": [[36.5, 258.0], [294.5, 516.0], [560.5, 782.0]], "rows": [[53.0, 117.0], [121.0, 185.0], [189.0, 253.0]],
        "r": 7.0}
FILLS = [CREAM, TAN, CREAM, TAN, CREAM, TAN, CREAM, TAN, CREAM]     # lot 1..9, column by column
BALLS = [(41.6, 86.05), (41.6, 154.05), (41.6, 223.1), (299.6, 86.05), (299.6, 154.05), (299.6, 223.0), (565.1, 86.1),
         (565.6, 153.9), (565.6, 223.1)]


def style(fill):
    return {"ow": 2.0, "fill": list(fill), "line": list(NAVY),
            "inner": {"w": 3.0, "pow": 1.0, "dir": [-0.4, -0.9], "rgb": [120, 116, 130], "k": 0.5},
            "drop": {"g": 0.0, "d": [1.0, 1.0], "soft": 1.5, "a": 0.45, "rgb": [40, 44, 90]}}


def shape(g, n):
    c, r = divmod(n - 1, 3)
    (x0, x1), (y0, y1) = g["cols"][c], g["rows"][r]
    return P.rr(x0, y0, x1, y1, g["r"])


def items(g, st):
    out = []
    for n in range(1, 10):
        S = shape(g, n)
        s = st["cream"] if FILLS[n - 1] == CREAM else st["tan"]
        c, r = divmod(n - 1, 3)
        (x0, x1), (y0, y1) = g["cols"][c], g["rows"][r]
        out.append({"name": f"capsule {n}", "box": [x0 - 6, y0 - 6, x1 + 7, y1 + 7], "layers": P.plate(S, s)})
    return out


def fit(g, st, passes=4):
    rgb, m = C.sheet("buy_land.pcx")
    x, y, w, h = 28, 44, 762, 216
    art = rgb[y:y + h, x:x + w]
    jj, ii = np.mgrid[y:y + h, x:x + w]
    mask = np.ones((h, w), bool)
    for bx, by in BALLS:       # the balls and the shadow they cast are not the capsules'
        mask &= np.hypot(ii + 0.5 - bx, jj + 0.5 - by) > 33
    bg = np.array([148.0, 148.0, 198.0])
    base = np.empty_like(art)
    base[:] = bg

    def loss():
        low = T.render_low(items(g, st), base, (x, y), 3)
        return float(((low - art) ** 2)[mask].mean())
    params = []
    for k in ("cream", "tan"):
        params += P.style_params(st[k])
    params += [[g, "r", 0.25, 2, 12, 2]] + [[g["cols"][c], 1, 0.1, None, None, 0.6] for c in range(3)] + \
              [[g["rows"][r], i, 0.1, None, None, 0.6] for r in range(3) for i in range(2)]
    f0, f1 = T.descend(loss, params, passes=passes, verbose=True, label="capsules")
    return f0, f1


if __name__ == "__main__":
    g = copy.deepcopy(GEOM)
    st = {"cream": style(CREAM), "tan": style(TAN)}
    fit(g, st)
    C.save_json({"geom": g, "style": st}, os.path.join(C.OUT, "read", "caps_fit.json"))
    print(json.dumps(st))
    print(json.dumps(g))
