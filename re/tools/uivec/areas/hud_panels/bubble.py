"""P41 hud_panels: the glossy bubble buttons of the four HUD panels (tan, yellow, dark, blue), measured per cell.

A bubble is: a cast shadow on whatever it sits on (a soft disc offset to the lower right), a navy rim (circle), the body
inside it (circle, lit surface = sdf bevel paint) and the glyph. The rim and the body are MEASURED per cell (circle fits);
the body paint is measured per paint family (tan idle, tan hover, yellow, dark, blue) by least squares over every cell
of that family at once (accept.solve_bevel), the glyph's own pixels left out by trimming the worst residuals.
"""
import json
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import common as C  # noqa: E402
sys.path.insert(0, C.UIVEC)
import accept  # noqa: E402
import fit  # noqa: E402
import recipes  # noqa: E402
import sdf  # noqa: E402
import title  # noqa: E402

STOPS = [0.0, 0.06, 0.14, 0.26, 0.45, 0.7, 1.0]   # fewer than recipes.BUBBLE_STOPS: these bodies are 11-18 px
PAINT_FILE = os.path.join(HERE, "bubble_paint.json")


def navy(a):
    L = fit.lum(a)
    return (L < 80) & (L > 12) & (a[..., 2] > a[..., 0] + 22)


def measure_rim(key, rect, guess=None):
    """The rim's outer circle: outermost navy pixels from the centre, fitted with trimming, rays that leave the circle
    (a strip, a shadow) dropped. guess: (cx, cy, r) to restrict the search."""
    rgb, m = C.sheet(key)
    x, y, w, h = rect
    a = rgb[y:y + h, x:x + w]
    dk = navy(a) & m[y:y + h, x:x + w]
    jj, ii = np.mgrid[0:h, 0:w]
    if guess is not None:
        dk &= np.hypot(ii + 0.5 + x - guess[0], jj + 0.5 + y - guess[1]) < guess[2] + 2.5
        c0 = (guess[0] - x, guess[1] - y)
    else:
        j, i = np.nonzero(dk)
        c0 = (i.mean() + 0.5, j.mean() + 0.5)
    r = None
    for _ in range(4):
        pts = fit.outermost(dk, c0, rays=120)
        rad = np.hypot(pts[:, 0] - c0[0], pts[:, 1] - c0[1])
        med = np.median(rad)
        pts = pts[np.abs(rad - med) < 1.6]
        cx, cy, r, res = fit.fit_circle(pts, trim=0.7)
        c0 = (cx, cy)
    return [round(cx + x, 2), round(cy + y, 2), round(r + 0.15, 2), round(res, 2)]


def measure_body(key, c):
    rgb, _ = C.sheet(key)
    return title.measure_body(rgb, c[0], c[1], c[2])


def body_rows(key, c, body, trim_glyph=None):
    """(u, nx, ny, rgb) of the body's pixels, glyph excluded by `trim_glyph` (a shape: pixels within 1.3 of it left out)."""
    rgb, _ = C.sheet(key)
    bx, by, br = body
    x0, y0, x1, y1 = int(bx - br), int(by - br), int(np.ceil(bx + br)), int(np.ceil(by + br))
    jj, ii = np.mgrid[y0:y1, x0:x1]
    x, y = ii + 0.5, jj + 0.5
    d = np.hypot(x - bx, y - by) - br
    mask = d < -0.6
    if trim_glyph is not None:
        mask &= sdf.sd(trim_glyph, x, y) > 1.3
    rr = np.maximum(np.hypot(x - bx, y - by), 1e-9)
    return (-d / br)[mask], ((x - bx) / rr)[mask], ((y - by) / rr)[mask], rgb[y0:y1, x0:x1][mask]


def solve_family(rows, rounds=3, keep=0.9):
    """Bevel coefficients over pooled rows; each round drops the pixels with the largest residual (the glyph)."""
    cur = rows
    for _ in range(rounds):
        coef, n = accept.solve_bevel(cur, STOPS)
        nxt = []
        for u, nx, ny, rgb in cur:
            pred = np.zeros(rgb.shape)
            for k, row in enumerate(sdf.bevel_terms(u, nx, ny, STOPS)):
                for hh, wgt in enumerate(row):
                    pred += wgt[:, None] * np.asarray(coef[k][hh], float)
            res = np.abs(pred - rgb).max(axis=1)
            lim = max(np.quantile(res, keep), 14.0)
            k_ = res <= lim
            nxt.append((u[k_], nx[k_], ny[k_], rgb[k_]))
        cur = nxt
    coef, n = accept.solve_bevel(cur, STOPS)
    return coef


