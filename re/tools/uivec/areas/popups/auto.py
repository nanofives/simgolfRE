"""P41 popups area: an element as AUTHORED shapes whose paints are MEASURED on the art.

measured_item(rel, box, parts, ground): parts is a list, bottom to top, of (shape, how) with how one of
  "flat"                          the median colour of the pixels the part shows (inside it, not under a later part)
  [r, g, b]                       a colour read beforehand
  {"bevel": {...options}}         the toolkit's bevel paint measured on those pixels (paint.fit_bevel): options
                                  stops (px of depth, default 0.5 .. 9), L (default 1: stops are pixels), radial (cx, cy)
  {"paint": P}                    a paint given as is
Every part may carry extra layer keys in a dict as a third element (alpha, clip, soft...).
A pixel counts for a part when its centre is at least `inset` px inside the part and at least `gap` px outside every later
part (so anti-aliased edges and the parts drawn over it do not bend the measurement).
"""
import numpy as np

import common as c
import paint

PX_STOPS = [0.5, 1.5, 2.5, 3.5, 5.0, 7.0, 10.0]


def measured_item(rel, box, parts, ground=c.FLAT, inset=0.35, gap=0.6, name=""):
    a = c.art(rel, box, ground)
    rgb, m, _ = c.sheet(rel)
    x0, y0, x1, y1 = box
    jj, ii = np.mgrid[y0:y1, x0:x1]
    x, y = ii + 0.5, jj + 0.5
    opaque = m[y0:y1, x0:x1]
    ds = [c.sdf.sd(p[0], x, y) for p in parts]
    layers = []
    for k, part in enumerate(parts):
        S, how = part[0], part[1]
        extra = part[2] if len(part) > 2 else {}
        own = (ds[k] < -inset) & opaque
        for d2 in ds[k + 1:]:
            own &= d2 > gap
        if isinstance(how, str) and how == "flat":
            fill = [round(float(v), 1) for v in np.median(a[own], axis=0)] if own.any() else [128, 128, 128]
        elif isinstance(how, dict) and "bevel" in how:
            o = how["bevel"]
            stops = o.get("stops", PX_STOPS)
            L = o.get("L", 1.0)
            fill, _ = paint.fit_bevel(o.get("of", S), a, (x0, y0), stops, L, own, radial_c=o.get("radial"))
            if "of" in o:
                fill["bevel"]["of"] = o["of"]
        elif isinstance(how, dict) and "linear" in how:
            o = how["linear"]
            fill = paint.fit_linear(a, (x0, y0), own, o.get("n", 3), o.get("dir"))
        elif isinstance(how, dict) and "paint" in how:
            fill = how["paint"]
        else:
            fill = list(how)
        Ld = {"shape": S, "fill": fill}
        Ld.update(extra)
        layers.append(Ld)
    return c.item(box, layers, name)


def deliver(eid, rel, states, ground=c.FLAT, how="", unsure="", z=None, status="done"):
    """states: list of (state, box, item). Writes the comparison image, the scene json and the results row."""
    rows, out, mads, edges = [], {}, [], []
    for st, box, it in states:
        sc = [it]
        mad, w16 = c.score(sc, rel, box, ground)
        e = c.edge_px(sc, rel, box, ground)
        mads.append(mad)
        if e is not None:
            edges.append(e)
        rows.append((sc, rel, box, ground))
        out[st] = {"box": list(box), "scene": sc}
        print(f"  {eid:22s} {st:12s} mad {mad:6.2f} w16 {w16:5.3f} edge {e if e is None else round(e, 2)}")
    p = c.compare(rows, eid, zoom_k=z)
    c.save_scene(eid, {"id": eid, "sheet": "Interface/" + rel, "states": out})
    c.record(eid, status, how, float(np.mean(mads)), float(np.mean(edges)) if edges else None, unsure)
    return p


def poly(*pts, smooth=0):
    S = {"t": "poly", "rings": [[list(p) for p in pts]]}
    if smooth:
        S["smooth"] = smooth
    return S


def ell(cx, cy, rx, ry, rot=0.0):
    return {"t": "ellipse", "c": [cx, cy], "r": [rx, ry], "rot": rot}


def mv(S, dx, dy):
    return {"t": "move", "of": S, "d": [dx, dy]}
