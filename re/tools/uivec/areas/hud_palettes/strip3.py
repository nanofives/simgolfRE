"""amenities-strip, round 3 (2026-11-24, verdict "fix": one of the squares is not the same dimensions; see the cubes below).
Round 2 (verdict "redo"): a new reading of the four pieces (start, middle, middle 2, end).

Read on luminance letter maps of every cut (5-level steps, 2026-11-24):
  panel      dark lilac (k) between a light RAIL along the top (rows 0..3: very light, lighter, light) and a BOTTOM EDGE (a 1 px
             mid line over a 2 px navy line, rows 75.5..79);
  cubes      faint iso cubes on the panel, each a TOP rhombus (o, a little lighter than the panel), a LIT LEFT FACE (p, lighter
             still) and a SHADED RIGHT FACE (n), the faces 11 px tall. On middle / middle 2 / end the cubes sit on a lattice
             (top rhombus 47 wide, 31 tall; cube A with its top corner at (23.6, 5.5), cubes B half a step aside at y 33.3);
             round 3: start has the same lattice cubes (round 2's wedges were a misreading);
             their edges are soft over about a pixel in the art (soft 0.6 here);
  edges      start ends on the left in a CONCAVE SCALLOP and middle 2 on the right in another, end in a CONVEX CAP: each a navy
             line about 2 px wide with the panel darkening toward it over 4 px (an inner shadow, drawn as a soft dark band
             clipped to the panel), the rail bending down along the edge's top as a light band;
  beyond     the cut carries a little of what lies past the edge: the lilac floor (l) and its light rail at rows 17..20.
Round 1 drew the cubes as two hard polygons per piece, a 4 px navy edge and no inner shadow. Local coordinates per cut.
"""
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hp  # noqa: E402
import bubble as B  # noqa: E402
import geom as G  # noqa: E402
from geom import poly, rect, line  # noqa: E402
from hp import sdf  # noqa: E402


def cube_lattice(cx, ty, hw=23.8, hh=15.5, face=11.0):
    T, R, Bv, L = (cx, ty), (cx + hw, ty + hh), (cx, ty + 2 * hh), (cx - hw, ty + hh)
    return {"top": poly(T, R, Bv, L), "left": poly(L, Bv, (Bv[0], Bv[1] + face), (L[0], L[1] + face)),
            "right": poly(Bv, R, (R[0], R[1] + face), (Bv[0], Bv[1] + face))}


def cubes(*cs):
    out = []
    for i, (cx, ty) in enumerate(cs):
        c = cube_lattice(cx, ty)
        out += [(f"cube {i + 1}: top rhombus", c["top"], "top"), (f"cube {i + 1}: lit left face", c["left"], "left"),
                (f"cube {i + 1}: shaded right face", c["right"], "right")]
    return out


# Round 3 (note "One of the squares is not same dimensions and proportions"): every cube of the four pieces is now ONE size
# (top rhombus 47.6 x 31, faces 11 tall, the next row of cubes 27.8 lower and half a step aside), only its place differs.
# Round 2 drew start's cube as hand-read wedges whose top was 16 wide for 15 tall (the lattice is 23.8 for 15.5) and
# 8 px left of where the art has it; a tone-class fit on start's cut puts the same lattice cube there (cube A's top
# corner at (45.25, 6.5), agreement 0.83 against 0.93 on middle, which has no edge shadow).
HW, HH, FACE, VSTEP = 23.8, 15.5, 11.0, 27.8
MID_CUBES = cubes((24.0, 6.0), (24.0 - HW, 6.0 + VSTEP), (24.0 + HW, 6.0 + VSTEP))
# middle 2: the cube below to the right would sit past the scallop (the art has plain panel there)
MID2_CUBES = cubes((23.6, 6.0), (23.6 - HW, 6.0 + VSTEP))
# end: only one cube shows: its top rhombus's right corner at (22.5, 49) and its shaded right face below (read on the cut)
END_CUBES = cubes((-1.3, 33.5))
# start: cube A and the cube below to its right; left of them the art has the plain panel (no cube there)
START_CUBES = cubes((45.25, 6.5), (45.25 + HW, 6.5 + VSTEP))

# edges (the navy line's centre, read row by row)
START_EDGE = [(12.3, -0.5), (10.5, 5.0), (8.5, 8.5), (7.2, 12.0), (6.2, 14.5), (5.0, 17.5), (3.2, 21.8), (2.6, 23.6), (4.0, 25.8), (8.5, 28.0), (12.0, 30.0), (16.5, 33.0),
              (20.0, 36.0), (24.0, 40.0), (28.0, 44.0), (33.0, 50.0), (37.0, 55.0), (40.0, 60.0), (42.5, 65.0), (44.0, 70.0),
              (45.5, 75.0), (46.0, 79.5)]
