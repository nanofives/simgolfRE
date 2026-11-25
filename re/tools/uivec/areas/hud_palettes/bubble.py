"""P41 hud_palettes: the HUD and palette bubble buttons as authored shapes with MEASURED paint.

A bubble cell is drawn as
  ground   the tray piece the cut carries around the bubble (authored polygons / rects), each with a linear paint measured
           on its own pixels (piecewise linear along the measured light direction), and the soft shadow the bubble casts on
           it (a blurred circle, offset and strength measured by descent);
  bubble   ONE circle (the outer edge of its navy rim) painted with sdf's bevel paint: colour as a function of the depth
           in pixels from the edge (fixed pixel stops, so the rim and the glass edge are as wide on a small bubble as on a
           big one) and of which way the edge faces. The coefficients are solved by least squares on the cut's own
           bubble pixels, glyph excluded: the navy rim, the light ring inside it, the body's gradient and its lit side
           all come out of the measurement;
  glyph    authored parts (shapes read from the enlarged art), each painted with its colour measured per state as the
           median of the art pixels well inside the part.
"""
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hp  # noqa: E402
from hp import fit, sdf  # noqa: E402

STOPS = [0.0, 1.0, 2.0, 3.0, 4.5, 6.5, 9.0, 12.5, 17.0, 23.0, 31.0, 42.0]


def stops_for(r):
    s = [t for t in STOPS if t < r - 0.5]
    return s + [max(r - 0.5, s[-1] + 0.5)]


def grid(x0, y0, w, h):
    jj, ii = np.mgrid[y0:y0 + h, x0:x0 + w]
    return ii + 0.5, jj + 0.5


def U(*s):
    return {"t": "union", "of": list(s)}


def bilinear(L, x, y):
    """L sampled at frame points (x, y) of a crop whose pixel (i, j) centre is (i + 0.5, j + 0.5)."""
    h, w = L.shape
    fx, fy = np.clip(x - 0.5, 0, w - 1.001), np.clip(y - 0.5, 0, h - 1.001)
    i, j = np.floor(fx).astype(int), np.floor(fy).astype(int)
    a, b = fx - i, fy - j
    return (L[j, i] * (1 - a) * (1 - b) + L[j, i + 1] * a * (1 - b) + L[j + 1, i] * (1 - a) * b + L[j + 1, i + 1] * a * b)


def fit_rim(rgb, mask, x0, y0, dark=70.0, guess=None, trim=0.6, arc=(0.0, 360.0)):
    """The circle of the bubble's outer edge. Along rays from the hand-read guess (cx, cy, r), the outermost point where the
    luminance falls halfway from the ground's to the rim's; rays only over `arc` (screen degrees, 0 = right, 90 = down):
    by default the lit upper-left side, away from the cast shadow and the dark halo at the lower right."""
    L = np.pad(fit.lum(rgb.astype(float)), 8, mode="edge")
    M = np.pad(mask.astype(float), 8)
    x0, y0 = x0 - 8, y0 - 8
    cx, cy, r = guess
    pts = []
    for a in np.deg2rad(np.arange(arc[0], arc[1], 2.0)):
        ts = np.arange(r + 4.0, r - 4.0, -0.1)
        px, py = cx + ts * np.cos(a) - x0, cy + ts * np.sin(a) - y0
        Mv = bilinear(M, px, py)
        v = bilinear(L, px, py) * Mv + 200.0 * (1 - Mv)      # outside the cut reads as light
        rim = float(v.min())
        if rim > dark:
            continue
        kd = int(np.argmax(v < rim + 30))
        gs = v[:kd][Mv[:kd] > 0.999]
        g = float(np.median(gs)) if len(gs) >= 2 else 200.0
        if g - rim < 25:
            continue
        thr = (g + rim) / 2
        k = int(np.argmax(v < thr))
        if k == 0:
            continue
        t = ts[k - 1] + (ts[k] - ts[k - 1]) * (v[k - 1] - thr) / max(v[k - 1] - v[k], 1e-9)
        pts.append((cx + t * np.cos(a), cy + t * np.sin(a)))
    c = fit.fit_circle(np.array(pts), trim=trim)
    return c


