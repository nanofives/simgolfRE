"""P41 popups area: measured paints (my own, the toolkit's files are not edited).

bevel_from_bands(...)  a flat box's edge lines as sdf.py's bevel paint, solved exactly from the colour of each side's pixel rows
fit_bevel(...)         a lit surface (disc body, sphere) measured on the art by least squares (sdf.py's bevel paint)
"""
import numpy as np

import common as c
sdf = c.sdf


def bevel_from_bands(top, bottom, left, right, fill, depth0=0.5, crisp=True):
    """Bands are lists of colours from the outer pixel row inward (any length; padded with fill). Returns a bevel paint whose
    colour across pixel row k (depth k .. k + 1) is side[k] on each straight side (exact: four sides = the four even direction
    terms). crisp: each row is one flat colour with a 0.1 px step to the next (the art's lines are whole pixel rows; a plain
    linear blend between row centres draws them as a soft gradient). crisp=False: stops at the row centres depth0 + k."""
    n = max(len(top), len(bottom), len(left), len(right)) + 1
    pad = lambda b: [list(map(float, v)) for v in b] + [list(map(float, fill))] * (n - len(b))  # noqa: E731
    T, B, Lf, R = (np.array(pad(b)) for b in (top, bottom, left, right))
    coef, stops = [], []
    for k in range(n):
        c0 = (T[k] + B[k] + Lf[k] + R[k]) / 4
        c1 = (R[k] - Lf[k]) / 2
        c2 = (B[k] - T[k]) / 2
        c3 = ((Lf[k] + R[k]) - (T[k] + B[k])) / 4
        row = [[round(float(v), 2) for v in q] for q in (c0, c1, c2, c3, np.zeros(3))]
        if crisp:
            stops += [k + (0.05 if k else 0.0), k + 0.95]
            coef += [row, row]
        else:
            stops.append(depth0 + k)
            coef.append(row)
    return {"bevel": {"L": 1.0, "stops": stops, "coef": coef}}


def fit_linear(rgb, origin, keep, nstops=3, direction=None):
    """A linear gradient measured on the pixels `keep` of an art crop: the direction is the luminance gradient's (least squares
    plane) unless given; colours at nstops evenly spaced points between the extreme pixels along it, by least squares on the
    piecewise-linear hat functions. Returns the toolkit's linear paint."""
    H, W = rgb.shape[:2]
    jj, ii = np.mgrid[0:H, 0:W]
    x, y = (ii + 0.5 + origin[0])[keep], (jj + 0.5 + origin[1])[keep]
    C = rgb[keep]
    if direction is None:
        Lm = C @ np.array([0.299, 0.587, 0.114])
        A = np.c_[x, y, np.ones_like(x)]
        gx, gy, _ = np.linalg.lstsq(A, Lm, rcond=None)[0]
        n = np.hypot(gx, gy)
        direction = (gx / n, gy / n) if n > 1e-6 else (0.0, 1.0)
    t = x * direction[0] + y * direction[1]
    t0, t1 = float(t.min()), float(t.max())
    u = (t - t0) / max(t1 - t0, 1e-6)
    knots = np.linspace(0, 1, nstops)
    B = np.stack([np.clip(1 - np.abs(u - k) * (nstops - 1), 0, 1) for k in knots], axis=1)
    X = np.linalg.lstsq(B, C, rcond=None)[0]
    cx, cy = float(x.mean()), float(y.mean())
    tm = cx * direction[0] + cy * direction[1]
    a = [cx + (t0 - tm) * direction[0], cy + (t0 - tm) * direction[1]]
    b = [cx + (t1 - tm) * direction[0], cy + (t1 - tm) * direction[1]]
    return {"linear": {"a": [round(v, 3) for v in a], "b": [round(v, 3) for v in b],
                       "stops": [[float(k), [round(float(v), 1) for v in X[i]]] for i, k in enumerate(knots)]}}


def solve(rows, stops, lam=1e-3, flat_last=False):
    A = np.concatenate([np.stack([w for row in sdf.bevel_terms(u, nx, ny, stops) for w in row], axis=1) for u, nx, ny, _ in rows])
    if flat_last:       # the deepest stop is the disc's centre, where the direction is undefined: no direction terms there
        A[:, (len(stops) - 1) * 5 + 1:(len(stops) - 1) * 5 + 5] = 0.0
    Y = np.concatenate([rgb for _, _, _, rgb in rows])
    lam = lam * len(A)
    G = A.T @ A + lam * np.eye(A.shape[1])
    prior = np.zeros((A.shape[1], 3))
    prior[0::5] = Y.mean(0)
    X = np.linalg.solve(G, A.T @ Y + lam * prior)
    return [[[round(float(v), 2) for v in X[k * 5 + h]] for h in range(5)] for k in range(len(stops))], X, A, Y


def fit_bevel(shape, rgb, origin, stops, L, keep, radial_c=None, rounds=2, trim=22.0):
    """Measure a bevel paint for `shape` on an art crop rgb (H x W x 3, crop top-left at frame point origin). keep: H x W bool
    of the pixels that show the surface (not a glyph, not outside). radial_c: use radial normals about this point (a disc).
    Pixels that the fit misses by more than `trim` levels (a glyph's soft edge left in keep) are dropped and it is solved again."""
    H, W = rgb.shape[:2]
    jj, ii = np.mgrid[0:H, 0:W]
    x, y = ii + 0.5 + origin[0], jj + 0.5 + origin[1]
    d = sdf.sd(shape, x, y)
    if radial_c is not None:
        rr = np.maximum(np.hypot(x - radial_c[0], y - radial_c[1]), 1e-9)
        nx, ny = (x - radial_c[0]) / rr, (y - radial_c[1]) / rr
    else:
        nx, ny = sdf.normal(shape, x, y, d)
    m = keep & (d < -0.5)
    for _ in range(rounds):
        coef, X, A, Y = solve([((-d / L)[m], nx[m], ny[m], rgb[m])], stops, flat_last=radial_c is not None and stops[-1] >= 0.99)
        P = {"bevel": {"L": L, "stops": stops, "coef": coef, "of": shape}}
        pred = sdf.paint(P, x, y)
        res = np.abs(pred - rgb).max(axis=2)
        m2 = m & (res < trim)
        if m2.sum() == m.sum() or m2.sum() < 30:
            break
        m = m2
    return {"bevel": {"L": L, "stops": stops, "coef": coef}}, m
