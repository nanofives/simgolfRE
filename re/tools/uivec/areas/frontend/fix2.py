"""P41 fix round (2026-11-24, log/roadmap/P41/FIX_BRIEF.md): the frontend area's corrections, written to
log/roadmap/P41/work2/frontend/ (common.py's writers are pointed there; work/ is left as it is).

Shared pieces added for this round (nothing under re/tools/uivec/ is changed):
- path(): one smooth centre line (a polyline sampled densely from arcs / quadratics / straight runs) drawn as a ROUND-capped
  stroke, so a curve that turns back never breaks (FIX_BRIEF point 4).
- capsule(): a pill whose ends are half ellipses (the editor pills: ends 4-5 px deep, full height tall).
- pill_paint(): baked pill lighting: a fill whose colour follows the rows (stops measured down the pill's middle column), a
  highlight that fades from the lit end (alpha and reach fitted by least squares), the outline lit top-left / dark lower-right
  (a linear paint fitted to the line's own pixels) and a cast shadow below (offset, softness and strength fitted).
- glyph_paint(): a glyph's own shading (a linear paint fitted to its pixels: lighter blue at the top, navy at the bottom).
"""
import json
import os

import numpy as np

import common as c
import parts
from common import fit, sdf

c.OUT = os.path.join(c.ROOT, "log", "roadmap", "P41", "work2", "frontend")
c.VIEW = os.path.join(c.OUT, "_view")
c.RES = os.path.join(c.OUT, "results.tsv")
os.makedirs(c.VIEW, exist_ok=True)

DECISIONS = os.path.join(c.ROOT, "log", "roadmap", "P41", "judge", "decisions_final.json")
COLS = ["id", "status", "how", "changed", "unsure"]
NAVY = [33, 41, 80]


def note(eid, area="frontend"):
    d = json.load(open(DECISIONS, encoding="utf-8"))
    return d.get(f"{area}--{eid}", {}).get("note", "")


def _rows():
    out = {}
    if os.path.exists(c.RES):
        lines = open(c.RES, encoding="utf-8").read().splitlines()
        for ln in lines[1:]:
            p = ln.split("\t") + [""] * len(COLS)
            out[p[0]] = dict(zip(COLS, p))
    return out


def _lock():
    import time
    path = c.RES + ".lock"
    for _ in range(600):
        try:
            return os.open(path, os.O_CREAT | os.O_EXCL | os.O_WRONLY), path
        except FileExistsError:
            time.sleep(0.1)
    return None, path


def row(eid, status, how, changed, unsure=""):
    fd, path = _lock()
    try:
        _row(eid, status, how, changed, unsure)
    finally:
        if fd is not None:
            os.close(fd)
            os.remove(path)


def _row(eid, status, how, changed, unsure=""):
    r = _rows()
    clean = lambda s: str(s).replace("\t", " ").replace("\n", " ")
    r[eid] = {"id": eid, "status": status, "how": clean(how), "changed": clean(changed), "unsure": clean(unsure)}
    with open(c.RES, "w", encoding="utf-8") as f:
        f.write("\t".join(COLS) + "\n")
        for k in sorted(r):
            f.write("\t".join(r[k][k2] for k2 in COLS) + "\n")


def deliver(eid, states, how, changed, unsure="", z=None, extra=None):
    """c.element() into work2 (png + json), then this round's results row (id, status, how, changed, unsure)."""
    save = c.result
    c.result = lambda *a, **k: None          # common's 6-column row is not this round's format
    try:
        sc = c.element(eid, states, how, unsure=unsure, z=z, extra=extra)
    finally:
        c.result = save
    if sc is not None:
        row(eid, "done", how, changed, unsure)
        print(eid, "done", sc)
    return sc