def measure_bevel(rgb, keep, x0, y0, circle, stops=None, kdir=1.0):
    """Bevel coefficients (stops x 5 x 3) of a circle's paint, least squares over the pixels in keep (bool array)."""
    cx, cy, r = circle
    stops = stops or stops_for(r)
    h, w = keep.shape
    # supersample: each art pixel is the mean of the paint over it, so fit with 3 x 3 subsamples averaged
    ss = 3
    rows_A, rows_Y = [], []
    jj, ii = np.nonzero(keep)
    Y = rgb[jj, ii].astype(float)
    A = 0
    for sj in range(ss):
        for si in range(ss):
            px = x0 + ii + (si + 0.5) / ss
            py = y0 + jj + (sj + 0.5) / ss
            rr = np.maximum(np.hypot(px - cx, py - cy), 1e-9)
            u = r - rr
            T = sdf.bevel_terms(u, (px - cx) / rr, (py - cy) / rr, stops)
            A = A + np.stack([wv for row in T for wv in row], axis=1) / (ss * ss)
    X = ridge(A, Y, stops, r, kdir=kdir)
    coef = [[[round(float(v), 2) for v in X[k * 5 + hh]] for hh in range(5)] for k in range(len(stops))]
    return {"bevel": {"L": 1.0, "stops": [round(s, 2) for s in stops], "coef": coef}}


def ridge(A, Y, stops, r=None, kdir=1.0):
    """Ridge least squares for bevel coefficients: the plain colour of a stop is pulled lightly toward the mean colour,
    the direction terms harder (they have little support where the pixels are few), and hardest deep inside a circle,
    where the nearest edge's direction stops meaning anything (it made a pinwheel at the centre of small bubbles)."""
    n = len(A)
    lam = np.zeros(A.shape[1])
    for k, t in enumerate(stops):
        lam[k * 5] = 1e-3 * n
        deep = 1.0 if r is None else 1.0 + 30.0 * max(0.0, t / r - 0.5) ** 2
        lam[k * 5 + 1:k * 5 + 3] = 2e-2 * n * deep * kdir
        lam[k * 5 + 3:k * 5 + 5] = 6e-2 * n * deep * kdir
    G = A.T @ A + np.diag(lam)
    prior = np.zeros((A.shape[1], 3))
    prior[0::5] = Y.mean(0)
    return np.linalg.solve(G, A.T @ Y + lam[:, None] * prior)


def inside_px(shape, x0, y0, h, w, depth=0.0):
    x, y = grid(x0, y0, w, h)
    return sdf.sd(shape, x, y) < -depth


def measure_linear(rgb, keep, x0, y0, n=3):
    """A linear paint for a flat region: direction of the luminance gradient (least squares plane), colour piecewise
    linear along it with n stops (least squares)."""
    jj, ii = np.nonzero(keep)
    Y = rgb[jj, ii].astype(float)
    px, py = x0 + ii + 0.5, y0 + jj + 0.5
    L = fit.lum(Y)
    A = np.c_[px - px.mean(), py - py.mean(), np.ones(len(px))]
    g = np.linalg.lstsq(A, L, rcond=None)[0]
    gx, gy = g[0], g[1]
    nrm = np.hypot(gx, gy)
    if nrm < 1e-6:
        gx, gy = 0.0, 1.0
    else:
        gx, gy = gx / nrm, gy / nrm
    t = px * gx + py * gy
    t0, t1 = float(t.min()), float(t.max())
    if t1 - t0 < 1e-6:
        c = [round(float(v), 1) for v in np.median(Y, 0)]
        return c
    a = [t0 * gx, t0 * gy]
    b = [t1 * gx, t1 * gy]
    u = (t - t0) / (t1 - t0)
    knots = np.linspace(0, 1, n)
    H = np.zeros((len(u), n))
    for k in range(n):
        if k > 0:
            m = (u >= knots[k - 1]) & (u <= knots[k])
            H[m, k] = (u[m] - knots[k - 1]) / (knots[k] - knots[k - 1])
        if k < n - 1:
            m = (u >= knots[k]) & (u <= knots[k + 1])
            H[m, k] = (knots[k + 1] - u[m]) / (knots[k + 1] - knots[k])
    C = np.linalg.lstsq(H, Y, rcond=None)[0]
    return {"linear": {"a": [round(v, 2) for v in a], "b": [round(v, 2) for v in b],
                       "stops": [[round(float(k), 3), [round(float(v), 1) for v in C[i]]] for i, k in enumerate(knots)]}}


