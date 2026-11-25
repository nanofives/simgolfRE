"""P41 fix round (2026-11-24), route-top-course: "consistent shade".

What was wrong: the banner's and the list's inner shade was a bevel paint keyed on the NEAREST edge and its direction, so
at each corner the shade switched walls along a diagonal (a mitre), and the right wall got a light inner band the art does
not have.

What the art shows (pixel rows of route screens_course.pcx): the shade inside a sunk recess is ONE inner shadow, the floor
multiplied toward black (ratio 0.63 at the wall on tan, cream and stripe alike: banner 139/220, list 161/255, stripe
153/243), under the top and left walls only, the same profile on the banner and on the list (0.63, 0.68, 0.75, 0.83, 0.90,
0.95, 1.0 over 7 px); the right wall darkens about 1 px; the corners are darker than either wall but not the product
(43,52: 0.61; 46,55: 0.73 where each wall gives 0.83): the shadow of a soft-edged rectangle.

So the inner shade is drawn as what it is: a black layer at alpha A over the recess, with a SOFT rounded rectangle cut out
of it (the lit floor), offset down-right; clipped to the recess. Every corner and wall is the same shadow, no seams. One
style ("ishadow") for the banner and the list. Fitted only in its 6 numbers (offsets, softness, corner radius, alpha) on
windows of the art; geometry and every other paint as read before (route_tops.py).
"""
import json
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import common as C      # noqa: E402
import lit              # noqa: E402
import parts as P       # noqa: E402
import route_tops as R  # noqa: E402

OUT2 = os.path.join(C.ROOT, "log", "roadmap", "P41", "work2", "reports_maps")
ISH_FILE = os.path.join(OUT2, "read", "ishadow.json")
FLOOR = {"banner": [220, 197, 155], "list_cream": R.CREAM, "list_stripe": R.STRIPE, "list_tan": R.TAN, "table": R.TAN}
ISH0 = {"l": 2.4, "t": 2.4, "r": 1.2, "b": 3.0, "soft": 3.2, "rad": 3.0, "a": 0.4}


def rect_of(S):
    cx, cy = S["c"]
    hx, hy = S["half"]
    return cx - hx, cy - hy, cx + hx, cy + hy


def shadow_layer(S, ish):
    """The inner shadow of recess S (a sharp rrect): black at alpha a everywhere in S except a soft rounded rectangle (the
    lit floor) whose edges sit l / t px inside the left / top walls and r / b px inside the right / bottom ones."""
    x0, y0, x1, y1 = rect_of(S)
    Q = P.rr(x0 + ish["l"], y0 + ish["t"], x1 - ish["r"], y1 - ish["b"], ish["rad"])
    big = P.rr(x0 - 50, y0 - 50, x1 + 50, y1 + 50)
    return {"shape": P.sub(big, Q), "fill": [0, 0, 0], "alpha": ish["a"], "soft": ish["soft"], "clip": S}


def item_split(name, kind, S, styles, ish):
    """(floor item, shade item): the recess's outer lip (as solved before) and its flat floor; then its inner shadow, which
    goes after anything lying on the floor (stripes), as the art's shade darkens the stripes by the same ratio."""
    st = styles[kind]
    L = lit.layers(S, st)[:-1]      # everything but the old inner bevel
    if kind.startswith("list") and st.get("out"):
        L[0]["clip"] = {"t": "half", "p": [0, S["c"][1] - S["half"][1] + 0.9], "n": [0, -1]}
    L.append({"shape": S, "fill": FLOOR[kind]})
    return ({"name": name, "box": [0, 0, 800, 600], "layers": L},
            {"name": name + " shadow", "box": [0, 0, 800, 600], "layers": [shadow_layer(S, ish)]})


BOX_SH0 = {"dx": 1.0, "dy": 1.0, "g": 0.0, "soft": 1.4, "a": 0.3}
BOX_FILE = os.path.join(OUT2, "read", "box_shadow.json")


def load_box():
    if os.path.exists(BOX_FILE):
        with open(BOX_FILE) as f:
            return json.load(f)
    return dict(BOX_SH0)