# --------------------------------------------------------------------------------------------------------- shapes
def arc_pts(cx, cy, r, a0, a1, n=None):
    """Points along a circle from angle a0 to a1 (degrees, 0 = right, 90 = down), either direction."""
    n = n or max(4, int(abs(a1 - a0) / 6))
    t = np.deg2rad(np.linspace(a0, a1, n + 1))
    return [[float(cx + r * np.cos(v)), float(cy + r * np.sin(v))] for v in t]


def quad_pts(p0, p1, p2, n=12):
    out = []
    for k in range(n + 1):
        u = k / n
        out.append([(1 - u) ** 2 * p0[0] + 2 * u * (1 - u) * p1[0] + u * u * p2[0],
                    (1 - u) ** 2 * p0[1] + 2 * u * (1 - u) * p1[1] + u * u * p2[1]])
    return out


def path(pieces, w):
    """One continuous round-capped line through pieces (lists of points, joined end to start)."""
    pts = []
    for p in pieces:
        for q in p:
            if not pts or abs(pts[-1][0] - q[0]) + abs(pts[-1][1] - q[1]) > 1e-6:
                pts.append([round(float(q[0]), 3), round(float(q[1]), 3)])
    return {"t": "stroke", "pts": pts, "w": w, "cap": "round"}


def poly(*pts):
    return {"t": "poly", "rings": [[list(map(float, p)) for p in pts]]}


def U(*s):
    return {"t": "union", "of": list(s)}


def box(x0, y0, x1, y1, r=0.0):
    return parts.rr(x0, y0, x1, y1, r)


def circ(cx, cy, r):
    return {"t": "circle", "c": [cx, cy], "r": r}


def ell(cx, cy, rx, ry, rot=0.0):
    return {"t": "ellipse", "c": [cx, cy], "r": [rx, ry], "rot": rot}


def mv(s, dx, dy):
    return {"t": "move", "of": s, "d": [dx, dy]}


def sub(a, b):
    return {"t": "sub", "a": a, "b": b}


def capsule(x0, y0, x1, y1, rx):
    """A pill with half-ellipse ends: outer extent (x0, y0)-(x1, y1), each end rx deep and the full height tall."""
    ry = (y1 - y0) / 2
    cy = (y0 + y1) / 2
    return U(box(x0 + rx, y0, x1 - rx, y1), ell(x0 + rx, cy, rx, ry), ell(x1 - rx, cy, rx, ry))


def bbox_of(shape, x0, y0, x1, y1, s=4):
    yy, xx = np.mgrid[0:int((y1 - y0) * s), 0:int((x1 - x0) * s)]
    X, Y = x0 + (xx + 0.5) / s, y0 + (yy + 0.5) / s
    m = sdf.sd(shape, X, Y) < 0
    if not m.any():
        return None
    return float(X[m].min()), float(Y[m].min()), float(X[m].max()), float(Y[m].max())


# ---------------------------------------------------------------------------------------------------------- paints
def glyph_paint(art, shape, box_, fallback=NAVY, core=-0.7):
    """A glyph's shading: a linear paint fitted to the art's glyph pixels (well inside the authored shape)."""
    x0, y0, x1, y1 = [int(v) for v in box_]
    X, Y = parts.grid(x0, y0, x1, y1)
    m = (sdf.sd(shape, X, Y) < core) & (fit.lum(art[y0:y1, x0:x1]) < 120)
    if m.sum() < 6:
        return fallback
    return parts.linear_fit(art[y0:y1, x0:x1], m, x0, y0)


def profile_stops(art, x, y0, y1, n=6):
    """A vertical linear paint whose stops are the art's colours down column x from y0 to y1 (n + 1 stops, row means
    of x-1..x+1)."""
    ys = np.linspace(y0 + 0.5, y1 - 0.5, n + 1)
    stops = []
    for k, yy in enumerate(ys):
        j = int(yy)
        col = art[j, x - 1:x + 2].mean(axis=0)
        stops.append([round(k / n, 4), [round(float(v), 1) for v in col]])
    return {"linear": {"a": [float(x), float(ys[0])], "b": [float(x), float(ys[-1])], "stops": stops}}


