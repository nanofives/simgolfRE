"""P41 popups area: shared helpers (sheet access, gridded zooms, comparison sheets, scoring).

Nothing here writes outside log/roadmap/P41/work/popups/. The toolkit (re/tools/uivec/*.py) is imported read-only.
"""
import json
import os
import sys

import numpy as np
from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
UIVEC = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, UIVEC)
import fit  # noqa: E402
import sdf  # noqa: E402

ROOT = fit.ROOT
OUT = os.path.join(ROOT, "log", "roadmap", "P41", "work", "popups")
SCR = os.path.join(OUT, "_read")          # reading aids (zooms), not deliverables
os.makedirs(SCR, exist_ok=True)
FLAT = np.array((70, 110, 90), float)     # what the roster puts under a transparent pixel

_sheets = {}


def sheet(rel):
    """(rgb float H x W x 3, opaque mask, index array) of a sheet under original\\Interface (rel without 'Interface/')."""
    if not rel.startswith("Interface/"):
        rel = "Interface/" + rel
    if rel not in _sheets:
        idx, pal = fit.load_sheet(rel)
        _sheets[rel] = (pal[idx].astype(float), idx < 0xfe, idx)
    return _sheets[rel]


def art(rel, box, ground=FLAT):
    """The art of box (x0, y0, x1, y1) with its transparent pixels on `ground`."""
    rgb, m, _ = sheet(rel)
    x0, y0, x1, y1 = box
    a = rgb[y0:y1, x0:x1].copy()
    a[~m[y0:y1, x0:x1]] = ground
    return a


def zoom(rel, box, s=14, name="zoom", ground=FLAT, step=2):
    """An enlarged gridded view of a sheet box for reading shapes by eye: thin lines every `step` px, labels every 10."""
    x0, y0, x1, y1 = box
    a = art(rel, box, ground)
    w, h = x1 - x0, y1 - y0
    big = Image.fromarray(np.clip(a, 0, 255).astype(np.uint8)).resize((w * s, h * s), Image.NEAREST)
    d = ImageDraw.Draw(big)
    for g in range(0, max(w, h) + 1):
        X, Y = x0 + g, y0 + g
        if g <= w and X % step == 0:
            d.line([(g * s, 0), (g * s, h * s)], fill=(255, 0, 0) if X % 10 == 0 else (255, 170, 170))
            if X % 10 == 0:
                d.text((g * s + 2, 2), str(X), fill=(255, 255, 0))
        if g <= h and Y % step == 0:
            d.line([(0, g * s), (w * s, g * s)], fill=(255, 0, 0) if Y % 10 == 0 else (255, 170, 170))
            if Y % 10 == 0:
                d.text((2, g * s + 2), str(Y), fill=(255, 255, 0))
    p = os.path.join(SCR, name + ".png")
    big.save(p)
    return p


