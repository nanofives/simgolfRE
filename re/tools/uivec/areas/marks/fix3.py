"""P41 fix round 3 (2026-11-24), area `marks`: helpers for the two-version redo (A faithful, B clean).

Version A ("follow the art's pixels as closely as shapes allow"): every patch of one tone in the art becomes its own shape:
the patch's outline on the pixel grid (pixel corners, so every edge sits on the half-pixel grid of the art), its corners
cut twice (a staircase becomes a slope, a single pixel a round dot), painted in the art's colour for that patch. The patch
list is read from the art's palette grid (read/*.txt); the shapes are numbers only.
"""
import numpy as np

import common as C  # noqa: F401  (sets sys.path for sdf)
import sdf


def pixel_rings(mask):
    """Boundary rings (pixel-corner coordinates) of a boolean pixel set; interior on the right of each directed edge in
    screen coordinates (y down). Collinear unit vertices are kept (the corner cutting then rounds by a quarter pixel)."""
    h, w = mask.shape
    M = np.zeros((h + 2, w + 2), bool)
    M[1:-1, 1:-1] = mask
    out = {}
    for j in range(h):
        for i in range(w):
            if not M[j + 1, i + 1]:
                continue
            if not M[j, i + 1]:
                out.setdefault((i, j), []).append((i + 1, j))
            if not M[j + 1, i + 2]:
                out.setdefault((i + 1, j), []).append((i + 1, j + 1))
            if not M[j + 2, i + 1]:
                out.setdefault((i + 1, j + 1), []).append((i, j + 1))
            if not M[j + 1, i]:
                out.setdefault((i, j + 1), []).append((i, j))
    rings = []
    while out:
        start = next(iter(out))
        ring = [start]
        cur = start
        prev_dir = None
        while True:
            nxt = out[cur]
            if len(nxt) > 1 and prev_dir is not None:
                # a pinch (two pixels touching at a corner): keep turning right so each 4-connected part closes on itself
                def turn(p):
                    d = (p[0] - cur[0], p[1] - cur[1])
                    return (prev_dir[0] * d[1] - prev_dir[1] * d[0])
                nxt.sort(key=turn, reverse=True)
            p = nxt.pop(0)
            if not nxt:
                del out[cur]
            prev_dir = (p[0] - cur[0], p[1] - cur[1])
            cur = p
            if cur == start:
                break
            ring.append(cur)
        rings.append([[float(a), float(b)] for a, b in ring])
    return rings


def patch(mask, smooth=2, grow=0.08):
    """One art patch as a shape: its pixel outline, corners cut `smooth` times, grown a little so neighbours meet."""
    S = {"t": "poly", "rings": pixel_rings(mask), "smooth": smooth}
    return {"t": "grow", "of": S, "d": grow} if grow else S


def components(mask, conn=8):
    """Connected parts of a boolean grid (lists of (j, i))."""
    h, w = mask.shape
    seen = np.zeros_like(mask)
    parts = []
    nb = [(-1, 0), (1, 0), (0, -1), (0, 1)] + ([(-1, -1), (-1, 1), (1, -1), (1, 1)] if conn == 8 else [])
    for j in range(h):
        for i in range(w):
            if mask[j, i] and not seen[j, i]:
                st, part = [(j, i)], []
                seen[j, i] = True
                while st:
                    a, b = st.pop()
                    part.append((a, b))
                    for dj, di in nb:
                        y, x = a + dj, b + di
                        if 0 <= y < h and 0 <= x < w and mask[y, x] and not seen[y, x]:
                            seen[y, x] = True
                            st.append((y, x))
                parts.append(part)
    return parts


def part_mask(part, shape):
    m = np.zeros(shape, bool)
    for j, i in part:
        m[j, i] = True
    return m
