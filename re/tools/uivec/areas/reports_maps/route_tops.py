"""Routing map top bands (course / employ / aura / value), read 2026-11-24 from the pixel rows of route screens_<mode>.pcx.

What the band IS (course, aura, value):
  lavender ground; the arched cream title plate (the same plate as Buy Land: the top-edge profiles agree within 0.02 px);
  a tan BANNER sunk into the lavender (no line: a shade falls outside its top and left edges, a light lip outside its right
  edge, an inner shade inside its top and left walls) from x 42.5 to 756.5, y 51.5 down under the slab;
  the lower SLAB: a lavender plate in a navy line (x 5.2..795.8, top y 91.8, corners r 8) whose top line is also the wire
  the column pills sit on; inside it the LIST, sunk the same way as the banner (x 18.5..780.5, its top on the slab line);
  the list's floor and stripes; lilac PILLS on the slab line (a navy line, an inner shade under the top / left walls, a thin
  drop shadow); course: a raised cream BOX hanging from the line (x 288.9..510.9, bottom 188.9); aura: a flat cream box
  (x 42..757, y 95..170); value: navy bullets on the stripes. aura / value: the meter (route_meters.py).
employ: no banner and no slab line at the top: two sunk TABLES (x 32.5..392.5 and 406.5..766.5, y 67.5..245.5) of tan
  cells and a cream column split by 2 px navy rules; the slab line and a tan list floor start at y 246.8.
Every paint is solved by least squares on the art (lit.py); geometry is read, not fitted.
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
import sdf              # noqa: E402

LAV = [148, 150, 198]
CREAM, STRIPE, TAN, LILAC = [255, 247, 231], [243, 230, 204], [220, 197, 155], [191, 193, 251]
NAVY = [32, 42, 76]
PLATE_G = {"box": [190.6, 610.7, 42.0], "rb": 2.0, "drop": 1.5, "lift": 0.0,
           "prof": [[0, 7.62], [25, 7.66], [50, 7.82], [75, 8.08], [100, 8.87], [125, 9.79], [150, 10.91], [170, 12.08],
                    [185, 13.5], [195, 14.84], [200, 15.65], [204, 16.4], [207.5, 17.0], [209.6, 17.8]]}
STYLE_FILE = os.path.join(C.OUT, "read", "route_tops_style.json")

COURSE_PILLS = [[38, 86, 97, 98], [117, 86, 150, 98], [166, 86, 199, 98], [215, 86, 266, 98], [329, 86, 473, 98],
                [536, 86, 595, 98], [615, 86, 648, 98], [664, 86, 697, 98], [713, 86, 764, 98]]
AURA_PILLS = [[257, 86, 544, 98]]
VALUE_PILLS = [[37, 86, 324, 98], [477, 86, 764, 98]]


def rr(x0, y0, x1, y1, r=0.0):
    return P.rr(x0, y0, x1, y1, r)


def pill(f):
    """A pill from its flat fill's extent (boxes.py): the navy line sits 5.1 px left of / 4.0 px above the fill (the inner
    shade between) and 2.6 px right of / 1.9 px below it."""
    x0, y0, x1, y1 = f[0] - 5.1, f[1] - 4.0, f[2] + 2.6, f[3] + 1.9
    return rr(x0, y0, x1, y1, (y1 - y0) / 2)


def components(mode):
    """[(name, kind, shape)] bottom to top. kind picks the shared paint; flat kinds carry their colour."""
    out = []
    if mode != "employ":
        out.append(("banner", "banner", rr(42.5, 51.5, 756.5, 130.0)))
        out.append(("slab", "slab", rr(5.2, 91.8, 795.8, 330.0, 8.0)))
        out.append(("list", "list_" + ("cream" if mode == "course" else "stripe"), rr(18.5, 91.6, 780.5, 330.0)))
    else:
        for x0, x1, xc in ((32.5, 392.5, 331.5), (406.5, 766.5, 705.5)):
            out.append(("table", "table", rr(x0, 67.5, x1, 245.6)))
            out.append(("cream column", ("flat", CREAM), rr(xc + 1.5, 67.5, x1, 245.6)))
            for yl in (114.5, 159.5, 204.5):
                out.append(("rule", ("flat", [36, 47, 84]), rr(x0, yl - 1.0, x1, yl + 1.0)))
            out.append(("rule", ("flat", [36, 47, 84]), rr(xc - 1.0, 67.5, xc + 1.0, 245.6)))
        out.append(("slab", "slab", rr(5.2, 246.8, 795.8, 480.0, 8.0)))
        out.append(("list", "list_tan", rr(18.5, 246.6, 780.5, 480.0)))
    if mode == "course":
        for a, b in ((101, 118), (135, 152), (170, 187), (203, 220), (236, 253)):
            out.append(("stripe", ("stripe", STRIPE), rr(18.5, a, 780.5, b)))
        out.append(("box", "box", P.inter(rr(288.9, 60.0, 510.9, 188.9, 6.0), rr(288.9, 91.8, 510.9, 260.0))))
        pills = COURSE_PILLS
    elif mode == "aura":
        out.append(("cream box", ("flat", CREAM), rr(42.0, 95.0, 757.0, 170.0)))
        pills = AURA_PILLS
    elif mode == "value":
        for a, b in ((118, 135), (152, 170)):
            out.append(("stripe", ("flat", CREAM), rr(18.5, a, 780.5, b)))
        for x in (43.9, 483.9):
            for y in (109.4, 126.4, 143.4, 160.4):
                out.append(("bullet", ("flat", [36, 47, 84]), {"t": "circle", "c": [x, y], "r": 2.9}))
        pills = VALUE_PILLS
    else:
        pills = []
    for f in pills:
        out.append(("pill", "pill", pill(f)))
    if mode in ("aura", "value"):        # drawn by route_meters.py; here only its place (left out of the paints' pixels)
        out.append(("meter", ("meter", mode), rr(106.0, 183.0, 694.0, 233.0, 8.0)))
    out.append(("title plate", "plate", P.title_plate(PLATE_G)))
    return out


KINDS = {"banner": dict(ow=0.0, L=7.0, out_w=6.0), "slab": dict(ow=1.7, L=6.0, out_w=2.0),
         "list_cream": dict(ow=0.0, L=8.0, out_w=6.0), "list_stripe": dict(ow=0.0, L=8.0, out_w=6.0),
         "list_tan": dict(ow=0.0, L=8.0, out_w=6.0), "table": dict(ow=0.0, L=8.0, out_w=6.0),
         "box": dict(ow=1.7, L=4.0, out_w=0.0), "pill": dict(ow=1.7, L=5.0, out_w=0.0), "plate": dict(ow=1.8, L=4.0, out_w=3.0)}


def item(name, kind, S, styles):
    if isinstance(kind, tuple):
        tag, col = kind
        if tag == "stripe":         # a stripe over the list's shaded floor: half the darker colour laid over it
            c = [2 * a - b for a, b in zip(col, CREAM)]
            return {"name": name, "box": [0, 0, 800, 600], "layers": [{"shape": S, "fill": c, "alpha": 0.5}]}
        return {"name": name, "box": [0, 0, 800, 600], "layers": [{"shape": S, "fill": col}]}
    if kind == "plate":       # the capsules' sunken cream plate style (see plates.py), this sheet's navy
        st = json.load(open(os.path.join(C.OUT, "read", "caps_fit.json")))["style"]["cream"]
        return {"name": name, "box": [0, 0, 800, 600], "layers": P.plate(S, dict(st, line=[47, 55, 107]))}
    L = lit.layers(S, styles[kind])
    if kind.startswith("list") and styles[kind].get("out"):
        # the list's top IS the slab line: its lip shows only along its sides and foot
        L[0]["clip"] = {"t": "half", "p": [0, S["c"][1] - S["half"][1] + 0.9], "n": [0, -1]}
    return {"name": name, "box": [0, 0, 800, 600], "layers": L}


def scene(mode, styles, meters=None):
    out = []
    for n, k, S in components(mode):
        if isinstance(k, tuple) and k[0] == "meter":
            if meters is not None:
                import route_meters
                out += route_meters.scene(mode, meters, styles)
            continue
        out.append(item(n, k, S, styles))
    return out


def solve_all(modes=("course", "employ", "aura", "value")):
    """Every paint kind over all its instances on the four sheets. A pixel counts for a component only when no component
    drawn after it covers it (grown by 2 px); stripes are left out of the list floor."""
    insts = {}
    for mode in modes:
        rgb, m = C.sheet(f"route screens_{mode}.pcx")
        H = rgb.shape[0]
        comps = components(mode)
        jj, ii = np.mgrid[0:H, 0:800]
        x, y = ii + 0.5, jj + 0.5
        cover = np.zeros((H, 800), bool)
        for name, kind, S in comps[::-1]:
            if not isinstance(kind, tuple) and kind in KINDS:
                insts.setdefault(kind, []).append((S, rgb, (0, 0, 800, H), ~cover))
            reach = 2.0 + (KINDS[kind]["out_w"] + KINDS[kind]["ow"] / 2 if (not isinstance(kind, tuple) and kind in KINDS) else 0.5)
            cover |= sdf.sd(S, x, y) < reach
    styles = {}
    for kind, lst in insts.items():
        st = lit.new_style(**KINDS[kind])
        if kind in ("box", "pill"):
            st["drop"] = {"d": [0.8, 1.2], "g": 0.0, "soft": 1.6, "a": 0.35, "rgb": [40, 40, 70]}
        lit.solve(lst, st)
        styles[kind] = st
        print(f"[solve] {kind}: {len(lst)} instances, line {st['line']}", flush=True)
    with open(STYLE_FILE, "w") as f:
        json.dump(styles, f)
    return styles


def tune_drops(styles):
    """The pills' and the box's cast shadow (offset, growth, softness, strength): a small descent on windows below them."""
    import tunefit as T
    rgb, m = C.sheet("route screens_course.pcx")
    sc = None
    wins = [(90, 90, 30, 16), (460, 92, 30, 14), (480, 180, 40, 16), (280, 180, 20, 16)]
    base = np.empty_like(rgb)
    base[:] = LAV

    def loss():
        sc = scene("course", styles)
        t = 0.0
        for x, y, w, h in wins:
            low = T.render_low(sc, base[y:y + h, x:x + w], (x, y), 3)
            t += float(((low - rgb[y:y + h, x:x + w]) ** 2).mean())
        return t
    params = []
    for k in ("pill", "box"):
        d = styles[k]["drop"]
        params += [[d["d"], 0, 0.2, -2, 4, 2], [d["d"], 1, 0.2, -2, 4, 2], [d, "g", 0.2, -2, 3, 2], [d, "soft", 0.2, 0.3, 5, 3],
                   [d, "a", 0.04, 0, 1, 0.5]]
    T.descend(loss, params, passes=3, verbose=True, label="drops")
    with open(STYLE_FILE, "w") as f:
        json.dump(styles, f)


def load_styles():
    with open(STYLE_FILE) as f:
        return json.load(f)


if __name__ == "__main__":
    old = load_styles() if os.path.exists(STYLE_FILE) else {}
    new = solve_all()
    for k in ("pill", "box"):        # keep the fitted cast shadows
        if k in old and "drop" in old[k]:
            new[k]["drop"] = old[k]["drop"]
    with open(STYLE_FILE, "w") as f:
        json.dump(new, f)
