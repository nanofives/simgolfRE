"""Slots, fix round (2026-11-24, verdict "redo"): a new reading of the art, not round 1's shapes.

building-slot (BuildingPanel.pcx; tall 77 x 64, short 77 x 58), read at 8-10x with a grid and the cut's silhouette per column:
  an iso box seen from above. Its top is the TILE: a rhombus with ROUNDED corners (r 2.5; the top corner is flat over ~5 px)
  inside a 1.4 px navy outline, lit by one measured bevel (light rim inside the upper left edges, darker inside the lower
  right, the flat teal / green / navy by state). Behind the tile's two upper edges a lavender BACK RIM about 4 px wide shows
  (its outer edges parallel to the tile's, cut off by the sheet cell); under the tile's lower left edge the box's LIT LEFT
  FACE (pale lilac), under its lower right edge the SHADED RIGHT FACE, darkest right under the tile's edge and lightening
  downward (the tile's shadow on the face, measured as the face's gradient). The faces' lower edges are parallel to the tile's
  lower edges (tall: (0, 40.6)-(38.5, 64.6)-(77, 36.4); short: the same box 6 px shorter, cut off at the cell's bottom).
  Round 1 drew the tile with sharp corners, filled the cut's staircase extent as a "ground" and gave the right face a
  pink and green blend; none of that is here.
amenities-slot (AmenitiesPanel.pcx, 47 x 40 / 47 x 41): not a box but a tile sunk in a diamond SOCKET. The socket is the
  outer rhombus (cut by the cell at the top, left and right), painted with a bevel measured on it; the tile (rounded rhombus,
  navy outline, measured bevel: a white rim inside its upper left edges when selected) casts a SHADOW onto the socket below its
  lower edges: the tile's own shape moved down, soft, its strength fitted (ped2.fit_cast).
Shapes in each cut's local coordinates; every shape is clipped to the cell so a composed palette does not spill.
"""
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hp  # noqa: E402
import bubble as B  # noqa: E402
import geom as G  # noqa: E402
import ped2  # noqa: E402
from geom import poly, rect  # noqa: E402
from hp import sdf  # noqa: E402

TILE_STOPS = [0.0, 0.7, 1.5, 2.3, 3.2, 4.4, 6.5, 10.0]
SOCKET_STOPS = [0.0, 1.0, 2.0, 3.5, 5.0, 7.0, 10.0]


def rounded_rhombus(T, R, Bv, L, r):
    """A rhombus with every corner rounded to radius r: the rhombus shrunk so each edge moves in by r, grown back by r."""
    cx = (T[0] + R[0] + Bv[0] + L[0]) / 4.0
    cy = (T[1] + R[1] + Bv[1] + L[1]) / 4.0
    pts = [T, R, Bv, L]
    out = []
    for k, p in enumerate(pts):
        a, b = pts[k - 1], p          # edge into p
        ex, ey = b[0] - a[0], b[1] - a[1]
        n = np.hypot(ex, ey)
        dist = abs((cx - a[0]) * ey - (cy - a[1]) * ex) / n       # centre to this edge
        s = 1.0 - r / dist
        out.append((cx + (p[0] - cx) * s, cy + (p[1] - cy) * s))
    return G.grow(poly(*out), r)


def clip(s, W, H):
    return G.inter(s, rect(0.0, 0.0, W, H))


def bslot(T, R, Bv, L, band_outer, left_low, right_low, W, H):
    tile = rounded_rhombus(T, R, Bv, L, 2.5)
    # the rim ends where the tile's upper edges are (its lower boundary runs just inside them), so it never shows under a vertex
    band = clip(poly(*band_outer, (W + 1, R[1] - 0.5), (T[0], T[1] + 3.0), (-1, L[1] - 0.5)), W, H)
    left = clip(poly((-1, L[1] - 0.5), (Bv[0] + 0.6, Bv[1] - 0.4), (Bv[0] + 0.6, left_low[1][1]), (-1, left_low[0][1])), W, H)
    right = clip(poly((Bv[0] - 0.3, Bv[1] - 0.4), (W + 1, R[1] - 0.6), (W + 1, right_low[0][1]), (Bv[0] - 0.3, right_low[1][1])), W, H)
    return dict(tile=tile, band=band, left=left, right=right)