def dump(rel, box, fn=None, chars=" .:+#"):
    """Text view of a box: one char per pixel, from fn(rgb) in 0..1 (default: darkness)."""
    a = art(rel, box)
    v = fn(a) if fn else 1 - fit.lum(a) / 255.0
    x0, y0, x1, y1 = box
    lines = ["     " + "".join(str(x // 10 % 10) for x in range(x0, x1)), "     " + "".join(str(x % 10) for x in range(x0, x1))]
    for j in range(y1 - y0):
        lines.append(f"{y0 + j:4d} " + "".join(chars[min(int(c * len(chars)), len(chars) - 1)] for c in v[j]))
    return "\n".join(lines)


def pix(rel, x, y):
    rgb, m, idx = sheet(rel)
    return tuple(int(v) for v in rgb[y, x]), int(idx[y, x])


def render_box(scene, box, s, ground):
    """The scene rendered at scale s over a plain ground (a colour, or an H x W x 3 array at 1x)."""
    x0, y0, x1, y1 = box
    w, h = x1 - x0, y1 - y0
    if np.ndim(ground) == 1:
        img = np.empty((h * s, w * s, 3))
        img[:] = ground
    else:
        img = np.kron(np.asarray(ground, float), np.ones((s, s, 1)))
    sdf.render(scene, img, s, origin=(x0, y0))
    return img


def score(scene, rel, box, ground=FLAT, mask=None, s=5):
    """(interior MAD, share of pixels within 16 levels) of the scene at 1x against the art over the opaque pixels."""
    x0, y0, x1, y1 = box
    low = render_box(scene, box, s, ground).reshape(y1 - y0, s, x1 - x0, s, 3).mean(axis=(1, 3))
    a = art(rel, box, ground)
    rgb, m, _ = sheet(rel)
    mm = m[y0:y1, x0:x1] if mask is None else mask
    d = np.abs(low - a)[mm]
    return float(d.mean()), float((d.max(axis=1) <= 16).mean())


def edge_px(scene, rel, box, ground=FLAT, s=5, thr=40.0):
    """Mean distance (px) between strong colour edges of the art and of the render (symmetric, capped at 4)."""
    x0, y0, x1, y1 = box
    low = render_box(scene, box, s, ground).reshape(y1 - y0, s, x1 - x0, s, 3).mean(axis=(1, 3))
    a = art(rel, box, ground)

    def edges(q):
        L = fit.lum(q)
        gx = np.abs(np.diff(L, axis=1, prepend=L[:, :1]))
        gy = np.abs(np.diff(L, axis=0, prepend=L[:1]))
        return np.maximum(gx, gy) > thr

    ea, eb = edges(a), edges(low)
    if ea.sum() == 0 or eb.sum() == 0:
        return None

    def dist_to(mask):
        pts = np.argwhere(mask)
        out = np.full(mask.shape, 4.0)
        jj, ii = np.mgrid[0:mask.shape[0], 0:mask.shape[1]]
        for dj in range(-4, 5):
            for di in range(-4, 5):
                sh = np.zeros_like(mask)
                ys = slice(max(dj, 0), mask.shape[0] + min(dj, 0))
                yd = slice(max(-dj, 0), mask.shape[0] + min(-dj, 0))
                xs = slice(max(di, 0), mask.shape[1] + min(di, 0))
                xd = slice(max(-di, 0), mask.shape[1] + min(-di, 0))
                sh[yd, xd] = mask[ys, xs]
                out = np.where(sh, np.minimum(out, np.hypot(dj, di)), out)
        return out
    return float((dist_to(eb)[ea].mean() + dist_to(ea)[eb].mean()) / 2)


def compare(rows, out_name, zoom_k=None):
    """rows: list of (scene, rel, box, ground). Each row = art (nearest) | shapes, same scale; stacked top to bottom."""
    tiles = []
    for scene, rel, box, ground in rows:
        x0, y0, x1, y1 = box
        w, h = x1 - x0, y1 - y0
        z = zoom_k or max(1, min(8, int(np.ceil(300 / min(w, h)))))
        while 2 * w * z + 8 > 1600 and z > 1:
            z -= 1
        a = np.kron(art(rel, box, ground if np.ndim(ground) == 1 else FLAT) if np.ndim(ground) == 1 else _art_over(rel, box, ground),
                    np.ones((z, z, 1)))
        r = render_box(scene, box, z, ground)
        sep = np.zeros((h * z, 8, 3)); sep[:] = (255, 255, 255)
        tiles.append(np.concatenate([a, sep, r], axis=1))
    W = max(t.shape[1] for t in tiles)
    sheet_ = np.concatenate([np.pad(t, ((0, 6), (0, W - t.shape[1]), (0, 0)), constant_values=255) for t in tiles])
    p = os.path.join(OUT, out_name + ".png")
    Image.fromarray(np.clip(sheet_ + 0.5, 0, 255).astype(np.uint8)).save(p)
    return p


def _art_over(rel, box, ground):
    a = art(rel, box)
    rgb, m, _ = sheet(rel)
    x0, y0, x1, y1 = box
    g = np.asarray(ground, float)
    a[~m[y0:y1, x0:x1]] = g[~m[y0:y1, x0:x1]]
    return a


def save_scene(eid, data):
    with open(os.path.join(OUT, eid + ".json"), "w") as f:
        json.dump(data, f, indent=1)


RESULTS = os.path.join(OUT, "results.tsv")
COLS = ("id", "status", "how", "interior_mad", "edge_px", "unsure")


def record(eid, status, how, mad=None, edge=None, unsure=""):
    rows = {}
    if os.path.exists(RESULTS):
        with open(RESULTS, encoding="utf-8") as f:
            for ln in f.read().splitlines()[1:]:
                p = ln.split("\t")
                rows[p[0]] = p
    rows[eid] = [eid, status, how.replace("\t", " "), "" if mad is None else f"{mad:.2f}",
                 "" if edge is None else f"{edge:.2f}", unsure.replace("\t", " ")]
    with open(RESULTS, "w", encoding="utf-8") as f:
        f.write("\t".join(COLS) + "\n")
        for k in sorted(rows):
            f.write("\t".join(rows[k]) + "\n")


# ------------------------------------------------------------------------------------------------- small shape helpers
def rr(x0, y0, x1, y1, r, rot=0.0):
    return {"t": "rrect", "c": [(x0 + x1) / 2, (y0 + y1) / 2], "half": [(x1 - x0) / 2, (y1 - y0) / 2], "r": r, "rot": rot}


def circ(cx, cy, r):
    return {"t": "circle", "c": [cx, cy], "r": r}


def poly(*pts, smooth=0):
    S = {"t": "poly", "rings": [[list(p) for p in pts]]}
    if smooth:
        S["smooth"] = smooth
    return S


def L(shape, fill, **kw):
    d = {"shape": shape, "fill": list(fill) if not isinstance(fill, dict) else fill}
    d.update(kw)
    return d


def item(box, layers, name=""):
    return {"name": name, "box": list(box), "layers": layers}