MID2_EDGE = [(57.0, -0.5), (58.5, 4.0), (60.5, 8.0), (62.0, 12.0), (63.5, 16.0), (64.4, 20.5), (63.6, 23.8), (61.5, 26.5),
             (59.0, 28.5), (56.0, 30.5), (52.0, 33.0), (48.5, 36.0), (43.5, 40.5), (38.0, 47.0), (33.5, 52.0), (30.0, 56.0),
             (28.0, 60.0), (26.5, 64.0), (24.5, 69.0), (23.0, 73.0), (22.5, 79.5)]
END_EDGE = [(9.0, -0.5), (13.5, 2.0), (17.0, 4.0), (20.0, 6.0), (22.0, 8.0), (23.2, 10.0), (25.0, 12.0), (26.2, 14.0), (27.5, 16.0),
            (28.6, 18.0), (29.5, 20.0), (30.4, 22.0), (31.1, 24.0), (32.0, 27.0), (33.0, 30.0), (33.6, 33.0), (34.1, 36.0),
            (34.6, 39.0), (35.0, 42.0), (35.0, 45.0), (34.8, 48.0), (34.2, 51.0), (32.6, 54.0), (31.0, 56.0), (29.8, 58.0),
            (28.5, 60.0), (27.5, 62.0), (26.5, 64.0), (25.5, 66.0), (24.5, 68.0), (23.5, 70.0), (22.6, 72.0), (22.0, 74.0),
            (21.5, 76.0), (21.0, 79.5)]

PIECES = {
    # state: (panel outline, cubes, edge path or None, rail bend along the edge's top, what the cut shows beyond the edge)
    "start": (poly(*START_EDGE, (68.5, 79.5), (68.5, -0.5), (12.3, -0.5)), START_CUBES, START_EDGE,
              [(14.6, 0.6), (12.7, 5.0), (10.8, 8.5), (9.4, 12.0), (8.2, 15.0), (6.8, 18.6), (5.0, 22.8)],
              poly((-0.6, 16.8), (6.0, 16.8), (2.6, 24.0), (3.0, 26.4), (-0.6, 26.4))),
    "middle": (rect(-0.5, -0.5, 47.5, 79.5), MID_CUBES, None, None, None),
    "middle 2": (poly((-0.5, -0.5), *MID2_EDGE, (-0.5, 79.5)), MID2_CUBES, MID2_EDGE, None,
                 poly((62.0, 16.8), (67.6, 16.8), (67.6, 27.6), (63.0, 27.6))),
    "end": (poly((-0.5, -0.5), *END_EDGE, (-0.5, 79.5)), END_CUBES, END_EDGE,
            [(4.5, 1.5), (9.0, 2.4), (12.0, 3.4), (14.0, 4.6), (15.5, 5.8), (16.8, 7.2)],
            poly((27.0, 16.8), (35.2, 16.8), (35.2, 21.0), (38.6, 23.6), (41.2, 27.0), (41.2, 51.6), (37.0, 53.6), (33.0, 54.0))),
}


def mode_rgb(v):
    if len(v) == 0:
        return None
    u, n = np.unique(v.astype(int), axis=0, return_counts=True)
    return [float(c) for c in u[int(np.argmax(n))]]


