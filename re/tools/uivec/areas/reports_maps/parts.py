"""reports_maps: the component recipes of the report screens (Buy Land, Routing map, Member Roster). Each returns scene layers
(numbers only). Shapes are written as read from the art's pixel grid; tunefit.py may nudge their numbers.

plate: a flat surface in a navy line. Its lighting (measured on the art's pixel rows, 2026-11-24): the line is the plate's own
edge; inside it a soft shade along the walls the light does not reach (an inner shadow toward the upper left on sunken cream
plates); outside it a soft drop shadow offset down-right that doubles as the thick lower line."""
import copy

import numpy as np

import sdf


def _smin(S, x, y):
    """rm.smin: a union whose concave seams are filleted (polynomial smooth minimum, fillet size k)."""
    d = sdf.sd(S["of"][0], x, y)
    k = S["k"]
    for q in S["of"][1:]:
        e = sdf.sd(q, x, y)
        h = np.clip(0.5 + 0.5 * (e - d) / k, 0, 1)
        d = e * (1 - h) + d * h - k * h * (1 - h)
    return d


sdf.EXTRA["rm.smin"] = _smin


def smin(k, *s):
    return {"t": "rm.smin", "of": list(s), "k": k}


def rr(x0, y0, x1, y1, r=0.0):
    return {"t": "rrect", "c": [(x0 + x1) / 2, (y0 + y1) / 2], "half": [(x1 - x0) / 2, (y1 - y0) / 2], "r": r, "rot": 0.0}


def grow(S, d):
    return {"t": "grow", "of": S, "d": d}


def move(S, dx, dy):
    return {"t": "move", "of": S, "d": [dx, dy]}


def U(*s):
    return {"t": "union", "of": list(s)}


def inter(*s):
    return {"t": "inter", "of": list(s)}


def sub(a, b):
    return {"t": "sub", "a": a, "b": b}


def rounded_inter(r, *s):
    """The intersection with its convex corners rounded to radius r."""
    return grow(inter(*[grow(q, -r) for q in s]), r)


def plate(S, st):
    """S: the shape whose edge is the CENTRE of the navy line. st: {"ow", "fill", "line", "inner": {...} | None,
    "drop": {...} | None}."""
    L = []
    d = st.get("drop")
    if d:
        L.append({"shape": move(grow(S, st["ow"] / 2 + d["g"]), *d["d"]), "fill": d["rgb"], "alpha": d["a"], "soft": d["soft"]})
    L.append({"shape": grow(S, st["ow"] / 2), "fill": st["line"]})
    fx = []
    i = st.get("inner")
    if i:
        fx.append({"t": "inner_shadow", "w": i["w"], "pow": i["pow"], "dir": i["dir"], "rgb": i["rgb"], "k": i["k"]})
    L.append({"shape": grow(S, -st["ow"] / 2), "fill": st["fill"], "fx": fx})
    return L


def style_params(st, which=("ow", "inner", "drop")):
    """The tunable numbers of a plate style as tunefit parameters."""
    P = []
    if "ow" in which:
        P.append([st, "ow", 0.1, 0.8, 3.0, 0.6])
    i = st.get("inner")
    if i and "inner" in which:
        P += [[i, "w", 0.25, 0.5, 8, 2], [i, "pow", 0.1, 0.3, 4, 1.5], [i, "k", 0.03, 0, 1, 0.5], [i["dir"], 0, 0.1, -1, 1, 1],
              [i["dir"], 1, 0.1, -1, 1, 1]] + [[i["rgb"], c, 6, 0, 255, 80] for c in range(3)]
    d = st.get("drop")
    if d and "drop" in which:
        P += [[d, "g", 0.2, -3, 4, 2], [d, "soft", 0.2, 0.3, 6, 3], [d, "a", 0.03, 0, 1, 0.6], [d["d"], 0, 0.2, -4, 4, 2],
              [d["d"], 1, 0.2, -4, 4, 2]] + [[d["rgb"], c, 6, 0, 255, 80] for c in range(3)]
    return P


def corner(cx, cy, r, a0, a1, n=6):
    """Points along a circle from angle a0 to a1 (degrees, 0 = right, 90 = down)."""
    return [[cx + r * np.cos(np.deg2rad(a0 + (a1 - a0) * k / n)), cy + r * np.sin(np.deg2rad(a0 + (a1 - a0) * k / n))]
            for k in range(n + 1)]


def title_plate(g):
    """The arched title plate as the line-centre outline. The top edge is one symmetric curve read from the art (the line's
    centre, averaged over both halves, at the distances "prof" = [[distance from the middle, y], ...]); it runs on into
    the rounded top corners and straight walls, a flat bottom with corners "rb". Lightly corner-cut ("smooth").
    g: {"box": [x0, x1, y1], "prof": [...], "rb": r}."""
    x0, x1, y1 = g["box"]
    c = (x0 + x1) / 2
    half = (x1 - x0) / 2
    prof = [[d, y - g.get("lift", 0.0)] for d, y in g["prof"]]
    rb = g["rb"]
    right = [[c + d, y] for d, y in prof if d < half] + [[x1, prof[-1][1] + g.get("drop", 2.5)]]
    ring = [[x1, y1 - rb]] + corner(x1 - rb, y1 - rb, rb, 0, 90, 4)[1:] + corner(x0 + rb, y1 - rb, rb, 90, 180, 4)
    left = [[2 * c - x, y] for x, y in right[::-1]]
    ring += left + right[1:]
    return {"t": "poly", "rings": [ring], "smooth": g.get("smooth", 2)}