def rim_colour(key, c, body):
    rgb, _ = C.sheet(key)
    cx, cy, r = c[:3]
    x0, y0, x1, y1 = int(cx - r), int(cy - r), int(np.ceil(cx + r)), int(np.ceil(cy + r))
    jj, ii = np.mgrid[y0:y1, x0:x1]
    x, y = ii + 0.5, jj + 0.5
    ring = (np.hypot(x - body[0], y - body[1]) > body[2] + 0.6) & (np.hypot(x - cx, y - cy) < r - 0.6)
    a = rgb[y0:y1, x0:x1][ring]
    return [round(float(v), 1) for v in np.median(a[navy(a[None])[0]], axis=0)] if ring.any() else [33, 41, 80]


def item(c, body, paint, glyph=(), shadow=None, ground=None, extra_box=0):
    """The bubble as one scene item. glyph: list of layers (shape + fill) drawn clipped to the body.
    shadow: (dx, dy, grow, soft, alpha) in pixels, clipped to `ground` (a shape) when given."""
    cx, cy, r = c[:3]
    bx, by, br = body
    bc = {"t": "circle", "c": [bx, by], "r": br}
    L = []
    if shadow is not None:
        dx, dy, g, soft, al = shadow
        lay = {"shape": {"t": "circle", "c": [cx + dx, cy + dy], "r": r + g}, "fill": [40, 38, 92], "alpha": al,
               "soft": max(soft, 0.3)}
        if ground is not None:
            lay["clip"] = ground
        L.append(lay)
    L.append({"shape": {"t": "circle", "c": [cx, cy], "r": r}, "fill": paint["rim"]})
    L.append({"shape": bc, "fill": {"bevel": {"L": br, "stops": STOPS, "coef": paint["shade"]}}})
    for g in glyph:
        g = dict(g)
        g.setdefault("clip", bc)
        L.append(g)
    m = r + 4 + extra_box
    return {"box": [cx - m, cy - m, cx + m + 3, cy + m + 3], "layers": L}


def load_paint():
    if os.path.exists(PAINT_FILE):
        with open(PAINT_FILE) as f:
            return json.load(f)
    return {}


def save_paint(p):
    with open(PAINT_FILE, "w") as f:
        json.dump(p, f)


SHADOW_RGB = np.array([40.0, 38.0, 92.0])


def fit_shadow(insts, ground_rgb, rows_fn=None):
    """The cast shadow (dx, dy, grow, soft, alpha) shared by a family, by grid search + refinement on the ground pixels
    around each rim. insts: list of (key, c, ground_ok) where ground_ok(x, y) -> bool mask of pixels that are plain ground
    in the art apart from the shadow (inside the ground shape, outside the rim)."""
    data = []
    for key, c, ok_fn in insts:
        rgb, m = C.sheet(key)
        cx, cy, r = c[:3]
        R = int(r * 1.6) + 3
        x0, y0 = max(int(cx) - R, 0), max(int(cy) - R, 0)
        x1, y1 = min(int(cx) + R, m.shape[1]), min(int(cy) + R, m.shape[0])
        jj, ii = np.mgrid[y0:y1, x0:x1]
        x, y = ii + 0.5, jj + 0.5
        ok = ok_fn(x, y) & m[y0:y1, x0:x1] & (np.hypot(x - cx, y - cy) > r + 0.8)
        gr_ = ground_rgb(x[ok], y[ok]) if callable(ground_rgb) else np.broadcast_to(np.asarray(ground_rgb, float), (ok.sum(), 3))
        data.append((x[ok], y[ok], rgb[y0:y1, x0:x1][ok], cx, cy, r, gr_))

    def loss(p):
        dx, dy, gr, s, al = p
        e = 0.0
        for x, y, a, cx, cy, r, g in data:
            d = np.hypot(x - cx - dx, y - cy - dy) - (r + gr)
            u = np.clip(0.5 - d / (2 * max(s, 0.3)), 0, 1)
            w = (u * u * (3 - 2 * u) * al)[:, None]
            e += (np.abs(g * (1 - w) + SHADOW_RGB * w - a)).sum()
        return e
    best = None
    for dx in (0.5, 1.0, 1.5, 2.0):
        for dy in (0.5, 1.0, 1.5, 2.0, 2.5):
            for gr in (-1.0, 0.0, 1.0):
                for s in (1.5, 2.5, 3.5):
                    for al in (0.4, 0.6, 0.8):
                        v = loss((dx, dy, gr, s, al))
                        if best is None or v < best[0]:
                            best = (v, [dx, dy, gr, s, al])
    p = best[1]
    steps = [0.25, 0.25, 0.25, 0.25, 0.05]
    cur = best[0]
    for _ in range(4):
        for i, st in enumerate(steps):
            for sgn in (1, -1):
                q = list(p)
                q[i] += sgn * st
                v = loss(q)
                if v < cur:
                    cur, p = v, q
    return [round(v, 2) for v in p]
