"""uivec: measuring shapes on the player's art (P41). Everything here returns NUMBERS (a circle, an ellipse, a glyph's outline as
points); the scenes built from them hold no art pixel.
"""
import os

import numpy as np
from PIL import Image

ROOT = os.path.normpath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
ORIG = os.path.join(ROOT, "original")


def load_sheet(rel):
    """(index array H x W uint8, palette 256 x 3 uint8) of a PCX under original\\."""
    im = Image.open(os.path.join(ORIG, rel))
    pal = np.array(im.getpalette()[:768], np.uint8).reshape(256, 3)
    return np.asarray(im), pal


def lum(rgb):
    rgb = np.asarray(rgb, float)
    return rgb[..., 0] * 0.299 + rgb[..., 1] * 0.587 + rgb[..., 2] * 0.114


# ------------------------------------------------------------------------------------------------------------ primitives
def fit_circle(pts, trim=1.0, rounds=4):
    """Least-squares circle through points (N x 2), dropping points farther than `trim` from the fit."""
    p = np.asarray(pts, float)
    for _ in range(rounds):
        A = np.c_[2 * p[:, 0], 2 * p[:, 1], np.ones(len(p))]
        b = (p ** 2).sum(1)
        cx, cy, c = np.linalg.lstsq(A, b, rcond=None)[0]
        r = np.sqrt(c + cx * cx + cy * cy)
        res = np.abs(np.hypot(p[:, 0] - cx, p[:, 1] - cy) - r)
        keep = res <= trim
        if keep.all() or keep.sum() < 8:
            break
        p = p[keep]
    return float(cx), float(cy), float(r), float(res.mean())


def fit_ellipse(pts, rot=None):
    """Least-squares ellipse. rot None: a general conic (centre, radii, rotation in degrees). rot given: axes fixed at that
    rotation (4 unknowns), for arcs that do not close."""
    p = np.asarray(pts, float)
    m = p.mean(0)
    q = p - m
    if rot is None:
        A = np.c_[q[:, 0] ** 2, q[:, 0] * q[:, 1], q[:, 1] ** 2, q[:, 0], q[:, 1]]
        a, b, c, d, e = np.linalg.lstsq(A, np.ones(len(q)), rcond=None)[0]
        M = np.array([[a, b / 2], [b / 2, c]])
        cen = -0.5 * np.linalg.solve(M, np.array([d, e]))
        k = 1 + cen @ M @ cen
        w, v = np.linalg.eigh(M / k)
        radii = 1 / np.sqrt(w)
        i = int(np.argmax(radii))
        ang = np.degrees(np.arctan2(v[1, i], v[0, i]))
        ang = (ang + 90) % 180 - 90
        return float(cen[0] + m[0]), float(cen[1] + m[1]), float(radii[i]), float(radii[1 - i]), float(ang)
    t = np.deg2rad(rot)
    u = q[:, 0] * np.cos(t) + q[:, 1] * np.sin(t)
    v = -q[:, 0] * np.sin(t) + q[:, 1] * np.cos(t)
    A = np.c_[u * u, v * v, u, v]
    a, c, d, e = np.linalg.lstsq(A, np.ones(len(q)), rcond=None)[0]
    cu, cv = -d / (2 * a), -e / (2 * c)
    k = 1 + a * cu * cu + c * cv * cv
    cx = cu * np.cos(t) - cv * np.sin(t) + m[0]
    cy = cu * np.sin(t) + cv * np.cos(t) + m[1]
    return float(cx), float(cy), float(np.sqrt(k / a)), float(np.sqrt(k / c)), float(rot)


def edge_points(mask):
    """Midpoints of the pixel edges between mask and not-mask (frame coordinates: pixel (i, j) covers [i, i+1) x [j, j+1))."""
    m = np.pad(mask.astype(bool), 1)
    pts = []
    j, i = np.nonzero(m[1:-1, 1:-1] & ~m[1:-1, :-2]); pts.append(np.c_[i, j + 0.5])          # left edges
    j, i = np.nonzero(m[1:-1, 1:-1] & ~m[1:-1, 2:]); pts.append(np.c_[i + 1.0, j + 0.5])     # right
    j, i = np.nonzero(m[1:-1, 1:-1] & ~m[:-2, 1:-1]); pts.append(np.c_[i + 0.5, j])          # top
    j, i = np.nonzero(m[1:-1, 1:-1] & ~m[2:, 1:-1]); pts.append(np.c_[i + 0.5, j + 1.0])     # bottom
    return np.concatenate(pts).astype(float)


