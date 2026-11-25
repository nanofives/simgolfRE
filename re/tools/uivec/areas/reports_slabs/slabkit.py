"""P41 area reports_slabs: the shared pieces of the report family (slab, title plate, pills, wires, bands) and the tools to
measure and render them.

Reading (Hole Stats, 2026-11-24, log/roadmap/P41/work/reports_slabs/_read/):
  * the SLAB is a raised lavender surface whose outline is a rectilinear polygon with rounded corners: a big quarter-ellipse
    shoulder top-left, small corners at the title tab, a round foot tab bottom-right for the OK bubble. It has no navy line
    outside; it is lit from the upper left (a 2-3 px light rim top / left) and darkens over ~20 px toward the right and
    the bottom. Its paint is a bevel (sdf.py) whose numbers are fitted on the art's slab pixels (lighting measured).
  * INK: every navy line (pill outlines, wires) is about 1.6 px with a soft dark copy one pixel below / right of it (the
    line's shadow on the slab).
  * PILLS are sunken capsules: navy line, a short inner shade under the top line, a flat fill (lilac or cream).
    OVALS are the same as ellipses. The TITLE PLATE is a sunken cream plate whose top edge is a shallow circular arch.
  * WIRES are navy polylines with rounded elbows ("trunk"), straight stubs between pills, and "(" brackets joining two
    pill ends (half circles).

New shape (registered in sdf.EXTRA): "reports_slabs.rpoly" {"verts": [[x, y, rx, ry], ...], "sag": {i: s}}: a polygon
whose vertex i is rounded by a fillet that reaches rx along the incoming edge and ry along the outgoing one (a quarter
ellipse when the edges are perpendicular, a quadratic curve otherwise; rx = ry = 0 is a sharp corner). "sag" makes the edge
from vertex i to i+1 a circular arc with that sagitta (pixels, positive = bulging to the left of the edge direction, which
is outward for a clockwise ring on screen).
"""
import json
import os
import sys

import numpy as np
from PIL import Image

import common as C

sdf = C.sdf
fit = C.fit


# sdf._normal needs two samples per axis; a scene item whose box meets a render box in a one-pixel strip would fail.
# Patched in this process only (sdf.py itself is shared and left alone).
_normal0 = sdf._normal


def _normal_safe(d):
    if d.ndim != 2 or min(d.shape) < 2:
        return np.zeros_like(d), np.zeros_like(d)
    return _normal0(d)


sdf._normal = _normal_safe


# ------------------------------------------------------------------------------------------------------------ rpoly shape
def _arc_pts(a, b, sag, step=2.0):
    a, b = np.asarray(a, float), np.asarray(b, float)
    ch = b - a
    L = float(np.hypot(*ch))
    if abs(sag) < 1e-6 or L < 1e-6:
        return [a.tolist()]
    R = (L * L / 4 + sag * sag) / (2 * abs(sag))
    left = np.array([ch[1], -ch[0]]) / L          # left of the direction on screen (y down): (dy, -dx)
    mid = (a + b) / 2
    cen = mid - left * np.sign(sag) * (R - abs(sag))
    a0 = np.arctan2(*(a - cen)[::-1])
    a1 = np.arctan2(*(b - cen)[::-1])
    da = (a1 - a0 + np.pi) % (2 * np.pi) - np.pi
    n = max(int(abs(da) * R / step), 2)
    return [(cen + R * np.array([np.cos(a0 + da * k / n), np.sin(a0 + da * k / n)])).tolist() for k in range(n)]