GEOM = {
    "tall": bslot((38.5, 1.2), (76.4, 26.6), (38.5, 52.4), (0.8, 27.4),
                  [(-1.0, 19.9), (30.1, -0.6), (54.6, -0.6), (78.0, 16.6)],
                  [(0.0, 40.6), (38.5, 64.6)], [(77.0, 36.4), (38.5, 64.6)], 77, 64),
    "short": bslot((37.8, 1.0), (76.3, 27.4), (38.6, 53.4), (0.8, 27.8),
                   [(-1.0, 20.6), (31.0, -0.6), (41.5, -0.6), (78.0, 23.6)],
                   [(0.0, 38.6), (38.6, 62.8)], [(77.0, 39.4), (38.6, 65.0)], 77, 58),
}


def build_building():
    el = "building-slot"
    scenes = {}
    for st, src, x0, y0, rgb, mask in hp.crops(el):
        g = GEOM["short" if "short" in st else "tall"]
        h, w = mask.shape
        X, Y = B.grid(x0, y0, w, h)
        mv = lambda s: G.moved(s, x0, y0)  # noqa: E731
        tile, band, left, right = (mv(g[k]) for k in ("tile", "band", "left", "right"))
        dt = sdf.sd(tile, X, Y)
        layers = []
        for shp, nm, n in ((band, "back rim: the lavender rim behind the tile's upper edges (measured gradient)", 4),
                           (left, "the box's lit left face under the tile's lower left edge (measured gradient)", 3),
                           (right, "the box's shaded right face, darkest under the tile's edge (measured gradient)", 4)):
            k = mask & (sdf.sd(shp, X, Y) < -0.5) & (dt > 0.9)
            for other in layers:
                pass
            layers.append({"shape": shp, "fill": B.measure_linear(rgb, k, x0, y0, n=n), "name": nm})
        own = mask & (dt < -0.05)
        tp = B.measure_bevel_shape(rgb, own, x0, y0, tile, TILE_STOPS)
        tp["bevel"]["of"] = tile
        layers.append({"shape": tile, "fill": tp, "name": "tile: a rhombus with rounded corners (r 2.5), lit by its measured bevel"})
        ol = {"t": "outline", "of": G.grow(tile, -0.7), "w": 1.4}
        ok = mask & (np.abs(dt + 0.7) < 0.5)
        layers.append({"shape": ol, "fill": dark_mode(rgb[ok]), "name": "tile: its 1.4 px navy outline (rounded corners)"})
        scenes[st] = [{"box": [x0 - 2, y0 - 2, x0 + w + 2, y0 + h + 2], "layers": layers}]
    finish(el, scenes, "an iso box seen from above (new reading): the tile on top a rhombus with rounded corners (r 2.5) in a "
                       "1.4 px navy outline, lit by one measured bevel (teal / green / navy by state); a lavender back rim "
                       "behind its two upper edges; the box's lit left face and shaded right face below (the right face "
                       "darkest under the tile's edge, each face's gradient measured); a tall and a short box")


# amenities slot: the outer socket rhombus and the tile (read on the idle cut; cut 2 is the same one row taller)
A_TILE = ((23.6, 3.6), (45.2, 18.8), (24.2, 33.4), (1.6, 18.6))
A_SOCKET = poly((-1.0, 10.7), (18.5, -1.0), (29.5, -1.0), (48.0, 12.4), (48.0, 28.4), (24.2, 40.7), (-1.0, 27.4))
A_SOCKET2 = poly((-1.0, 12.0), (21.4, -1.6), (26.6, -1.6), (48.0, 11.8), (48.0, 28.6), (24.2, 41.4), (-1.0, 26.4))   # cut 2's silhouette