def fit_falloff(art, shape_inner, base_paint, light, box_, centres, radii, pows=(1.0, 2.0)):
    """The lit end's highlight: a layer of colour `light` over base_paint, alpha fading from a centre (sdf's falloff).
    Tries each (centre, radius, pow) and fits alpha by least squares on the pixels inside shape_inner. Returns the layer
    dict fields {alpha, falloff} or None when no highlight helps."""
    x0, y0, x1, y1 = box_
    X, Y = parts.grid(x0, y0, x1, y1)
    m = sdf.sd(shape_inner, X, Y) < -0.6
    A = art[y0:y1, x0:x1][m]
    B = sdf.paint(base_paint, X, Y)[m]
    Lc = np.asarray(light, float)
    best = None
    for (cx, cy) in centres:
        for r in radii:
            for p in pows:
                f = np.clip(1 - np.hypot(X - cx, Y - cy) / r, 0, 1)[m] ** p
                D = (Lc - B) * f[:, None]
                den = (D * D).sum()
                if den < 1e-6:
                    continue
                a = float(np.clip(((A - B) * D).sum() / den, 0, 1))
                err = float(((A - B - a * D) ** 2).sum())
                if best is None or err < best[0]:
                    best = (err, a, cx, cy, r, p)
    base_err = float(((A - B) ** 2).sum())
    if best is None or best[0] > 0.97 * base_err or best[1] < 0.05:
        return None
    _, a, cx, cy, r, p = best
    return {"alpha": round(a, 3), "falloff": {"c": [round(cx, 2), round(cy, 2)], "r": r, "pow": p}}


def fit_shadow(art, shape, bg, box_, rgb=(40, 40, 70), dys=(1.0, 1.5, 2.0, 2.5, 3.0), dxs=(0.0, 1.0), softs=(1.0, 1.5, 2.0, 2.5, 3.0),
               mask=None):
    """A cast shadow under shape: shape moved (dx, dy), soft edge, colour rgb at alpha a, over bg (the art with the element
    wiped). Fitted on the pixels outside the shape within box_. Returns the layer or None."""
    x0, y0, x1, y1 = box_
    X, Y = parts.grid(x0, y0, x1, y1)
    out = sdf.sd(shape, X, Y) > 0.7
    if mask is not None:
        out &= mask
    A = art[y0:y1, x0:x1][out]
    B = bg[y0:y1, x0:x1][out]
    Cc = np.asarray(rgb, float)
    best = None
    for dy in dys:
        for dx in dxs:
            for s in softs:
                d = sdf.sd(mv(shape, dx, dy), X, Y)
                u = np.clip(0.5 - d / (2 * s), 0, 1)
                w = (u * u * (3 - 2 * u))[out]
                D = (Cc - B) * w[:, None]
                den = (D * D).sum()
                if den < 1e-6:
                    continue
                a = float(np.clip(((A - B) * D).sum() / den, 0, 1))
                err = float(((A - B - a * D) ** 2).sum())
                if best is None or err < best[0]:
                    best = (err, dx, dy, s, a)
    if best is None or best[4] < 0.05:
        return None
    _, dx, dy, s, a = best
    return {"shape": mv(shape, dx, dy), "fill": list(rgb), "soft": s, "alpha": round(a, 3)}


def wiped(art, scene, rect, grow=1.5):
    """art with the scene's shapes inpainted away inside rect (the background the shapes are drawn over)."""
    x, y, w, h = rect
    m = c.shape_mask(scene, x, y, w, h, grow)
    out = art.copy()
    out[y:y + h, x:x + w] = c.inpaint(art[y:y + h, x:x + w], m)
    return out


def _only(eid):
    """FE_ONLY=<id,id> (env) restricts a run to those elements."""
    only = [v for v in os.environ.get("FE_ONLY", "").split(",") if v]
    return not only or eid in only