def rpoly_ring(S):
    key = json.dumps([S["verts"], S.get("sag", {})])
    if key in _RING:
        return _RING[key]
    V = [np.asarray(v[:2], float) for v in S["verts"]]
    n = len(V)
    sag = {int(k): float(v) for k, v in S.get("sag", {}).items()}
    # each edge as a polyline from vertex i (included) to vertex i+1 (excluded)
    edges = [_arc_pts(V[i], V[(i + 1) % n], sag.get(i, 0.0)) for i in range(n)]
    ring = []
    for i in range(n):
        rx, ry = (S["verts"][i][2], S["verts"][i][3]) if len(S["verts"][i]) > 2 else (0.0, 0.0)
        prev = edges[i - 1] + [V[i].tolist()]
        nxt = edges[i] + [V[(i + 1) % n].tolist()]
        if rx <= 0 and ry <= 0:
            ring.append(V[i].tolist())
        else:
            # tangent directions at the vertex: the last piece of the incoming polyline, the first of the outgoing one
            pin = np.asarray(prev[-2], float)
            pout = np.asarray(nxt[1], float)
            din = (V[i] - pin) / max(np.hypot(*(V[i] - pin)), 1e-9)
            dout = (pout - V[i]) / max(np.hypot(*(pout - V[i])), 1e-9)
            P1 = V[i] - din * rx
            P2 = V[i] + dout * ry
            m = max(int(max(rx, ry) * 1.2), 6)
            if abs(float(din @ dout)) < 0.02:
                cen = P1 + dout * ry
                for k in range(m + 1):
                    t = np.pi / 2 * k / m
                    ring.append((cen - dout * ry * np.cos(t) + din * rx * np.sin(t)).tolist())
            else:
                for k in range(m + 1):
                    t = k / m
                    ring.append(((1 - t) ** 2 * P1 + 2 * t * (1 - t) * V[i] + t * t * P2).tolist())
        # the rest of edge i, past the fillet of vertex i and before that of vertex i+1
        rx1 = S["verts"][(i + 1) % n][2] if len(S["verts"][(i + 1) % n]) > 2 else 0.0
        ry0 = ry if (rx > 0 or ry > 0) else 0.0
        pts = edges[i][1:]
        a, b = V[i], V[(i + 1) % n]
        Lab = float(np.hypot(*(b - a)))
        for p in pts:
            p = np.asarray(p)
            if np.hypot(*(p - a)) > ry0 + 0.5 and np.hypot(*(p - b)) > rx1 + 0.5:
                ring.append(p.tolist())
    _RING[key] = ring
    return ring


_RING = {}


DMAX = 48.0     # distances past this are clamped (the deepest bevel stop is 40 px)


def sd_ring_fast(ring, x, y, dmax=DMAX):
    """Signed distance to a closed ring on a regular grid (x, y from mgrid-like arrays): each edge only updates the grid
    cells within dmax of it, the inside comes from a polygon raster, and cells within two grid steps of an edge get the
    exact even-odd test. Same result as sdf's poly within dmax, much faster on big shapes with many short edges."""
    from PIL import Image as _I, ImageDraw as _D
    H, W = x.shape
    ox, oy = float(x[0, 0]), float(y[0, 0])
    st = float(x[0, 1] - x[0, 0]) if W > 1 else (float(y[1, 0] - y[0, 0]) if H > 1 else 1.0)
    R = np.asarray(ring, float)
    im = _I.new("L", (W, H), 0)
    _D.Draw(im).polygon([((p[0] - ox) / st, (p[1] - oy) / st) for p in R], fill=1)
    inside = np.asarray(im).astype(bool)
    d2 = np.full((H, W), dmax * dmax)
    n = len(R)
    pad = dmax / st
    for k in range(n):
        ax, ay = R[k]
        bx, by = R[(k + 1) % n]
        i0 = int(max(np.floor((min(ax, bx) - ox) / st - pad), 0))
        i1 = int(min(np.ceil((max(ax, bx) - ox) / st + pad) + 1, W))
        j0 = int(max(np.floor((min(ay, by) - oy) / st - pad), 0))
        j1 = int(min(np.ceil((max(ay, by) - oy) / st + pad) + 1, H))
        if i1 <= i0 or j1 <= j0:
            continue
        xs, ys = x[j0:j1, i0:i1], y[j0:j1, i0:i1]
        ex, ey = bx - ax, by - ay
        L = ex * ex + ey * ey
        wx, wy = xs - ax, ys - ay
        u = np.clip((wx * ex + wy * ey) / max(L, 1e-12), 0, 1)
        q = (wx - ex * u) ** 2 + (wy - ey * u) ** 2
        np.minimum(d2[j0:j1, i0:i1], q, out=d2[j0:j1, i0:i1])
    d = np.sqrt(d2)
    near = d < 2.5 * st
    if near.any():
        px, py = x[near], y[near]
        ins = np.zeros(px.shape, bool)
        for k in range(n):
            ax, ay = R[k]
            bx, by = R[(k + 1) % n]
            if ay == by:
                continue
            c = ((ay <= py) & (by > py)) | ((by <= py) & (ay > py))
            ins ^= c & (px < ax + (py - ay) * (bx - ax) / (by - ay))
        inside = inside.copy()
        inside[near] = ins
    return np.where(inside, -d, d)


def _sd_rpoly(S, x, y):
    if x.ndim == 2 and x.shape[0] > 1 and x.shape[1] > 1:
        return sd_ring_fast(rpoly_ring(S), x, y)
    return sdf.sd({"t": "poly", "rings": [rpoly_ring(S)]}, x, y)


sdf.EXTRA["reports_slabs.rpoly"] = _sd_rpoly