def build_amen():
    el = "amenities-slot"
    scenes, meas = {}, {}
    for st, src, x0, y0, rgb, mask in hp.crops(el):
        h, w = mask.shape
        dy = 0.6 if st.endswith(" 2") else 0.0
        X, Y = B.grid(x0, y0, w, h)
        mv = lambda s: G.moved(s, x0, y0 + dy)  # noqa: E731
        tile = mv(rounded_rhombus(*A_TILE, 2.0))
        sock = G.inter(G.moved(A_SOCKET2 if st.endswith(" 2") else A_SOCKET, x0, y0), G.moved(rect(0, 0, w, h), x0, y0))
        dt = sdf.sd(tile, X, Y)
        ds = sdf.sd(sock, X, Y)
        # the socket's bevel is measured away from the tile's shadow (below the tile's lower edges)
        shadow_zone = G.grow(G.moved(tile, 0.8, 3.0), 1.5)
        sk = mask & (ds < -0.05) & (dt > 0.8) & (sdf.sd(shadow_zone, X, Y) > 0)
        # a measured gradient (a bevel on this clipped polygon showed ridges running in from its corners)
        Ls = {"shape": sock, "fill": B.measure_linear(rgb, sk, x0, y0, n=3),
              "name": "socket: the outer rhombus the tile sits in, its measured gradient"}
        recv = mask & (ds < -0.5) & (dt > 0.6)
        fn = ped2.shadow_layer(tile, "the tile's shadow on the socket below its lower edges (offset, softness, strength measured)",
                               clip=sock)
        p, _ = ped2.fit_cast(rgb.astype(float), recv, x0, y0, [{"box": [x0, y0, x0 + w, y0 + h], "layers": [Ls]}], fn,
                             init=(0.5, 2.0, 1.5, 0.5), steps=(1.0, 1.0, 1.0, 0.15), lo=(-2, 0, 0.3, 0), hi=(3, 6, 4, 0.95))
        Lsh = fn(p)
        own = mask & (dt < -0.05)
        tp = B.measure_bevel_shape(rgb, own, x0, y0, tile, TILE_STOPS)
        tp["bevel"]["of"] = tile
        Lt = {"shape": tile, "fill": tp, "name": "tile: a rhombus with rounded corners (r 2), lit by its measured bevel"}
        ok = mask & (np.abs(dt + 0.5) < 0.5)
        Lo = {"shape": {"t": "outline", "of": G.grow(tile, -0.5), "w": 1.0}, "fill": dark_mode(rgb[ok]),
              "name": "tile: its 1 px navy outline"}
        scenes[st] = [{"box": [x0 - 2, y0 - 2, x0 + w + 2, y0 + h + 2], "layers": [Ls, Lsh, Lt, Lo]}]
        meas[st] = {"tile_shadow": [round(float(v), 2) for v in p], "params": "dx, dy, soft, alpha"}
    finish(el, scenes, "a small tile sunk in a diamond socket (new reading): the socket an outer rhombus in its measured bevel; the "
                       "tile a rhombus with rounded corners (r 2), 1.4 px navy outline, measured bevel (lilac idle, light blue "
                       "with a white upper left rim selected, dark disabled); the tile's shadow on the socket below its lower "
                       "edges as its own soft layer, offset and strength measured", meas)


def dark_mode(v):
    if len(v) == 0:
        return [40, 40, 80]
    lum = v.astype(float) @ np.array([0.3, 0.59, 0.11])
    sel = v[lum <= np.percentile(lum, 12)]
    return [round(float(c), 1) for c in np.median(sel.astype(float), 0)]


def finish(el, scenes, how, meas=None):
    sc = hp.score(el, scenes, z=3)
    for st in scenes:
        print(f"  {el} {st}: mad {sc[st][0]:.2f} edge {sc[st][1]}")
    hp.compare(el, scenes)
    hp.save_scene(el, scenes, {"how": how, "measured": meas or {}})


if __name__ == "__main__":
    want = sys.argv[1:]
    if not want or "building-slot" in want:
        build_building()
    if not want or "amenities-slot" in want:
        build_amen()