def box_item(name, S, styles, bsh):
    """The raised cream box hanging from the slab line: a flat cream fill, the 1.7 px navy line, and the line's own shadow
    cast down-right (the art: 0.77, 0.86, 0.95 of the cream on the right of / below EVERY wall, so inside the box along its
    left wall and outside along its right wall and foot: one shadow, the same on all sides)."""
    st = styles["box"]
    h = st["ow"] / 2
    line = {"t": "outline", "of": S, "w": st["ow"]}
    sh = {"t": "move", "of": {"t": "outline", "of": S, "w": st["ow"] + 2 * bsh["g"]}, "d": [bsh["dx"], bsh["dy"]]}
    return {"name": name, "box": [0, 0, 800, 600], "layers": [
        {"shape": P.grow(S, -h + 0.01), "fill": R.CREAM},
        {"shape": sh, "fill": [0, 0, 0], "alpha": bsh["a"], "soft": bsh["soft"]},
        {"shape": line, "fill": st["line"]}]}


def scene(mode, styles, ish, meters=None, bsh=None):
    bsh = bsh or load_box()
    out, later = [], []
    for n, k, S in R.components(mode):
        if isinstance(k, tuple) and k[0] == "meter":
            if meters is not None:
                import route_meters
                out += route_meters.scene(mode, meters, styles)
            continue
        if not isinstance(k, tuple) and k in FLOOR:
            a, b = item_split(n, k, S, styles, ish)
            out.append(a)
            later.append(b)
            continue
        if later and not (isinstance(k, tuple) and k[0] == "stripe"):
            out += later        # the shadows go on once the floor's stripes are down
            later = []
        out.append(box_item(n, S, styles, bsh) if k == "box" else R.item(n, k, S, styles))
    return out + later


def load_ish():
    if os.path.exists(ISH_FILE):
        with open(ISH_FILE) as f:
            return json.load(f)
    return dict(ISH0)


WINS = [(36, 46, 30, 30), (735, 46, 25, 30), (14, 90, 30, 34), (760, 90, 25, 34), (380, 52, 20, 30), (100, 92, 30, 40)]


def tune(styles):
    import tunefit as T
    rgb, m = C.sheet("route screens_course.pcx")
    base = np.empty_like(rgb)
    base[:] = R.LAV
    ish = load_ish()

    def loss():
        sc = scene("course", styles, ish)
        t = 0.0
        for x, y, w, h in WINS:
            low = T.render_low(sc, base[y:y + h, x:x + w], (x, y), 3)
            t += float(((low - rgb[y:y + h, x:x + w]) ** 2).mean())
        return t
    params = [[ish, "l", 0.3, -2, 6], [ish, "t", 0.3, -2, 6], [ish, "r", 0.3, -3, 6], [ish, "soft", 0.3, 0.5, 6],
              [ish, "rad", 0.5, 0, 8], [ish, "a", 0.03, 0.1, 1.0]]
    T.descend(loss, params, passes=6, verbose=True, label="ishadow")
    os.makedirs(os.path.dirname(ISH_FILE), exist_ok=True)
    with open(ISH_FILE, "w") as f:
        json.dump(ish, f)
    return ish


BOX_WINS = [(282, 120, 14, 20), (503, 120, 14, 20), (370, 182, 20, 14), (282, 180, 16, 14), (500, 180, 16, 14)]


def tune_box(styles):
    import tunefit as T
    rgb, m = C.sheet("route screens_course.pcx")
    base = np.empty_like(rgb)
    base[:] = R.LAV
    ish = load_ish()
    bsh = load_box()

    def loss():
        bsh["dy"] = bsh["dx"]       # one light: the shadow falls the same distance right and down
        sc = scene("course", styles, ish, bsh=bsh)
        t = 0.0
        for x, y, w, h in BOX_WINS:
            low = T.render_low(sc, base[y:y + h, x:x + w], (x, y), 4)
            t += float(((low - rgb[y:y + h, x:x + w]) ** 2).mean())
        return t
    bsh.update(BOX_SH0)
    params = [[bsh, "dx", 0.2, -1, 4], [bsh, "g", 0.2, -1, 3], [bsh, "soft", 0.2, 0.3, 4],
              [bsh, "a", 0.03, 0.05, 1.0]]
    T.descend(loss, params, passes=8, verbose=True, label="box shadow")
    with open(BOX_FILE, "w") as f:
        json.dump(bsh, f)
    return bsh


if __name__ == "__main__":
    if "box" in sys.argv[1:]:
        print(tune_box(R.load_styles()))
    else:
        print(tune(R.load_styles()))