def bbox(S, pad=0.0):
    """Frame bounding box of a shape (enough for the shapes used here)."""
    t = S["t"]
    if t == "reports_slabs.rpoly":
        r = np.array(rpoly_ring(S))
        x0, y0 = r.min(0)
        x1, y1 = r.max(0)
    elif t == "rrect":
        (cx, cy), (hx, hy) = S["c"], S["half"]
        x0, y0, x1, y1 = cx - hx, cy - hy, cx + hx, cy + hy
    elif t == "ellipse":
        (cx, cy), (rx, ry) = S["c"], S["r"]
        x0, y0, x1, y1 = cx - rx, cy - ry, cx + rx, cy + ry
    elif t == "circle":
        (cx, cy), r = S["c"], S["r"]
        x0, y0, x1, y1 = cx - r, cy - r, cx + r, cy + r
    elif t == "stroke":
        p = np.array(S["pts"])
        x0, y0 = p.min(0) - S["w"]
        x1, y1 = p.max(0) + S["w"]
    elif t == "seg":
        p = np.array([S["a"], S["b"]])
        x0, y0 = p.min(0) - S["w"]
        x1, y1 = p.max(0) + S["w"]
    elif t == "half":
        x0, y0, x1, y1 = -1e4, -1e4, 1e4, 1e4
    elif t == "poly":
        p = np.array([q for ring in S["rings"] for q in ring], float)
        x0, y0 = p.min(0)
        x1, y1 = p.max(0)
    elif t == "arc":
        (cx, cy), r = S["c"], S["r"] + S["w"]
        x0, y0, x1, y1 = cx - r, cy - r, cx + r, cy + r
    elif t == "move":
        b = bbox(S["of"])
        return [b[0] + S["d"][0] - pad, b[1] + S["d"][1] - pad, b[2] + S["d"][0] + pad, b[3] + S["d"][1] + pad]
    elif t in ("union", "inter"):
        bs = [bbox(q) for q in S["of"]]
        if t == "union":
            x0, y0, x1, y1 = min(b[0] for b in bs), min(b[1] for b in bs), max(b[2] for b in bs), max(b[3] for b in bs)
        else:
            x0, y0, x1, y1 = max(b[0] for b in bs), max(b[1] for b in bs), min(b[2] for b in bs), min(b[3] for b in bs)
    elif t == "sub":
        return bbox(S["a"], pad)
    elif t == "outline":
        b = bbox(S["of"])
        x0, y0, x1, y1 = b[0] - S["w"], b[1] - S["w"], b[2] + S["w"], b[3] + S["w"]
    elif t == "grow":
        b = bbox(S["of"])
        x0, y0, x1, y1 = b[0] - S["d"], b[1] - S["d"], b[2] + S["d"], b[3] + S["d"]
    else:
        raise ValueError(t)
    return [float(x0 - pad), float(y0 - pad), float(x1 + pad), float(y1 + pad)]


# ------------------------------------------------------------------------------------------------------------ paints
def bevel(L, stops, coef, of=None):
    b = {"L": L, "stops": list(stops), "coef": coef}
    if of is not None:
        b["of"] = of
    return {"bevel": b}


def fit_bevel(shapes, art_rgb, select, stops, L=1.0, ridge=1e-3, dir_max=None, linear=False):
    """Least-squares bevel coefficients (K x 5 x 3) for a paint shared by `shapes`, from the art pixels where select(shape
    index, x, y, d) is True (pixel centres). dir_max: stops deeper than this keep only their constant term (no direction
    terms), so a flat interior shows no seams along the shape's medial axis. Returns (coef, rms)."""
    rows, cols = [], []
    for k, S in enumerate(shapes):
        x0, y0, x1, y1 = bbox(S, 2)
        i0, j0 = max(int(np.floor(x0)), 0), max(int(np.floor(y0)), 0)
        i1, j1 = min(int(np.ceil(x1)), art_rgb.shape[1]), min(int(np.ceil(y1)), art_rgb.shape[0])
        yy, xx = np.mgrid[j0:j1, i0:i1]
        x, y = xx + 0.5, yy + 0.5
        d = sdf.sd(S, x, y)
        nx, ny = sdf.normal(S, x, y, d)
        m = select(k, x, y, d)
        if not m.any():
            continue
        T = sdf.bevel_terms(-d / L, nx, ny, stops)
        F = np.stack([w[m] for row in T for w in row], axis=1)
        rows.append(F)
        cols.append(art_rgb[j0:j1, i0:i1][m])
    A = np.concatenate(rows)
    B = np.concatenate(cols)
    if dir_max is not None:
        for k, t in enumerate(stops):
            if t > dir_max:
                A[:, k * 5 + 1:k * 5 + 5] = 0.0
    if linear:        # light from one side only: no quadratic direction terms
        for k in range(len(stops)):
            A[:, k * 5 + 3:k * 5 + 5] = 0.0
    n = A.shape[1]
    AtA = A.T @ A + ridge * np.eye(n) * len(A)
    # ridge toward zero only on the direction terms; the constant terms are pulled toward the mean colour
    coef = np.linalg.solve(AtA, A.T @ B + ridge * len(A) * np.repeat(
        np.array([[1, 0, 0, 0, 0]] * len(stops)).reshape(-1)[:, None], 3, 1) * B.mean(0))
    res = A @ coef - B
    K = len(stops)
    coef = coef.reshape(K, 5, 3)
    return [[[round(float(v), 2) for v in coef[k, h]] for h in range(5)] for k in range(K)], float(np.sqrt((res ** 2).mean()))


