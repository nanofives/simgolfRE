"""reports_maps area (P41): shared helpers. Sheet access, crops, enlarged gridded views, comparison images."""
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
OUT = os.path.join(ROOT, "log", "roadmap", "P41", "work", "reports_maps")
IS = "Interface/infoscreens/"
_cache = {}


def sheet(name):
    """(rgb float H x W x 3, opaque mask) of a sheet under Interface/infoscreens/."""
    if name not in _cache:
        idx, pal = fit.load_sheet(IS + name)
        _cache[name] = (pal[idx].astype(float), idx < 0xfe, idx, pal)
    return _cache[name][:2]


def raw(name):
    sheet(name)
    return _cache[name][2:]


def grid(rgb, x0, y0, s, step=2, major=10):
    """rgb (h x w x 3) enlarged s times (nearest) with a pixel grid; labels in sheet coordinates (x0, y0 = its corner)."""
    h, w = rgb.shape[:2]
    big = Image.fromarray(np.clip(rgb + 0.5, 0, 255).astype(np.uint8)).resize((w * s, h * s), Image.NEAREST)
    d = ImageDraw.Draw(big)
    for g in range(0, max(w, h)):
        if (x0 + g) % step == 0 and g < w:
            mj = (x0 + g) % major == 0
            d.line([(g * s, 0), (g * s, h * s)], fill=(255, 0, 0) if mj else (255, 170, 170))
            if mj:
                d.text((g * s + 2, 2), str(x0 + g), fill=(255, 255, 0))
        if (y0 + g) % step == 0 and g < h:
            mj = (y0 + g) % major == 0
            d.line([(0, g * s), (w * s, g * s)], fill=(255, 0, 0) if mj else (255, 170, 170))
            if mj:
                d.text((2, g * s + 2), str(y0 + g), fill=(255, 255, 0))
    return big


def save_grid(name, box, s, out):
    rgb, m = sheet(name)
    x, y, w, h = box
    grid(rgb[y:y + h, x:x + w], x, y, s).save(out)
    return out