def build():
    el = "amenities-strip"
    scenes = {}
    for st, src, x0, y0, rgb, mask in hp.crops(el):
        panel0, cubes0, edge0, bend0, beyond0 = PIECES[st]
        h, w = mask.shape
        X, Y = B.grid(x0, y0, w, h)
        mv = lambda s: G.moved(s, x0, y0)  # noqa: E731
        box = mv(rect(0, 0, w, h))
        panel = G.inter(mv(panel0), box)
        dp = sdf.sd(panel, X, Y)
        layers = []
        # beyond the edge: the lilac floor and its light rail (only where the cut shows them), and the light lip the floor
        # makes along the outside of the navy line
        if edge0 is not None:
            beyond = G.sub(mv(beyond0), panel)
            bk = mask & (dp > 1.6) & (sdf.sd(beyond, X, Y) < -0.4) & ((Y - y0 > 21.5) | (Y - y0 < 16))
            layers.append({"shape": beyond, "fill": mode_rgb(rgb[bk]) or [118, 118, 166],
                           "name": "beyond the edge: the lilac floor the strip stands on"})
            rail_b = G.inter(beyond, mv(rect(0, 17.0, w, 20.0)))
            rk = mask & (dp > 1.6) & (sdf.sd(rail_b, X, Y) < -0.2)
            if rk.sum() > 2:
                layers.append({"shape": rail_b, "fill": B.measure_linear(rgb, rk, x0, y0, n=3),
                               "name": "beyond the edge: the floor's light rail"})
            lip = G.sub(mv(line(*edge0, w=5.0, cap="round")), panel)
            dl = sdf.sd(mv(line(*edge0, w=1.0)), X, Y)
            lk = mask & (dp > 1.3) & (dl < 2.6) & ((Y - y0 > 21.5) | (Y - y0 < 16))
            layers.append({"shape": G.inter(lip, box), "fill": mode_rgb(rgb[lk]) or [118, 118, 166],
                           "name": "the floor's light lip along the outside of the edge"})
        # the panel and its cubes (flat measured tones, soft edges as in the art)
        cube_shapes = [(n, G.inter(mv(s), panel), k) for n, s, k in cubes0]
        inside = mask & (dp < -3.5) & (Y - y0 > 3.5) & (Y - y0 < 74.5)
        cover = {k: np.zeros_like(mask) for k in ("top", "left", "right")}
        anyc = np.zeros_like(mask)
        ds = [sdf.sd(s, X, Y) for _, s, _ in cube_shapes]
        for (n, s, k), d in zip(cube_shapes, ds):
            cover[k] |= d < -0.8
            anyc |= d < 0.8
        later_left_right = cover["left"] | cover["right"]
        cols = {"base": mode_rgb(rgb[inside & ~anyc]) or [110, 110, 158],
                "top": mode_rgb(rgb[inside & cover["top"] & ~later_left_right]),
                "left": mode_rgb(rgb[inside & cover["left"]]), "right": mode_rgb(rgb[inside & cover["right"]])}
        layers.append({"shape": panel, "fill": cols["base"], "name": "panel: dark lilac"})
        for n, s, k in cube_shapes:
            if cols.get(k):
                # grown 0.6 px under the next face so two soft edges never let the panel show through as a seam
                layers.append({"shape": G.inter(G.grow(s, 0.6) if k != "right" else s, panel), "fill": cols[k], "soft": 0.6,
                               "name": "faint " + n})
        # rail along the top and the bottom edge
        rail = G.inter(mv(rect(-1, -1, w + 1, 3.0)), panel)
        rk = mask & (sdf.sd(rail, X, Y) < -0.1) & (dp < -1.5)
        layers.append({"shape": rail, "fill": B.measure_linear(rgb, rk, x0, y0, n=3), "name": "light rail along the top"})
        if bend0:
            bend = G.inter(mv(line(*bend0, w=3.0, cap="round")), panel)
            bk = mask & (sdf.sd(bend, X, Y) < -0.3) & (Y - y0 > 3.2)
            lumb = fit_lum(rgb)
            layers.append({"shape": bend, "fill": mode_rgb(rgb[bk & (lumb > 150)]) or mode_rgb(rgb[bk & (lumb > 135)]) or cols["top"], "soft": 0.45,
                           "name": "the rail bending down along the edge's top: a light band"})
        mid = G.inter(mv(rect(-1, 75.5, w + 1, 76.6)), panel)
        mk = mask & (sdf.sd(mid, X, Y) < -0.2) & (dp < -1.5)
        layers.append({"shape": mid, "fill": mode_rgb(rgb[mk]) or [95, 95, 140], "name": "bottom edge: a 1 px mid line"})
        bot = G.inter(mv(rect(-1, 76.6, w + 1, 80.0)), panel)
        nk = mask & (sdf.sd(bot, X, Y) < -0.3) & (dp < -1.5)
        layers.append({"shape": bot, "fill": mode_rgb(rgb[nk]) or [40, 40, 80], "name": "bottom edge: a 2 px navy line"})
        # the scallop / cap: the panel darkening toward it (inner shadow) and the navy line itself
        if edge0 is not None:
            path = mv(line(*edge0, w=1.0, cap="round"))
            dpath = sdf.sd(path, X, Y)
            ek = mask & (dpath < 0.4)
            navy = mode_rgb(rgb[ek & (fit_lum(rgb) < 80)]) or [40, 40, 90]
            layers.append({"shape": G.inter(mv(line(*edge0, w=9.0, cap="round")), panel), "fill": navy, "alpha": 0.55,
                           "soft": 2.2, "name": "the panel's inner shadow toward the edge (darkening over 4 px)"})
            layers.append({"shape": mv(line(*edge0, w=2.0, cap="round")), "fill": navy, "clip": box,
                           "name": "the edge: a 2 px navy line (one smooth path, round ends)"})
        scenes[st] = [{"box": [x0 - 2, y0 - 2, x0 + w + 2, y0 + h + 2], "layers": layers}]
    sc = hp.score(el, scenes, z=3)
    for st in scenes:
        print(f"  {el} {st}: mad {sc[st][0]:.2f} edge {sc[st][1]}")
    hp.compare(el, scenes)
    hp.save_scene(el, scenes, {"how": HOW})


def fit_lum(rgb):
    return rgb.astype(float) @ np.array([0.3, 0.59, 0.11])


HOW = ("four strip pieces: a dark lilac panel with a light rail along the top and a mid line over a navy line along the bottom; "
       "faint iso cubes on it, every one the same size (top rhombus 47.6 x 31, faces 11 tall, the next row 27.8 lower and half a "
       "step aside), each a top rhombus, a lit left face and a shaded right face with soft edges; start ends on the left in a "
       "concave scallop, middle 2 on the right in another, end in a convex cap: each a 2 px navy line drawn as one smooth path "
       "with the panel darkening toward it (inner shadow) and the rail bending down along its top; the floor and its rail "
       "beyond the edge")

if __name__ == "__main__":
    build()