# ------------------------------------------------------------------------------------------------------------ rendering
def item(layers, pad=3.0):
    """A scene item whose box covers every layer's shape."""
    bs = [bbox(L["shape"], pad + L.get("soft", 0) * 2) for L in layers]
    return {"box": [min(b[0] for b in bs), min(b[1] for b in bs), max(b[2] for b in bs), max(b[3] for b in bs)],
            "layers": layers}


def render(scene, box, s, bg=None, cache=None):
    """Render the scene over the frame box (x0, y0, x1, y1) at scale s; bg = frame-resolution image to start from
    (nearest-upscaled), else the flat backdrop. Returns float H x W x 3 at s."""
    x0, y0, x1, y1 = box
    W, H = (x1 - x0) * s, (y1 - y0) * s
    if bg is None:
        img = np.empty((H, W, 3))
        img[:] = C.FLAT
    else:
        img = np.kron(bg[y0:y1, x0:x1], np.ones((s, s, 1)))
    if cache is not None:
        cache = cache.setdefault((tuple(box), s), {})
    sdf.render(scene, img, s, origin=(x0, y0), cache=cache)
    return img


def down(img, s):
    H, W = img.shape[0] // s, img.shape[1] // s
    return img.reshape(H, s, W, s, 3).mean(axis=(1, 3))


def to8(img):
    return np.clip(img + 0.5, 0, 255).astype(np.uint8)


def pick_scale(w, h):
    """Enlargement for a comparison: small icons up to 8x so the short side reaches ~300 px, panels 2x, full sheets 2x
    in tiles."""
    short = max(min(w, h), 1)
    if short >= 100:
        return 2
    s = int(max(2, min(8, np.ceil(300 / short))))
    if max(w, h) >= 560:
        return 2                                      # full-width strips: 2x, tiled across
    return min(s, 4) if max(w, h) >= 300 else s       # plates and shorter strips: 4x, tiled across


def compare_png(path, art, mine_frame_render, rows, scale=None, max_w=1600, label=True):
    """art: frame-resolution composited art; rows: list of (state, (x, y, w, h)) frame boxes; mine_frame_render(box, s)
    returns my render of that box at scale s. Writes art LEFT (nearest) | mine RIGHT, rows stacked. A row too wide for
    max_w at its scale is cut into side-by-side tiles shown one under the other (labelled part i/n)."""
    from PIL import ImageDraw
    tiles = []
    for st, (x, y, w, h) in rows:
        s = scale or pick_scale(w, h)
        tw = (max_w - 8) // 2 // s           # frame pixels per tile
        n = int(np.ceil(w / tw))
        tw = int(np.ceil(w / n))
        for k in range(n):
            xa = x + k * tw
            wa = min(tw, x + w - xa)
            A = np.kron(art[y:y + h, xa:xa + wa], np.ones((s, s, 1)))
            M = mine_frame_render((xa, y, xa + wa, y + h), s)
            gap = np.zeros((A.shape[0], 8, 3))
            gap[:] = (20, 20, 20)
            lab = f"{st}" + (f" (part {k + 1}/{n}, x {xa}..{xa + wa})" if n > 1 else "") + f" at {s}x"
            tiles.append((lab, np.concatenate([A, gap, M], axis=1)))
    Wt = max(t.shape[1] for _, t in tiles)
    out = []
    for st, t in tiles:
        head = np.zeros((16, Wt, 3))
        head[:] = (20, 20, 20)
        body = np.zeros((t.shape[0], Wt, 3))
        body[:] = (20, 20, 20)
        body[:, :t.shape[1]] = t
        out += [head, body]
    im = Image.fromarray(to8(np.concatenate(out, axis=0)))
    if label:
        d = ImageDraw.Draw(im)
        yy = 0
        for st, t in tiles:
            d.text((4, yy + 2), f"{st}: art (left) | shapes (right)", fill=(255, 255, 0))
            yy += 16 + t.shape[0]
    im.save(path)
    return im.size


def score(art, mine, box, mask=None):
    """Mean absolute difference over a frame box (optionally only where mask)."""
    x0, y0, x1, y1 = box
    d = np.abs(art[y0:y1, x0:x1] - mine)
    if mask is not None:
        m = mask[y0:y1, x0:x1]
        return float(d[m].mean()) if m.any() else 0.0
    return float(d.mean())