def part_colour(rgb, mask, x0, y0, shape, depth=0.6, fallback=None, pick="median"):
    """A part's flat colour: the median of the art pixels inside it; pick "dark" / "light" takes the median of the darkest /
    lightest 40 % only (a thin navy line's pixels are partly covered, its full colour is in its darkest pixels)."""
    h, w = mask.shape
    m = inside_px(shape, x0, y0, h, w, depth) & mask
    if m.sum() < 1:
        m = inside_px(shape, x0, y0, h, w, 0.0) & mask
    if m.sum() < 1:
        return fallback
    v = rgb[m].astype(float)
    if pick in ("dark", "light") and len(v) >= 3:
        L = fit.lum(v)
        o = np.argsort(L)
        n = max(1, int(round(len(v) * 0.4)))
        v = v[o[:n]] if pick == "dark" else v[o[-n:]]
    return [round(float(c), 1) for c in np.median(v, 0)]


def fit_shadow(rgb, keep, x0, y0, base_scene, circle, init=(0.15, 0.25, 1.0, 0.25, 0.35), rgbsh=(30, 30, 80)):
    """The bubble's cast shadow on the ground: a soft circle (dx, dy in radii, radius scale, softness in radii, alpha),
    by coordinate descent on the ground pixels in keep. base_scene: the ground layers without the shadow."""
    cx, cy, r = circle
    h, w = keep.shape
    z = 3
    base = hp.render_state(base_scene, x0, y0, w, h, z)

    def layer(p):
        return {"shape": {"t": "circle", "c": [cx + p[0] * r, cy + p[1] * r], "r": p[2] * r}, "fill": list(rgbsh),
                "alpha": p[4], "soft": max(p[3] * r, 0.3)}

    def loss(p):
        img = base.copy()
        sdf.render([{"box": [x0, y0, x0 + w, y0 + h], "layers": [layer(p)]}], img, z, origin=(x0, y0))
        img = img.reshape(h, z, w, z, 3).mean((1, 3))
        return float(np.abs(img - rgb)[keep].mean())

    p = list(init)
    steps = [0.2, 0.2, 0.2, 0.2, 0.2]
    best = loss(p)
    for _ in range(8):
        for k in range(5):
            for sgn in (1, -1):
                q = list(p)
                q[k] += sgn * steps[k]
                if k == 4:
                    q[k] = min(max(q[k], 0.0), 1.0)
                if k in (2, 3):
                    q[k] = max(q[k], 0.05)
                lq = loss(q)
                if lq < best:
                    best, p = lq, q
        steps = [s * 0.5 for s in steps]
    return layer(p), best


BODY_STOPS = [0.0, 0.9, 2.0, 3.5, 5.5, 8.0, 11.5, 16.0, 22.0, 30.0, 40.0]


def body_stops(r):
    s = [t for t in BODY_STOPS if t < r - 0.5]
    return s + [max(r - 0.5, s[-1] + 0.5)]


