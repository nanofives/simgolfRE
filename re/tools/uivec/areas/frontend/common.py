"""P41 frontend area: shared helpers (sheets, gridded views, wiping an element out of its art, the comparison image, scoring,
writing <id>.png / <id>.json / results rows). Imports the toolkit read-only; nothing under re/tools/uivec/ is changed.
"""
import json
import os
import sys

import numpy as np
from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
UIVEC = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, UIVEC)
import fit      # noqa: E402
import sdf      # noqa: E402

ROOT = fit.ROOT
OUT = os.path.join(ROOT, "log", "roadmap", "P41", "work", "frontend")
VIEW = os.path.join(OUT, "_view")
os.makedirs(VIEW, exist_ok=True)

_sheets = {}


def sheet(rel):
    """(rgb float H x W x 3, opaque mask) of a PCX under original\\."""
    if rel not in _sheets:
        idx, pal = fit.load_sheet(rel)
        _sheets[rel] = (pal[idx].astype(float), idx < 0xfe)
    return _sheets[rel]


def over(base, top, rect=None, at=None):
    """base sheet with the opaque pixels of top laid over it; rect (x, y, w, h) in top taken to screen point at (default
    the same place)."""
    a = sheet(base)[0].copy()
    t, m = sheet(top)
    if rect is None:
        a[m] = t[m]
        return a
    x, y, w, h = rect
    ax, ay = at or (x, y)
    sub, sm = t[y:y + h, x:x + w], m[y:y + h, x:x + w]
    dst = a[ay:ay + h, ax:ax + w]
    dst[sm] = sub[sm]
    return a


def zoom(img, x, y, w, h, s=8, name="zoom", grid=2):
    """An enlarged gridded view (nearest neighbour) of img's box, red lines every `grid` px, labels every 10."""
    a = np.clip(img[y:y + h, x:x + w], 0, 255).astype(np.uint8)
    big = Image.fromarray(a).resize((w * s, h * s), Image.NEAREST)
    d = ImageDraw.Draw(big)
    for g in range(0, max(w, h) + 1):
        if (x + g) % grid == 0 and g <= w:
            major = (x + g) % 10 == 0
            d.line([(g * s, 0), (g * s, h * s)], fill=(255, 0, 0) if major else (255, 170, 170))
            if major:
                d.text((g * s + 2, 2), str(x + g), fill=(255, 255, 0))
        if (y + g) % grid == 0 and g <= h:
            major = (y + g) % 10 == 0
            d.line([(0, g * s), (w * s, g * s)], fill=(255, 0, 0) if major else (255, 170, 170))
            if major:
                d.text((2, g * s + 2), str(y + g), fill=(255, 255, 0))
    p = os.path.join(VIEW, name + ".png")
    big.save(p)
    return p


