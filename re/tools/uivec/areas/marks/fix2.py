"""P41 fix round 2 (2026-11-24), area `marks`: shared helpers for the redone elements.

New shape: marks.vline, a smooth line through points (Catmull-Rom spline, so it passes through every point and has no
corners) whose width changes from point to point, with round ends and joins: one continuous stroke for curves that turn
back (a dollar S, a mouth outline, a club shaft), which sdf's butt-capped `stroke` cuts.
"""
import numpy as np

import common as C
import sdf


def catmull(P, n=8, closed=False):
    """Points along the Catmull-Rom spline through P (n per span)."""
    P = [np.asarray(p, float) for p in P]
    ext = ([P[-1]] + P + [P[0], P[1]]) if closed else ([2 * P[0] - P[1]] + P + [2 * P[-1] - P[-2]])
    spans = len(P) if closed else len(P) - 1
    out = []
    for k in range(spans):
        p0, p1, p2, p3 = ext[k], ext[k + 1], ext[k + 2], ext[k + 3]
        for i in range(n):
            t = i / n
            out.append(0.5 * (2 * p1 + (-p0 + p2) * t + (2 * p0 - 5 * p1 + 4 * p2 - p3) * t * t
                              + (-p0 + 3 * p1 - 3 * p2 + p3) * t ** 3))
    out.append(P[0] if closed else P[-1])
    return out


def _vline(S, x, y):
    pts, ws = S["pts"], S["ws"]
    closed = S.get("closed", False)
    n = S.get("n", 8)
    Q = catmull(pts, n, closed)
    wsx = list(ws) + ([ws[0]] if closed else [])
    W = []
    for k in range(len(wsx) - 1):
        for i in range(n):
            W.append(wsx[k] * (1 - i / n) + wsx[k + 1] * i / n)
    W.append(wsx[-1])
    d = np.full(x.shape, 1e18)
    for k in range(len(Q) - 1):
        a, b = Q[k], Q[k + 1]
        ex, ey = b[0] - a[0], b[1] - a[1]
        ll = max(ex * ex + ey * ey, 1e-12)
        wx, wy = x - a[0], y - a[1]
        u = np.clip((wx * ex + wy * ey) / ll, 0, 1)
        r = (W[k] * (1 - u) + W[k + 1] * u) / 2
        d = np.minimum(d, np.hypot(wx - ex * u, wy - ey * u) - r)
    return d


sdf.EXTRA["marks.vline"] = _vline


def vline(pts, ws, closed=False, n=8):
    if not isinstance(ws, (list, tuple)):
        ws = [ws] * len(pts)
    return {"t": "marks.vline", "pts": [[float(a), float(b)] for a, b in pts], "ws": [float(w) for w in ws],
            "closed": closed, "n": n}


def cub(p0, p1, p2, p3, n=10):
    """Points of a cubic Bezier from p0 to p3 (p0 included, p3 included)."""
    P = [np.asarray(p, float) for p in (p0, p1, p2, p3)]
    out = []
    for i in range(n + 1):
        t = i / n
        q = (1 - t) ** 3 * P[0] + 3 * t * (1 - t) ** 2 * P[1] + 3 * t * t * (1 - t) * P[2] + t ** 3 * P[3]
        out.append([round(float(q[0]), 3), round(float(q[1]), 3)])
    return out


def poly(*rings, smooth=0):
    S = {"t": "poly", "rings": [list(r) for r in rings]}
    if smooth:
        S["smooth"] = smooth
    return S


def lay(shape, fill, **kw):
    d = {"shape": shape, "fill": fill}
    d.update(kw)
    return d


def rect(x0, y0, x1, y1, r=0.0):
    return {"t": "rrect", "c": [(x0 + x1) / 2, (y0 + y1) / 2], "half": [(x1 - x0) / 2, (y1 - y0) / 2], "r": r}


def ell(cx, cy, rx, ry, rot=0.0):
    S = {"t": "ellipse", "c": [cx, cy], "r": [rx, ry]}
    if rot:
        S["rot"] = rot
    return S


def circ(cx, cy, r):
    return {"t": "circle", "c": [cx, cy], "r": r}


def U(*S):
    return {"t": "union", "of": list(S)}


def sub(a, b):
    return {"t": "sub", "a": a, "b": b}


def inter(*S):
    return {"t": "inter", "of": list(S)}


def mv(S, dx, dy):
    return {"t": "move", "of": S, "d": [dx, dy]}


def mirror(S, ax):
    """S mirrored about the vertical line x = ax (every shape type used in this round)."""
    t = S["t"]
    S = dict(S)
    fx = lambda p: [2 * ax - p[0], p[1]]  # noqa: E731
    if t in ("circle", "rrect", "ellipse"):
        S["c"] = fx(S["c"])
        if "rot" in S:
            S["rot"] = -S["rot"]
    elif t == "poly":
        S["rings"] = [[fx(p) for p in r] for r in S["rings"]]
    elif t in ("marks.pline", "marks.vline", "stroke"):
        S["pts"] = [fx(p) for p in S["pts"]]
    elif t == "half":
        S["p"] = fx(S["p"])
        S["n"] = [-S["n"][0], S["n"][1]]
    elif t in ("union", "inter"):
        S["of"] = [mirror(q, ax) for q in S["of"]]
    elif t == "sub":
        S["a"], S["b"] = mirror(S["a"], ax), mirror(S["b"], ax)
    elif t in ("move", "grow", "outline"):
        S["of"] = mirror(S["of"], ax)
        if t == "move":
            S["d"] = [-S["d"][0], S["d"][1]]
    else:
        raise ValueError(t)
    return S


def mirror_layers(layers, ax):
    out = []
    for L in layers:
        L = dict(L)
        L["shape"] = mirror(L["shape"], ax)
        if "clip" in L:
            L["clip"] = mirror(L["clip"], ax)
        f = L["fill"]
        if isinstance(f, dict) and "linear" in f:
            l = dict(f["linear"])
            l["a"], l["b"] = [2 * ax - l["a"][0], l["a"][1]], [2 * ax - l["b"][0], l["b"][1]]
            L["fill"] = {"linear": l}
        elif isinstance(f, dict) and "radial" in f:
            r = dict(f["radial"])
            r["c"] = [2 * ax - r["c"][0], r["c"][1]]
            L["fill"] = {"radial": r}
        out.append(L)
    return out


def item(layers, box=(-1, -1, 17, 17)):
    return [{"box": list(box), "layers": layers}]


DISC = circ(7.5, 7.5, 7.5)
DISC_SHADOW = sub(mv(DISC, 1, 1), DISC)
SHADOW = [24, 24, 24]