def outermost(mask, centre, rays=180):
    """The farthest mask pixel centre along each ray from `centre` (N x 2)."""
    j, i = np.nonzero(mask)
    x, y = i + 0.5, j + 0.5
    ang = np.arctan2(y - centre[1], x - centre[0])
    rad = np.hypot(x - centre[0], y - centre[1])
    b = ((ang + np.pi) / (2 * np.pi) * rays).astype(int) % rays
    out = []
    for k in range(rays):
        m = b == k
        if m.any():
            n = np.argmax(np.where(m, rad, -1))
            out.append((x[n], y[n]))
    return np.array(out)


# ------------------------------------------------------------------------------------------------------------ outlines
_CASES = {1: ["lb"], 2: ["br"], 3: ["lr"], 4: ["tr"], 6: ["tb"], 7: ["lt"], 8: ["lt"], 9: ["tb"], 11: ["tr"], 12: ["lr"],
          13: ["br"], 14: ["lb"]}


def contours(cov, iso=0.5):
    """Closed iso-lines of a coverage grid (value at pixel centres), as lists of (x, y) in pixel coordinates of that grid
    (pixel (i, j) centre = (i + 0.5, j + 0.5))."""
    c = np.pad(np.asarray(cov, float), 1)
    H, W = c.shape
    pos, adj = {}, {}

    def edge(kind, j, i):
        key = (kind, j, i)
        if key not in pos:
            if kind == "h":   # between (j, i) and (j, i + 1)
                a, b = c[j, i], c[j, i + 1]
                pos[key] = (i + (iso - a) / (b - a), float(j))
            else:             # between (j, i) and (j + 1, i)
                a, b = c[j, i], c[j + 1, i]
                pos[key] = (float(i), j + (iso - a) / (b - a))
        return key

    for j in range(H - 1):
        row0, row1 = c[j] >= iso, c[j + 1] >= iso
        for i in np.nonzero((row0[:-1] != row0[1:]) | (row1[:-1] != row1[1:]) | (row0[:-1] != row1[:-1]))[0]:
            tl, tr, br, bl = row0[i], row0[i + 1], row1[i + 1], row1[i]
            code = tl * 8 + tr * 4 + br * 2 + bl * 1
            if code in (0, 15):
                continue
            if code in (5, 10):
                mid = (c[j, i] + c[j, i + 1] + c[j + 1, i] + c[j + 1, i + 1]) / 4 >= iso
                # saddle: 5 = top-right and bottom-left inside, 10 = top-left and bottom-right inside; the cell's middle
                # says whether the two inside corners join (the lines then cut off the two outside corners)
                if code == 5:
                    segs = ["lt", "br"] if mid else ["tr", "lb"]
                else:
                    segs = ["tr", "lb"] if mid else ["lt", "br"]
            else:
                segs = _CASES[code]
            E = {"t": ("h", j, i), "b": ("h", j + 1, i), "l": ("v", j, i), "r": ("v", j, i + 1)}
            for sg in segs:
                a, b = edge(*E[sg[0]]), edge(*E[sg[1]])
                adj.setdefault(a, []).append(b)
                adj.setdefault(b, []).append(a)
    loops, seen = [], set()
    for start in adj:
        if start in seen:
            continue
        loop, prev, cur = [], None, start
        while cur not in seen:
            seen.add(cur)
            loop.append(pos[cur])
            nxt = [n for n in adj[cur] if n != prev]
            if not nxt:
                break
            prev, cur = cur, nxt[0]
        if len(loop) >= 4:
            # grid node (i, j) of the padded array is the pixel centre (i - 1 + 0.5, j - 1 + 0.5)
            loops.append([(x - 0.5, y - 0.5) for x, y in loop])
    return loops


def _dp(pts, eps):
    """Douglas-Peucker on an open polyline."""
    if len(pts) < 3:
        return pts
    a, b = np.array(pts[0]), np.array(pts[-1])
    p = np.array(pts)
    ab = b - a
    L = np.hypot(*ab)
    d = np.abs(ab[0] * (p[:, 1] - a[1]) - ab[1] * (p[:, 0] - a[0])) / L if L > 1e-9 else np.hypot(*(p - a).T)
    k = int(np.argmax(d))
    if d[k] <= eps:
        return [pts[0], pts[-1]]
    return _dp(pts[:k + 1], eps)[:-1] + _dp(pts[k:], eps)


