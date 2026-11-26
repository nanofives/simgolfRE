"""P41 round 6 (log/roadmap/P41/FIX6_BRIEF.md), area reports_slabs: shared helpers.

Output goes to log/roadmap/P41/work6/reports_slabs/. The previous drawing of an element is its approved scene in
re/tools/uivec/scenes/reports_slabs/<id>.json; the comparison images are art LEFT, previous MIDDLE, new RIGHT.
"""
import copy
import json
import math
import os

import numpy as np
from PIL import Image, ImageDraw

import common as C
import slabkit as K

OUT6 = os.path.join(C.ROOT, "log", "roadmap", "P41", "work6", "reports_slabs")
os.makedirs(OUT6, exist_ok=True)
APPROVED = os.path.join(C.UIVEC, "scenes", "reports_slabs")
NAVY = [32.0, 42.0, 75.0]                    # the wire paint of the approved slab scenes
SHADOW = {"fill": [40, 40, 80], "alpha": 0.3, "soft": 1.2, "d": [0.5, 2.0]}


def approved(eid):
    return json.load(open(os.path.join(APPROVED, eid + ".json"), encoding="utf-8"))


def save(eid, doc):
    with open(os.path.join(OUT6, eid + ".json"), "w", encoding="utf-8", newline="\n") as f:
        json.dump(doc, f, indent=1)


def arc_pts(c, r, a0, a1, step_deg=10.0):
    """points on a circular arc from angle a0 to a1 (degrees, y down: 90 = +y)"""
    n = max(2, int(math.ceil(abs(a1 - a0) / step_deg)) + 1)
    return [[c[0] + r * math.cos(math.radians(a0 + (a1 - a0) * k / (n - 1))),
             c[1] + r * math.sin(math.radians(a0 + (a1 - a0) * k / (n - 1)))] for k in range(n)]


def wire_item(pts, w, pad=3.0):
    S = {"t": "stroke", "pts": [[float(x), float(y)] for x, y in pts], "w": float(w), "cap": "butt"}
    return K.item([{"shape": S, "fill": list(NAVY)}], pad)


def shadow_item(pts, w):
    S = {"t": "stroke", "pts": [[float(x), float(y)] for x, y in pts], "w": float(w), "cap": "butt"}
    L = {"shape": {"t": "move", "of": S, "d": list(SHADOW["d"])}, "fill": list(SHADOW["fill"]),
         "alpha": SHADOW["alpha"], "soft": SHADOW["soft"]}
    return {"box": K.bbox(L["shape"], 3.0 + SHADOW["soft"] * 2), "layers": [L]}


def three_up(path, sheet, prev_scene, new_scene, rows, art=None):
    """rows: [(label, (x0, y0, x1, y1), scale)]; writes art | previous | new per row, stacked, labelled."""
    if art is None:
        art = C.art(sheet)[0]
    tiles = []
    for lab, box, s in rows:
        x0, y0, x1, y1 = box
        A = np.kron(art[y0:y1, x0:x1], np.ones((s, s, 1)))
        P = K.render(prev_scene, box, s)
        N = K.render(new_scene, box, s)
        gap = np.zeros((A.shape[0], 8, 3))
        gap[:] = (20, 20, 20)
        tiles.append((lab, np.concatenate([A, gap, P, gap, N], axis=1)))
    Wt = max(t.shape[1] for _, t in tiles)
    out = []
    for lab, t in tiles:
        head = np.zeros((16, Wt, 3))
        head[:] = (20, 20, 20)
        body = np.zeros((t.shape[0], Wt, 3))
        body[:] = (20, 20, 20)
        body[:, :t.shape[1]] = t
        out += [head, body]
    im = Image.fromarray(K.to8(np.concatenate(out, axis=0)))
    d = ImageDraw.Draw(im)
    yy = 0
    for lab, t in tiles:
        d.text((4, yy + 2), f"{lab}: art (left) | previous approved (middle) | new (right)", fill=(255, 255, 0))
        yy += 16 + t.shape[0]
    im.save(path)
    return im.size


def deep(x):
    return copy.deepcopy(x)