def measure_rim_body(rgb, mask, x0, y0, circle, excl, widths=(1.0, 1.25, 1.5, 1.75, 2.0, 2.25, 2.5, 3.0), kdir=1.0):
    """The bubble as a navy RIM (a circle of the outer radius, flat colour) under a BODY (the circle rw inside it, painted with
    the bevel paint measured on its own pixels). rw is measured: the width whose two layers reproduce the cut's pixels best.
    Returns (rim layer, body layer, rw, loss)."""
    cx, cy, R = circle
    h, w = mask.shape
    X, Y = grid(x0, y0, w, h)
    dc = np.hypot(X - cx, Y - cy) - R
    best = None
    z = 3
    for rw in widths:
        rb = R - rw
        keep = mask & (dc < -rw - 0.45) & ~excl
        paint = measure_bevel(rgb, keep, x0, y0, (cx, cy, rb), stops=body_stops(rb), kdir=kdir)
        st_, cf_ = paint["bevel"]["stops"], paint["bevel"]["coef"]
        if len(st_) > 2 and st_[-1] - st_[-2] < 1.0:
            # fix round: the last stop is a disc under 1 px at the very centre, mostly under the glyph, so it was fitted on
            # almost nothing and left a dark dot where the glyph leaves the centre open (bridge tab): it takes its neighbour's paint
            cf_[-1] = [list(v) for v in cf_[-2]]
        ring = mask & (dc < -0.35) & (dc > -rw + 0.35)
        if ring.sum() < 3:
            ring = mask & (dc < 0) & (dc > -rw)
        v = rgb[ring].astype(float)
        o = np.argsort(fit.lum(v))
        rimc = [round(float(c), 1) for c in np.median(v[o[:max(1, int(len(v) * 0.5))]], 0)]
        rim = {"shape": {"t": "circle", "c": [cx, cy], "r": R}, "fill": rimc}
        body = {"shape": {"t": "circle", "c": [cx, cy], "r": rb}, "fill": paint}
        img = np.zeros((h * z, w * z, 3)) + 128
        sdf.render([{"box": [x0, y0, x0 + w, y0 + h], "layers": [rim, body]}], img, z, origin=(x0, y0))
        img = img.reshape(h, z, w, z, 3).mean((1, 3))
        sel = mask & (dc < -0.6) & ~excl
        loss = float(np.abs(img - rgb)[sel].mean())
        if best is None or loss < best[3]:
            best = (rim, body, rw, loss)
    return best


def measure_bevel_shape(rgb, keep, x0, y0, shape, stops, L=1.0):
    """Bevel coefficients of any shape's paint (depth from its edge in pixels, the nearest edge's normal), least squares over
    the pixels in keep, 3 x 3 subsamples per pixel."""
    jj, ii = np.nonzero(keep)
    Y = rgb[jj, ii].astype(float)
    ss = 3
    A = 0
    for sj in range(ss):
        for si in range(ss):
            px = (x0 + ii + (si + 0.5) / ss).astype(float)
            py = (y0 + jj + (sj + 0.5) / ss).astype(float)
            d = sdf.sd(shape, px, py)
            if shape["t"] in ("circle", "ellipse"):
                nx, ny = sdf.normal(shape, px, py, d)
            else:
                e = 0.05
                gx = (sdf.sd(shape, px + e, py) - sdf.sd(shape, px - e, py)) / (2 * e)
                gy = (sdf.sd(shape, px, py + e) - sdf.sd(shape, px, py - e)) / (2 * e)
                n = np.maximum(np.hypot(gx, gy), 1e-9)
                nx, ny = gx / n, gy / n
            T = sdf.bevel_terms(-d / L, nx, ny, stops)
            A = A + np.stack([wv for row in T for wv in row], axis=1) / (ss * ss)
    X = ridge(A, Y, stops, kdir=0.05)      # a ground's lighting is mostly its direction terms: a light ridge on them
    coef = [[[round(float(v), 2) for v in X[k * 5 + hh]] for hh in range(5)] for k in range(len(stops))]
    return {"bevel": {"L": L, "stops": list(stops), "coef": coef}}
