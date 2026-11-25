"""P41 hud_panels round 3 (2026-11-24): version A ("faithful") of a glyph or a small picture, patch by patch.

Mariano rejected these redos twice with no note; the round-3 brief asks for a faithful version beside a clean one: every
dark, mid and light patch of the glyph becomes its own small shape in the art's colour, nothing simplified, no symmetry the
art lacks. This module does exactly that and nothing more:

1. The glyph's pixels: opaque art pixels inside `region` that differ from the redrawn surface under them (the approved
   bubble / disc scene rendered without its glyph) by more than `thr` levels in some channel.
2. Their tones: k-means on the pixels' RGB (k clusters, seeded by luminance quantiles), so each cluster is one tone the
   artist painted (core, mid, highlight, antialiased edge).
3. Shapes: a base shape (the whole glyph's pixel outline, one round of corner cutting) in the glyph's median colour, so no
   background shows between patches; then every 4-connected patch of every tone, largest tone first, as its own polygon:
   its pixel outline with one round of corner cutting (a rounded blob whose straight sides sit on the pixel edges, its
   corners cut at the quarter pixel), filled with that patch's own mean colour.

Usage from a builder: patch_layers(key, rct, bg_scene, region, thr=16, k=5) -> layers (sheet coordinates).
"""
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import common as C  # noqa: E402
import fix3  # noqa: E402
import fit  # noqa: E402
import sdf  # noqa: E402


def trace(mask, ox=0.0, oy=0.0):
    """Closed rings along the pixel edges of a boolean mask (outer rings and holes; even-odd), collinear points dropped."""
    H, W = mask.shape
    nxt = {}
    m = np.pad(mask, 1)
    for j in range(H):
        for i in range(W):
            if not m[j + 1, i + 1]:
                continue
            # clockwise in screen coordinates (y down): top edge left->right, right edge top->bottom, ...
            if not m[j, i + 1]:
                nxt.setdefault((i, j), []).append((i + 1, j))
            if not m[j + 1, i + 2]:
                nxt.setdefault((i + 1, j), []).append((i + 1, j + 1))
            if not m[j + 2, i + 1]:
                nxt.setdefault((i + 1, j + 1), []).append((i, j + 1))
            if not m[j + 1, i]:
                nxt.setdefault((i, j + 1), []).append((i, j))
    rings = []
    while nxt:
        start = next(iter(nxt))
        ring, p = [start], start
        while True:
            q = nxt[p].pop()
            if not nxt[p]:
                del nxt[p]
            if q == start:
                break
            ring.append(q)
            p = q
        # drop collinear points
        out = []
        n = len(ring)
        for k in range(n):
            a, b, c = ring[k - 1], ring[k], ring[(k + 1) % n]
            if (b[0] - a[0]) * (c[1] - b[1]) - (b[1] - a[1]) * (c[0] - b[0]) != 0:
                out.append([b[0] + ox, b[1] + oy])
        rings.append(out)
    return rings


def components(mask):
    H, W = mask.shape
    lab = np.zeros((H, W), int)
    n = 0
    for j in range(H):
        for i in range(W):
            if mask[j, i] and not lab[j, i]:
                n += 1
                st = [(j, i)]
                lab[j, i] = n
                while st:
                    a, b = st.pop()
                    for da, db in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                        c, d = a + da, b + db
                        if 0 <= c < H and 0 <= d < W and mask[c, d] and not lab[c, d]:
                            lab[c, d] = n
                            st.append((c, d))
    return [lab == t for t in range(1, n + 1)]


def kmeans(X, k, it=20):
    L = X.mean(axis=1)
    cen = np.array([X[np.argsort(L)[int((q + 0.5) / k * (len(X) - 1))]] for q in range(k)], float)
    for _ in range(it):
        d = ((X[:, None, :] - cen[None]) ** 2).sum(axis=2)
        lab = d.argmin(axis=1)
        for q in range(k):
            if (lab == q).any():
                cen[q] = X[lab == q].mean(axis=0)
    return lab, cen


def glyph_mask(key, rct, bg_scene, region=None, thr=16.0, sign=0):
    """sign -1: only pixels darker than the surface (a dark glyph), +1 only lighter, 0 either."""
    x0, y0, w, h = rct
    rgb, m = C.sheet(key)
    art = rgb[y0:y0 + h, x0:x0 + w]
    bg = fix3.low(bg_scene, rct, 4)
    diff = (art - bg) if sign == 0 else sign * (art - bg)
    mask = m[y0:y0 + h, x0:x0 + w] & ((np.abs(diff) if sign == 0 else diff).max(axis=2) > thr)
    if region is not None:
        jj, ii = np.mgrid[y0:y0 + h, x0:x0 + w]
        mask &= sdf.sd(region, ii + 0.5, jj + 0.5) < -0.3
    return mask, art


def patch_layers(key, rct, bg_scene, region=None, thr=16.0, k=5, mask=None, smooth=1, name="A", sign=0):
    x0, y0, w, h = rct
    if mask is None:
        mask, art = glyph_mask(key, rct, bg_scene, region, thr, sign)
    else:
        art = C.sheet(key)[0][y0:y0 + h, x0:x0 + w]
    if not mask.any():
        return []
    X = art[mask]
    lab, cen = kmeans(X, min(k, len(X)))
    tone = np.full(mask.shape, -1)
    tone[mask] = lab
    L = [{"name": f"{name} base", "shape": {"t": "poly", "rings": trace(mask, x0, y0), "smooth": smooth},
          "fill": [round(float(v), 1) for v in np.median(X, axis=0)]}]
    order = sorted(range(len(cen)), key=lambda q: -(lab == q).sum())
    for q in order:
        for comp in components(tone == q):
            L.append({"name": f"{name} tone {q}", "shape": {"t": "poly", "rings": trace(comp, x0, y0), "smooth": smooth},
                      "fill": [round(float(v), 1) for v in art[comp].mean(axis=0)]})
    return L


def strip_glyph(scene):
    """A copy of a bubble scene without its glyph layers (those carry a "name")."""
    import json
    sc = json.loads(json.dumps(scene))
    for it in sc:
        it["layers"] = [L for L in it["layers"] if "name" not in L]
    return sc
