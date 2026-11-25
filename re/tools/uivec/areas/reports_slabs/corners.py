"""Fit the fillet extents (rx, ry) of each corner of an authored slab outline against the sheet's alpha (the edge the
player sees). The vertex positions are read by hand (edges.py); only the corner sizes are fitted, one corner at a time,
on a window around it, by coverage at 4x supersampling.
Usage: py -3.12 corners.py <sheet key>     (prints the fitted verts; build.py reads them from geom/<key>.json)"""
import json
import os
import sys

import numpy as np

import common as C
import slabkit as K
import sheets


def coverage(S, box, s=8):
    """Area coverage of the outline per frame pixel of box, by drawing its ring at s x s supersampling (PIL polygon)."""
    from PIL import Image, ImageDraw
    x0, y0, x1, y1 = box
    ring = K.rpoly_ring(S)
    im = Image.new("L", ((x1 - x0) * s, (y1 - y0) * s), 0)
    ImageDraw.Draw(im).polygon([((p[0] - x0) * s - 0.5, (p[1] - y0) * s - 0.5) for p in ring], fill=255)
    a = np.asarray(im, float) / 255.0
    return a.reshape(y1 - y0, s, x1 - x0, s).mean(axis=(1, 3))


def fit_corner(verts, i, alpha, valid, span=None):
    v = verts[i]
    R = max(v[2], v[3]) + 5
    cx, cy = v[0], v[1]
    box = [int(np.floor(cx - R)), int(np.floor(cy - R)), int(np.ceil(cx + R)), int(np.ceil(cy + R))]
    box = [max(box[0], 0), max(box[1], 0), min(box[2], alpha.shape[1]), min(box[3], alpha.shape[0])]
    A = alpha[box[1]:box[3], box[0]:box[2]]
    m = valid[box[1]:box[3], box[0]:box[2]]
    if m.sum() < 10:
        return v[2], v[3], None
    def err(p, q):
        vv = [list(t) for t in verts]
        vv[i][2], vv[i][3] = float(max(p, 0)), float(max(q, 0))
        return float(np.abs(coverage({"t": "reports_slabs.rpoly", "verts": vv}, box) - A)[m].sum())

    # pattern search from the hand-read sizes: try +/- step on each, halve the step when nothing improves
    p, q = float(v[2]), float(v[3])
    e = err(p, q)
    step = max(p, q, 4.0) / 2
    while step > 0.1:
        moved = False
        for dp, dq in ((step, 0), (-step, 0), (0, step), (0, -step), (step, step), (-step, -step)):
            if p + dp < 0 or q + dq < 0:
                continue
            e2 = err(p + dp, q + dq)
            if e2 < e - 1e-9:
                p, q, e, moved = p + dp, q + dq, e2, True
                break
        if not moved:
            step /= 2
    best = (e, p, q)
    return round(best[1], 2), round(best[2], 2), round(best[0], 2)


def main(key):
    cfg = sheets.SHEETS[key]
    _, alpha, _ = C.art(cfg["sheet"])
    valid = np.zeros(alpha.shape, bool)
    for y0, y1 in cfg["rows"]:
        valid[y0:y1] = True
    out = {}
    for name, verts in cfg["outlines"].items():
        verts = [list(map(float, v)) for v in verts]
        for i in range(len(verts)):
            if verts[i][2] <= 0 and verts[i][3] <= 0:
                continue
            a, b, e = fit_corner(verts, i, alpha, valid)
            print(f"{name} corner {i} at {verts[i][:2]}: rx {verts[i][2]} -> {a}, ry {verts[i][3]} -> {b} (err {e})", flush=True)
            if e is not None:
                verts[i][2], verts[i][3] = a, b
        out[name] = verts
    os.makedirs(os.path.join(C.HERE, "geom"), exist_ok=True)
    p = os.path.join(C.HERE, "geom", key + ".json")
    old = json.load(open(p)) if os.path.exists(p) else {}
    old.update({"outlines": out})
    json.dump(old, open(p, "w"), indent=1)
    print(p)


if __name__ == "__main__":
    main(sys.argv[1])