def simplify(loop, eps=0.2):
    p = np.array(loop)
    k = int(np.argmax(np.hypot(*(p - p.mean(0)).T)))
    loop = loop[k:] + loop[:k]
    far = int(np.argmax(np.hypot(*(np.array(loop) - np.array(loop[0])).T)))
    return _dp(loop[:far + 1], eps)[:-1] + _dp(loop[far:] + [loop[0]], eps)[:-1]


def smooth(loop, corner_deg=55.0, rounds=2):
    """Corner cutting that keeps real corners: a vertex turning more than corner_deg stays, the others are cut (Chaikin)."""
    pts = [tuple(p) for p in loop]
    keep = [False] * len(pts)
    for _ in range(rounds):
        n = len(pts)
        if n < 4:
            break
        turn = []
        for i in range(n):
            a, b, c = np.array(pts[i - 1]), np.array(pts[i]), np.array(pts[(i + 1) % n])
            u, v = b - a, c - b
            cr = u[0] * v[1] - u[1] * v[0]
            turn.append(abs(np.degrees(np.arctan2(cr, u @ v))))
        if not any(keep):
            keep = [t > corner_deg for t in turn]
        out, kout = [], []
        for i in range(n):
            a, b, c = pts[i - 1], pts[i], pts[(i + 1) % n]
            if keep[i]:
                out.append(b); kout.append(True)
            else:
                out.append((a[0] * 0.25 + b[0] * 0.75, a[1] * 0.25 + b[1] * 0.75)); kout.append(False)
                out.append((b[0] * 0.75 + c[0] * 0.25, b[1] * 0.75 + c[1] * 0.25)); kout.append(False)
        pts, keep = out, kout
    return pts


def dark_coverage(rgb, inside, navy_l=None):
    """How much of each pixel a dark glyph covers, 0..1: the pixel's luminance between the glyph's and the local ground's (the
    mean of the not-dark pixels around it). `inside`: the pixels to look at (the button's body)."""
    L = lum(rgb)
    v = L[inside]
    lo = float(np.percentile(v, 4)) if navy_l is None else navy_l
    hi = float(np.median(v[v > (lo + v.max()) / 2])) if (v > (lo + v.max()) / 2).any() else float(v.max())
    mid = (lo + hi) / 2
    light = inside & (L > mid + (hi - mid) * 0.35)
    H, W = L.shape
    acc, cnt = np.zeros_like(L), np.zeros_like(L)
    R = 4
    Lp, lp = np.pad(L * light, R), np.pad(light.astype(float), R)
    for dj in range(-R, R + 1):
        for di in range(-R, R + 1):
            acc += Lp[R + dj:R + dj + H, R + di:R + di + W]
            cnt += lp[R + dj:R + dj + H, R + di:R + di + W]
    ground = np.where(cnt >= 3, acc / np.maximum(cnt, 1), hi)
    cov = np.clip((ground - L) / np.maximum(ground - lo, 1.0), 0, 1)
    return np.where(inside, cov, 0.0), lo, hi


def glyph_rings(rgb, inside, origin=(0.0, 0.0), eps=0.16, min_area=1.5):
    """The outline rings of the dark glyph in an RGB crop, in frame coordinates (origin = the crop's top-left)."""
    cov, lo, hi = dark_coverage(rgb, inside)
    rings = []
    for loop in contours(cov):
        p = np.array(loop)
        area = 0.5 * abs(np.dot(p[:, 0], np.roll(p[:, 1], -1)) - np.dot(p[:, 1], np.roll(p[:, 0], -1)))
        if area < min_area:
            continue
        q = smooth(simplify(loop, eps))
        rings.append([[round(x + origin[0], 2), round(y + origin[1], 2)] for x, y in q])
    return rings, cov


def largest_inner_component(mask, min_px=200):
    """The largest 4-connected component of `mask` that does not touch the array's border (None when there is none)."""
    m = mask.astype(bool)
    H, W = m.shape
    lab = np.zeros((H, W), np.int32)
    best, best_n, cur = 0, 0, 0
    for j0, i0 in zip(*np.nonzero(m)):
        if lab[j0, i0]:
            continue
        cur += 1
        stack, n, border = [(j0, i0)], 0, False
        lab[j0, i0] = cur
        while stack:
            j, i = stack.pop()
            n += 1
            if j == 0 or i == 0 or j == H - 1 or i == W - 1:
                border = True
            for dj, di in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                a, b = j + dj, i + di
                if 0 <= a < H and 0 <= b < W and m[a, b] and not lab[a, b]:
                    lab[a, b] = cur
                    stack.append((a, b))
        if not border and n > best_n:
            best, best_n = cur, n
    return (lab == best) if best_n >= min_px else None