def dump(img, x, y, w, h, lo=None):
    """Luminance of a box as text, one char per pixel (dark '#', mid '+', light '.'), with column ruler."""
    L = fit.lum(img[y:y + h, x:x + w])
    lo = lo or (70, 140)
    out = ["      " + "".join(str((x + i) // 10 % 10) if (x + i) % 10 == 0 else " " for i in range(w))]
    for j in range(h):
        out.append(f"{y + j:5d} " + "".join("#" if v < lo[0] else "+" if v < lo[1] else "." for v in L[j]))
    return "\n".join(out)


def inpaint(img, mask, iters=4000):
    """Fill the pixels under mask from their neighbours (onion peel, mean of the known 8-neighbours)."""
    out = img.copy()
    known = ~mask.copy()
    H, W = mask.shape
    for _ in range(iters):
        if known.all():
            break
        k = known.astype(float)
        acc = np.zeros_like(out)
        cnt = np.zeros((H, W))
        P = np.pad(out * k[..., None], ((1, 1), (1, 1), (0, 0)))
        K = np.pad(k, 1)
        for dj in (-1, 0, 1):
            for di in (-1, 0, 1):
                if dj == 0 and di == 0:
                    continue
                acc += P[1 + dj:1 + dj + H, 1 + di:1 + di + W]
                cnt += K[1 + dj:1 + dj + H, 1 + di:1 + di + W]
        new = (~known) & (cnt > 0)
        if not new.any():
            break
        out[new] = acc[new] / cnt[new][:, None]
        known = known | new
    return out


def shape_mask(scene, x0, y0, w, h, grow=1.5):
    """Pixels of the box (x0, y0, w, h) within `grow` px of any layer's shape of the scene (what the element covers)."""
    yy, xx = np.mgrid[y0:y0 + h, x0:x0 + w]
    X, Y = xx + 0.5, yy + 0.5
    m = np.zeros((h, w), bool)
    for it in scene:
        for L in it["layers"]:
            if L.get("nowipe"):
                continue
            k = sdf.sd(L["shape"], X, Y) < grow
            if "clip" in L:
                k &= sdf.sd(L["clip"], X, Y) < grow
            m |= k
    return m


def render_crop(bg, scene, rect, s):
    """bg (frame image) box rect rendered at s x with the scene drawn over it (float, h*s x w*s x 3)."""
    x, y, w, h = rect
    img = np.kron(bg[y:y + h, x:x + w], np.ones((s, s, 1)))
    sdf.render(scene, img, s, origin=(x, y))
    return img


def down(img, s):
    h, w = img.shape[0] // s, img.shape[1] // s
    return img.reshape(h, s, w, s, 3).mean(axis=(1, 3))


def scores(art, low, inside):
    """interior_mad: mean |art - shapes| over the element's interior (inside, eroded 2 px); edge_px: median distance between
    where the luminance edges of the art and of the shapes fall (both found as |gradient| peaks along rows and columns)."""
    m = inside.copy()
    for _ in range(2):
        m = m & np.roll(m, 1, 0) & np.roll(m, -1, 0) & np.roll(m, 1, 1) & np.roll(m, -1, 1)
    d = np.abs(art - low).mean(axis=2)
    imad = float(d[m].mean()) if m.any() else float(d.mean())
    la, lv = fit.lum(art), fit.lum(low)
    dist = []
    for A, V in ((la, lv), (la.T, lv.T)):
        ga, gv = np.abs(np.diff(A, axis=1)), np.abs(np.diff(V, axis=1))
        for j in range(0, A.shape[0], 2):
            pk = np.nonzero((ga[j] > 40) & (ga[j] >= np.roll(ga[j], 1)) & (ga[j] >= np.roll(ga[j], -1)))[0]
            pv = np.nonzero((gv[j] > 25) & (gv[j] >= np.roll(gv[j], 1)) & (gv[j] >= np.roll(gv[j], -1)))[0]
            if len(pk) == 0 or len(pv) == 0:
                continue
            for p in pk:
                dist.append(float(np.min(np.abs(pv - p))))
    edge = float(np.mean(np.minimum(dist, 3.0))) if dist else float("nan")
    return round(imad, 2), round(edge, 2)


def compare(states, z, vertical=False):
    """states: list of (art crop 1:1, shapes image at z). Each row: art (nearest) | 8 px gap | shapes. Rows stacked.
    vertical (long thin elements that would not reach 2x side by side under 1600 px): art above, shapes below."""
    rows = []
    for art, hi in states:
        a = np.kron(art, np.ones((z, z, 1)))
        if hi.shape[:2] != a.shape[:2]:
            hi = np.asarray(Image.fromarray(np.clip(hi + 0.5, 0, 255).astype(np.uint8)).resize((a.shape[1], a.shape[0]), Image.LANCZOS), float)
        if vertical:
            rows.append(np.concatenate([a, np.full((4, a.shape[1], 3), 255.0), hi], axis=0))
            continue
        gap = np.full((a.shape[0], max(0, min(8, 1600 - 2 * a.shape[1])), 3), 255.0)
        rows.append(np.concatenate([a, gap, hi], axis=1))
    W = max(r.shape[1] for r in rows)
    rows = [np.pad(r, ((0, 0), (0, W - r.shape[1]), (0, 0)), constant_values=255) for r in rows]
    sep = np.full((8, W, 3), 255.0)
    out = rows[0]
    for r in rows[1:]:
        out = np.concatenate([out, sep, r])
    return out


def zoom_for(w, h):
    z = max(1, int(np.ceil(300 / max(min(w, h), 1))))
    while z > 1 and (2 * w * z + 8) > 1600:
        z -= 1
    return min(z, 8)


_results = {}
RES = os.path.join(OUT, "results.tsv")
COLS = ["id", "status", "how", "interior_mad", "edge_px", "unsure"]


def _load_results():
    _results.clear()
    if os.path.exists(RES):
        with open(RES, encoding="utf-8") as f:
            lines = f.read().splitlines()
        for ln in lines[1:]:
            p = ln.split("\t")
            p += [""] * (len(COLS) - len(p))
            _results[p[0]] = dict(zip(COLS, p))


def result(eid, status, how, imad="", edge="", unsure=""):
    _load_results()
    _results[eid] = {"id": eid, "status": status, "how": how.replace("\t", " ").replace("\n", " "),
                     "interior_mad": "" if imad == "" else str(imad), "edge_px": "" if edge == "" else str(edge),
                     "unsure": unsure.replace("\t", " ").replace("\n", " ")}
    with open(RES, "w", encoding="utf-8") as f:
        f.write("\t".join(COLS) + "\n")
        for k in sorted(_results):
            f.write("\t".join(_results[k][c] for c in COLS) + "\n")


def save_png(img, eid):
    p = os.path.join(OUT, eid + ".png")
    Image.fromarray(np.clip(img + 0.5, 0, 255).astype(np.uint8)).save(p)
    return p


def save_json(obj, eid):
    with open(os.path.join(OUT, eid + ".json"), "w") as f:
        json.dump(obj, f, default=lambda o: o.item() if hasattr(o, "item") else list(o))


def element(eid, states, how, unsure="", z=None, extra=None, status="done", score_state=0):
    """Deliver one element. states: list of dicts {name, art (frame image the art state is in), bg (frame image with the
    element wiped; default: the art inpainted under the scene's shapes), scene, rect}. Writes <id>.png and <id>.json and
    the results row; returns (imad, edge) of state score_state. FE_ONLY=<id,id> (env) builds only those."""
    only = [v for v in os.environ.get("FE_ONLY", "").split(",") if v]
    if only and eid not in only:
        return None
    rect0 = states[0]["rect"]
    z = z or zoom_for(rect0[2], rect0[3])
    vertical = False
    if rect0[2] >= 3 * rect0[3] and 2 * rect0[2] * max(z, 2) + 8 > 1600 and rect0[2] * 2 <= 1600:
        vertical, z = True, max(2, min(z if z > 1 else 3, 1600 // rect0[2]))
    while not vertical and z > 1 and 2 * rect0[2] * z + 8 > 1600:
        z -= 1
    tiles, sc = [], None
    for k, st in enumerate(states):
        x, y, w, h = st["rect"]
        art = st["art"]
        bg = st.get("bg")
        if bg is None:
            m = shape_mask(st["scene"], x, y, w, h, st.get("grow", 1.5))
            crop = inpaint(art[y:y + h, x:x + w], m)
            bg = art.copy()
            bg[y:y + h, x:x + w] = crop
        hi = render_crop(bg, st["scene"], st["rect"], z)
        tiles.append((art[y:y + h, x:x + w], hi))
        if k == score_state:
            s = 4
            low = down(render_crop(bg, st["scene"], st["rect"], s), s)
            inside = shape_mask(st["scene"], x, y, w, h, -0.5)
            sc = scores(art[y:y + h, x:x + w], low, inside)
    save_png(compare(tiles, z, vertical), eid)
    save_json({"id": eid, "states": [{"name": st["name"], "rect": list(st["rect"]), "scene": st["scene"]} for st in states],
               **(extra or {})}, eid)
    result(eid, status, how, sc[0], sc[1], unsure)
    return sc


def kept(eid, how, art=None, rect=None, unsure=""):
    """An element that stays art: the row, and (when given) the art crop beside itself so the sheet is complete."""
    if art is not None:
        x, y, w, h = rect
        z = zoom_for(w, h)
        a = art[y:y + h, x:x + w]
        save_png(compare([(a, np.kron(a, np.ones((z, z, 1))))], z), eid)
        save_json({"id": eid, "kept_art": True, "rect": list(rect)}, eid)
    result(eid, "kept_art", how, "", "", unsure)
