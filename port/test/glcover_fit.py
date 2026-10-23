"""Coverage-rule fit for Windows' generic OpenGL (phase 5 step 3): which pixels does a triangle cover? A probe scene of many
random flat-coloured triangles is run through the REAL software GL; candidate vertex / edge models are scored by the number of
pixels whose coverage differs from the real frame.

    ~/bin/py -3.12 -m port.test.glcover_fit [n_triangles]
"""
from __future__ import annotations

import sys

import numpy as np

from port.test import glprobe as P
from port.test import lib as L

F32 = np.float32


def make_scene(n: int, seed: int = 7):
    rng = np.random.default_rng(seed)
    cols, rows = 16, max(1, (n + 15) // 16)
    cw, ch = 800 // cols, 600 // rows
    tris = []
    for i in range(n):
        gx, gy = i % cols, i // cols
        ox, oy = gx * cw + 2, gy * ch + 2
        pts = [(float(F32(ox + rng.uniform(0, cw - 6))), float(F32(oy + rng.uniform(0, ch - 6)))) for _ in range(3)]
        tris.append(pts)
    return tris


def real_coverage(lib, tris):
    sc = P.Scene()
    sc.setup2d()
    for t in tris:
        sc.tri(*t, colors=[(1.0, 1.0, 1.0, 1.0)] * 3)
    d = sc.write("cover")
    real = P.real_frame(d)
    return (real.reshape(600, 800) != 0)


def cover_mask(t, shape=(600, 800)):
    """Boolean mask of the pixel centres inside the triangle (edge functions in doubles, top-left free: ties are rare)."""
    (x0, y0), (x1, y1), (x2, y2) = t
    xs0, xs1 = int(max(0, min(x0, x1, x2) - 2)), int(min(shape[1], max(x0, x1, x2) + 3))
    ys0, ys1 = int(max(0, min(y0, y1, y2) - 2)), int(min(shape[0], max(y0, y1, y2) + 3))
    px = np.arange(xs0, xs1)[None, :] + 0.5
    py = np.arange(ys0, ys1)[:, None] + 0.5
    ar = (x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0)
    e0 = (x2 - x1) * (py - y1) - (y2 - y1) * (px - x1)
    e1 = (x0 - x2) * (py - y2) - (y0 - y2) * (px - x2)
    e2 = (x1 - x0) * (py - y0) - (y1 - y0) * (px - x0)
    if ar < 0:
        e0, e1, e2 = -e0, -e1, -e2
    inside = (e0 >= 0) & (e1 >= 0) & (e2 >= 0)
    m = np.zeros(shape, bool)
    m[ys0:ys1, xs0:xs1] = inside
    return m


def score(tris, real_cov, vertex_fn):
    mine = np.zeros_like(real_cov)
    for t in tris:
        mine |= cover_mask([vertex_fn(p) for p in t])
    return int((mine != real_cov).sum())


def snap(bits: int, offset: float, per_axis_offset=(None, None)):
    q = float(1 << bits)
    ox, oy = per_axis_offset
    return lambda p: (np.floor(p[0] * q + (offset if ox is None else ox)) / q, np.floor(p[1] * q + (offset if oy is None else oy)) / q)


def span_mask(t, bits: int, off: float, shape=(600, 800)):
    """Span-based coverage: for each scanline centre the exact x interval of the triangle, its two ends snapped to 2^-bits
    (floor(v * 2^bits + off) / 2^bits), pixels whose centre lies in [left, right) covered."""
    q = float(1 << bits) if bits else 0.0
    (x0, y0), (x1, y1), (x2, y2) = t
    m = np.zeros(shape, bool)
    pts = [(x0, y0), (x1, y1), (x2, y2)]
    for y in range(int(max(0, min(y0, y1, y2))), int(min(shape[0], max(y0, y1, y2) + 1))):
        yc = y + 0.5
        xs = []
        for (ax, ay), (bx, by) in ((pts[0], pts[1]), (pts[1], pts[2]), (pts[2], pts[0])):
            if (ay <= yc < by) or (by <= yc < ay):
                xs.append(ax + (yc - ay) * (bx - ax) / (by - ay))
        if len(xs) < 2:
            continue
        lo, hi = min(xs), max(xs)
        if bits:
            lo, hi = np.floor(lo * q + off) / q, np.floor(hi * q + off) / q
        a, b = int(np.ceil(lo - 0.5)), int(np.ceil(hi - 0.5))
        if b > a:
            m[y, max(a, 0):min(b, shape[1])] = True
    return m


def score_span(tris, real_cov, bits, off):
    mine = np.zeros_like(real_cov)
    for t in tris:
        mine |= span_mask(t, bits, off)
    return int((mine != real_cov).sum())


def main():
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 480
    lib = L.load()  # noqa: F841
    tris = make_scene(n)
    real = real_coverage(lib, tris)
    print("triangles", n, "real covered pixels", int(real.sum()))
    print("exact doubles:", score(tris, real, lambda p: p))
    print("span (exact ends):", score_span(tris, real, 0, 0.0))
    for bits in (4, 6, 8, 10, 12, 14, 16):
        print("span ends snapped", bits, "bits  offset 0 / 0.5 / 1:", [score_span(tris, real, bits, o) for o in (0.0, 0.5, 1.0)], flush=True)


if __name__ == "__main__":
    main()