def dump(rgb, x0, y0, fn):
    """A text view: one char per pixel from fn(rgb) -> 0..1."""
    v = fn(rgb)
    h, w = v.shape
    lines = ["     " + "".join(str((x0 + i) // 10 % 10) for i in range(w)), "     " + "".join(str((x0 + i) % 10) for i in range(w))]
    for j in range(h):
        lines.append(f"{y0 + j:4d} " + "".join("#" if c > 0.85 else "+" if c > 0.6 else ":" if c > 0.35 else "." if c > 0.12 else " "
                                               for c in v[j]))
    return "\n".join(lines)


def render_on(base, scene, s, origin):
    """base (h x w x 3, art pixel grid) enlarged s x (nearest) and the scene painted on it; returns (hi, low)."""
    img = np.kron(base, np.ones((s, s, 1)))
    sdf.render(scene, img, s, origin=origin)
    h, w = base.shape[:2]
    low = img.reshape(h, s, w, s, 3).mean(axis=(1, 3))
    return img, low


def compare_tile(art, base, scene, origin, z, s_score=None):
    """art beside the shapes, both at z x. Returns (tile, low-res render)."""
    h, w = art.shape[:2]
    s = z
    img = np.kron(base, np.ones((s, s, 1)))
    sdf.render(scene, img, s, origin=origin)
    low = img.reshape(h, s, w, s, 3).mean(axis=(1, 3))
    left = np.kron(art, np.ones((z, z, 1)))
    gap = np.full((h * z, 6, 3), 255.0)
    return np.concatenate([left, gap, img], axis=1), low


def stack(tiles, gap=6):
    W = max(t.shape[1] for t in tiles)
    out = []
    for k, t in enumerate(tiles):
        out.append(np.pad(t, ((0, gap if k < len(tiles) - 1 else 0), (0, W - t.shape[1]), (0, 0)), constant_values=255))
    return np.concatenate(out)


def save_png(a, path):
    Image.fromarray(np.clip(a + 0.5, 0, 255).astype(np.uint8)).save(path)
    return path


def save_json(obj, path):
    with open(path, "w") as f:
        json.dump(obj, f, indent=1)


def mad(a, b, mask=None):
    d = np.abs(a - b)
    if mask is not None:
        d = d[mask]
    return float(d.mean())


def dark_centre(rgb, x, y, dx, dy, n, ref=None):
    """Walk n pixels from (x, y) by (dx, dy); the darkness-weighted centre of the darkest line met (frame coordinates of the
    pixel centre) and its weight. ref: the luminance taken as 'no line' (default: the brightest end)."""
    L = fit.lum(rgb)
    v, pts = [], []
    for k in range(n):
        xx, yy = x + dx * k, y + dy * k
        if not (0 <= xx < L.shape[1] and 0 <= yy < L.shape[0]):
            break
        v.append(float(L[yy, xx]))
        pts.append((xx + 0.5, yy + 0.5))
    v = np.array(v)
    k0 = int(np.argmin(v))
    lo = max(k0 - 3, 0)
    hi = min(k0 + 4, len(v))
    base = ref if ref is not None else max(v[lo], v[hi - 1])
    w = np.clip(base - v[lo:hi], 0, None)
    if w.sum() < 1e-6:
        return None, 0.0
    t = (w * np.arange(lo, hi)).sum() / w.sum()
    return (x + 0.5 + dx * t, y + 0.5 + dy * t), float(w.sum())


def solve_bevel_near(rows, stops, dir_until=0.3, lam_k=1e-3):
    """accept.solve_bevel, but the edge-direction terms are kept only on the stops up to `dir_until` (deeper inside, a lit
    surface has one colour per depth): on a polygon the nearest edge flips across the middle ridges, and direction terms
    there paint a seam."""
    import sdf as _sdf
    A = np.concatenate([np.stack([w for row in _sdf.bevel_terms(u, nx, ny, stops) for w in row], axis=1)
                        for u, nx, ny, _ in rows])
    Y = np.concatenate([rgb for _, _, _, rgb in rows])
    keep = [k * 5 + h for k, t in enumerate(stops) for h in range(5) if h == 0 or t <= dir_until]
    As = A[:, keep]
    lam = lam_k * len(As)
    G = As.T @ As + lam * np.eye(As.shape[1])
    prior = np.zeros((As.shape[1], 3))
    mean = Y.mean(0)
    for c, k in enumerate(keep):
        if k % 5 == 0:
            prior[c] = mean
    X = np.linalg.solve(G, As.T @ Y + lam * prior)
    full = np.zeros((len(stops) * 5, 3))
    full[keep] = X
    return [[[round(float(v), 2) for v in full[k * 5 + h]] for h in range(5)] for k in range(len(stops))], len(A)


def wipe(art, S, origin=(0, 0), inside=0.6, step=2.6):
    """art with the pixels under shape S (sd < inside) replaced by the art's own pixel found by walking straight out of S to
    `step` px past its edge (boxes.py's wipe for any shape): what lies around stays, nothing of the element is lent."""
    h, w = art.shape[:2]
    jj, ii = np.mgrid[0:h, 0:w]
    x, y = ii + 0.5 + origin[0], jj + 0.5 + origin[1]
    d = sdf.sd(S, x, y)
    gy, gx = np.gradient(d)
    n = np.maximum(np.hypot(gx, gy), 1e-6)
    k = np.clip(step - d, 0, None)
    tx = np.clip(np.rint(ii + gx / n * k).astype(int), 0, w - 1)
    ty = np.clip(np.rint(jj + gy / n * k).astype(int), 0, h - 1)
    out = art.copy()
    m = d < inside
    out[m] = art[ty[m], tx[m]]
    return out
