"""P41 popups round 3: trans-small-arrows (redo, two versions). TransPopups.pcx, cells (0, y0, 35, y0 + 12), y0 = 200 / 220
(cream pair), 250 / 270 (orange pair).

Read again at 16x with every pixel dumped as hex:
- the plate: a pointed dome on a 3-row bar (row extents as round 2's DOME; the down cells' plate is the up one upside down);
  its rims are lit, no dark fringe; up cells: light rim cccde4 / bebfdc along the upper left (bebfdc / b3b5d7 lower down),
  ababd2 / a3a4ce along the upper right, a8a8bf over the apex; down cells: a dark bar on top (8183ab; one row of 6870ba in the
  orange pair), the lower edges in shadow, darkest at the lower right (373949), a lighter left rim;
- the orange pair is the PRESSED state: same plate, the up triangle one row lower, the down triangle one row higher, so the
  light lip under the up triangle is one row (pair 1: two rows) and the dark bar over the down triangle one row (pair 1: two);
- up triangle: apex (20.4, 200.9), base y 209.0, x 7.9-32.3 (edges from the per-row coverage of the AA pixels); along its left
  edge a white band (f2f2ed / fdf9f3, 1.6 px), a cream band (fcf5ec / fcf1d9, 2 px), a light yellow band (fae6af / f2e2b9 /
  eed697), then the face (e8ca7d; orange fcc102); its last rows shaded (d4c27b, c5ac6a; f6b402, f6a402), the shade starting
  further left row by row; a groove 2 px wide along its left edge (8183ab / 8889b3; blue 6870ba / 737dc1 in the orange pair)
  and a milder one along its right edge; under it a light lip (cccde4 / d8d9ea, e8e8f2 at its right end, x 29-31);
- down triangle: top edge y 222.0, x 8.6-31.7, its top right corner cut vertical down to y 224.4, apex (21.7, 231.1); its top
  two rows light cream (f5e9c8; fee88a) with a white corner at the upper left, a white band 3.2 px wide along its right edge
  (f5f6f9) down to the apex, the face darker toward that band (e8ca7d -> d4c27b -> c5ac6a), its left edge c5ac6a; a light line
  (ababd2 / bebfdc) right of the white band and a faint one (9d9db5) left of it.
Round 2 against the art at 16x, the five largest differences: (1) the triangles' light: round 2 drew a thin white line and
flat tan, the art has a broad white -> cream -> yellow band along the lit edge (4-5 px) and a 3 px white band on the down
triangle; (2) the down triangle's top rows were tan, in the art they are light cream; (3) the plate was faceted (corners on the
pixel steps) and its down-cell shading too dark and uniform, the art shades the lower edges only, darkest at the lower right;
(4) the grooves were hard dark wedges, in the art a 2 px band that fades at both ends (and blue in the orange pair); (5) the
up triangle's light lip was too dim, in the art the brightest lilac of the cell with a white right end.

A, faithful: every patch above is its own shape, painted with the art's median colour inside it (measured per cell, so each
pair keeps its own paint). B, clean: the same parts as clean lit surfaces with gradients and crisp highlights.

py -3.12 sa3.py [a|b]
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import w3               # noqa: E402
import common as c      # noqa: E402
import auto             # noqa: E402
import smalls2          # noqa: E402
from auto import mv, poly  # noqa: E402
from lens3 import H, V, I, Sb, hx, edge_seg  # noqa: E402

w3.apply()
TP = "TransPopups.pcx"
CELLS = [("up", 200, "up", 1), ("down", 220, "down", 1), ("up 2", 250, "up", 2), ("down 2", 270, "down", 2)]


def plate(flip, y0, smooth=0):
    dy = y0 - (220 if flip else 200)
    dome = poly(*[(x, (432.0 - y if flip else y) + dy) for x, y in smalls2.DOME])
    if smooth:
        dome["smooth"] = smooth
    by = (432.0 - 210.5 if flip else 210.5) + dy
    return {"t": "union", "of": [dome, c.rr(3.0, by - 1.5, 33.0, by + 1.5, 0.0)]}


# B's dome: a clean roof read from the row extents (top slopes 3 px per row from the apex, lower left 1 px per row, the right
# shoulder at x 35 on row 205), rounded by corner cutting
DOME_B = [(3.0, 209.5), (3.0, 208.6), (7.8, 204.3), (10.5, 202.6), (19.2, 200.0), (22.8, 200.0), (30.0, 202.7), (33.6, 204.2),
          (34.8, 205.3), (33.0, 207.4), (33.0, 209.5)]


def plate_b(flip, y0):
    dy = y0 - (220 if flip else 200)
    dome = poly(*[(x, (432.0 - y if flip else y) + dy) for x, y in DOME_B], smooth=2)
    by = (432.0 - 210.5 if flip else 210.5) + dy
    return {"t": "union", "of": [dome, c.rr(3.0, by - 1.5, 33.0, by + 1.5, 0.0)]}


def up_geom(y0, k):
    o = y0 - 200 + (1 if k == 2 else 0)
    apex, bl, br = (20.4, 200.9 + o), (7.9, 209.0 + o), (32.3, 209.0 + o)
    return o, apex, bl, br, poly(apex, br, bl)


def down_geom(y0, k):
    o = y0 - 220 - (1 if k == 2 else 0)
    tl, tr, cr, apex = (8.6, 222.0 + o), (31.7, 222.0 + o), (31.7, 224.4 + o), (21.7, 231.1 + o)
    return o, tl, tr, cr, apex, poly(tl, tr, cr, apex)


# ------------------------------------------------------------------------------------------------------------- version A
def up_a(y0, k):
    P = plate(False, y0)
    o, apex, bl, br, U = up_geom(y0, k)
    top = y0 + 5.5
    lip_bot = y0 + 11.0
    sx = (18.6, 23.6) if k == 1 else (16.6, 27.6)        # the first shaded row's patch (pair 1: d4c27b x 19-23; 2: f6b402)
    F = "flat"
    parts = [(P, F),
             (I(Sb(P, mv(P, -2.0, 1.2)), V(21.0)), F),
             (I(Sb(P, mv(P, -0.9, 0.6)), V(21.0)), F)]
    for d in (3.0, 2.0, 1.0):                             # the left rim: three bands, brighter in the upper rows
        band = Sb(P, mv(P, d, 0.85 * d))
        parts += [(I(band, V(20.0, right=False), H(top, below=False)), F), (I(band, V(20.0, right=False), H(top)), F)]
    parts += [
        (I(Sb(mv(U, 0.0, -1.4), U), V(15.5), V(25.5, right=False), P), F),                     # over the apex
        (I(Sb(edge_seg(bl, apex, 0.05, 0.88, 1.2, 2.4), U), P), F),                            # left groove
        (I(Sb(edge_seg(bl, apex, 0.2, 0.6, 0.8, 1.6), U), P), F),
        (I(Sb(edge_seg(apex, br, 0.25, 0.82, 1.0, 2.0), U), P), F),                            # right groove
        (I(Sb(edge_seg(apex, br, 0.3, 0.7, 0.6, 1.2), U), P), F),
        (c.rr(9.4, 209.0 + o, 32.6, lip_bot, 0.0), F),                                        # the light lip
        (c.rr(9.4, 209.0 + o, 11.8, lip_bot, 0.0), F),
        (c.rr(28.6, 209.0 + o, 32.0, 210.0 + o, 0.0), F),
        (U, F),
        (I(Sb(U, mv(U, 5.0, 0.0)), H(207.0 + o, below=False)), F),                             # light yellow band
        (I(Sb(U, mv(U, 3.6, 0.0)), H(207.0 + o, below=False)), F),                             # cream band
        (I(Sb(U, mv(U, 1.6, 0.0)), H(207.0 + o, below=False)), F),                             # white band
        (I(Sb(U, mv(U, 2.6, 0.0)), H(207.0 + o), H(208.0 + o, below=False)), F),
        (I(Sb(U, mv(U, 1.6, 0.0)), H(208.0 + o)), F),
        (I(U, H(206.0 + o), H(207.0 + o, below=False), V(sx[0]), V(sx[1], right=False)), F),  # first shaded row
        (I(U, H(207.0 + o), mv(U, 3.6, 0.0)), F),                                             # last two rows in shade
    ]
    return parts


def down_a(y0, k):
    P = plate(True, y0)
    o, tl, tr, cr, apex, D = down_geom(y0, k)
    bar_bot = 222.0 + o
    F = "flat"
    parts = [(P, F)]
    for x0, x1 in ((7.0, 32.0), (8.0, 31.0), (9.0, 30.0), (10.0, 29.0)):                    # the dark bar, darker inward
        parts.append((c.rr(x0, y0, x1, bar_bot, 0.0), F))
    parts += [(I(Sb(P, mv(P, 1.0, 0.0)), H(y0 + 4.0, below=False)), F),                       # lighter left rim
              (I(Sb(P, mv(P, -1.0, 0.0)), H(y0 + 5.0, below=False)), F)]                      # right rim
    for vx, vy in ((-2.0, -5.0), (-2.0, -4.2), (-2.0, -3.4), (-1.8, -2.6), (-1.4, -1.8), (-0.8, -0.9)):
        parts.append((I(Sb(P, mv(P, vx, vy)), H(y0 + 4.5)), F))                               # shadow on the lower edges
    parts += [
        (I(Sb(edge_seg(apex, tl, 0.15, 0.85, 0.6, 1.2), D), P), F),                            # faint line left of it
        (D, F),
        (I(D, mv(D, -6.6, 0.0), mv(D, 1.6, 0.0)), F),
        (I(D, mv(D, -7.5, 0.0), mv(D, 3.0, 0.0), H(226.0 + o, below=False)), F),              # the face's core
        (I(D, H(224.0 + o), H(225.0 + o, below=False), V(15.6), V(22.6, right=False)), F),
        (I(D, poly((9.0, 221.5 + o), (26.2, 221.5 + o), (23.2, 224.0 + o), (11.0, 224.0 + o))), F),  # light top rows
        (I(D, poly((26.2, 221.5 + o), (29.0, 221.5 + o), (26.0, 224.0 + o), (23.2, 224.0 + o))), F),
        (I(D, H(223.0 + o, below=False), V(13.6, right=False)), F),                             # white corner
        (I(Sb(D, mv(D, -3.4, 0.0)), H(222.9 + o)), F),                                         # white band, right edge
        # (the light pixels right of the white band, bebfdc / ababd2 / afb1ca, are its own edge blended into the dark plate)
    ]
    return parts


HOW_A = ("A, faithful: per cell the plate (dome on a 3-row bar, light rims as crescent bands, the up cells' rim brighter in "
         "the upper rows, the down cells' dark bar and six shadow bands on the lower edges), the up triangle with its white, "
         "cream and light yellow bands along the left edge, its shaded last rows, the 2 px grooves along both edges and the "
         "light lip under it; the down triangle with its light top rows, white corner, 3.2 px white band along the right edge, "
         "the face darkening toward it, and a faint light line left of it; every patch painted with the art's median colour inside "
         "it, measured per cell; the orange pair is the pressed state (triangles one row nearer the bar)")
CHANGED_A = ("redo, rejected twice with no note; against round 2: the triangles' light is the art's broad white -> cream -> "
             "yellow band (up) and 3 px white band (down) instead of a thin line on flat tan; the down triangle's top rows "
             "light cream; the plate's shadow only on its lower edges, darkest at the lower right; grooves that fade at both "
             "ends (blue in the orange pair); the light lip as bright as in the art with its white right end; plate corners "
             "rounded one step")


def measured(box, parts, name):
    """auto.measured_item, then any part left without pixels of its own (a 1 px band whose pixels all sit near a later part)
    is measured again on the pixels inside it that no later part covers by more than half a pixel, then on all its pixels."""
    import numpy as np
    it = auto.measured_item(TP, box, parts, inset=0.2, gap=0.3, name=name)
    a = c.art(TP, box)
    rgb, m, _ = c.sheet(TP)
    x0, y0, x1, y1 = box
    jj, ii = np.mgrid[y0:y1, x0:x1]
    x, y = ii + 0.5, jj + 0.5
    ds = [c.sdf.sd(q[0], x, y) for q in parts]
    for k, L in enumerate(it["layers"]):
        if L["fill"] != [128, 128, 128]:
            continue
        own = (ds[k] < 0.0) & m[y0:y1, x0:x1]
        mm = own.copy()
        for d2 in ds[k + 1:]:
            mm &= d2 > -0.5
        use = mm if mm.any() else own
        if use.any():
            L["fill"] = [round(float(v), 1) for v in np.median(a[use], axis=0)]
        else:
            print("  no pixels for part", k, name)
    return it


# ------------------------------------------------------------------------------------------------------------- version B
PB = {
    1: {"body": "9597c6", "rimL": "d2d3ea", "rimL2": "b8b9da", "rimR": "aeafd4", "top": "a8a8c4", "groove": "7c7ea6",
        "grooveR": "8a8cb6", "lip": "cfd0e7", "lipHi": "eeeef6", "face": "e8ca7d", "faceLt": "f6e4b4", "cream": "fcf2dc",
        "white": "fdfbf5", "shade": "c9b06c", "shadeDk": "a8925a", "bar": "7e80a7", "barEnd": "8c8ebb", "dk": "3a3c4e",
        "mid": "6a6c8e", "dTop": "f6eccd", "dWhite": "f7f8fb", "lineR": "c0c1de"},
    2: {"body": "9597c6", "rimL": "d2d3ea", "rimL2": "b8b9da", "rimR": "aeafd4", "top": "aab0d0", "groove": "6870ba",
        "grooveR": "7f8bc8", "lip": "cfd0e7", "lipHi": "eeeef6", "face": "fcc102", "faceLt": "fde07a", "cream": "fef3cc",
        "white": "fefcf0", "shade": "f6ac02", "shadeDk": "e89a02", "bar": "6870ba", "barEnd": "8183ab", "dk": "3a3c4e",
        "mid": "6a6c8e", "dTop": "feeca0", "dWhite": "f7f8fb", "lineR": "c0c1de"},
}


def lin(a, b, stops):
    return {"linear": {"a": list(a), "b": list(b), "stops": [[t, hx(col)] for t, col in stops]}}


def LB(S, fill, **kw):
    d = {"shape": S, "fill": hx(fill) if isinstance(fill, str) else fill}
    d.update(kw)
    return d


def up_b(y0, k):
    C = PB[k]
    P = plate_b(False, y0)
    o, apex, bl, br, U = up_geom(y0, k)
    lip_bot = y0 + 11.0
    lay = [LB(P, lin((0.0, y0), (0.0, y0 + 12.0), [(0.0, C["rimR"]), (0.45, C["body"]), (1.0, C["body"])])),
           LB(I(Sb(P, mv(P, 1.6, 1.4)), V(20.4, right=False)), C["rimL2"], soft=0.5, clip=P),
           LB(I(Sb(P, mv(P, 0.8, 0.7)), V(20.4, right=False), H(y0 + 8.0, below=False)), C["rimL"], soft=0.4, clip=P),
           LB(I(Sb(P, mv(P, -0.9, 0.7)), V(20.4)), C["rimR"], soft=0.4, clip=P),
           LB(I(Sb(edge_seg(bl, apex, 0.0, 1.04, 1.1, 2.2), U), P), C["groove"], soft=0.6),
           LB(I(Sb(edge_seg(apex, br, 0.1, 0.9, 0.8, 1.6), U), P), C["grooveR"], soft=0.6),
           # the lip: a bright ledge, its right end catching the light
           LB(c.rr(9.6, 209.0 + o, 32.4, lip_bot, 0.5), lin((9.6, 0.0), (32.4, 0.0),
                                                           [(0.0, C["rimL2"]), (0.12, C["lip"]), (0.8, C["lip"]),
                                                            (1.0, C["lipHi"])])),
           # the up triangle: face from light (apex, left) to shade (base), a crisp white edge and a cream band on the left
           LB(U, lin((15.0, 202.0 + o), (21.5, 209.0 + o),
                     [(0.0, C["faceLt"]), (0.45, C["face"]), (0.75, C["face"]), (0.9, C["shade"]), (1.0, C["shadeDk"])])),
           LB(I(Sb(U, mv(U, 4.0, 0.0)), H(207.5 + o, below=False)), C["cream"], soft=0.6, clip=U),
           LB(I(Sb(U, mv(U, 1.7, 0.0)), H(207.8 + o, below=False)), C["white"], soft=0.35, clip=U),
           ]
    return lay


def down_b(y0, k):
    C = PB[k]
    P = plate_b(True, y0)
    o, tl, tr, cr, apex, D = down_geom(y0, k)
    lay = [LB(P, C["body"]),
           LB(c.rr(4.0, y0, 32.0, 222.0 + o, 0.0), lin((4.0, 0.0), (32.0, 0.0),
                                                      [(0.0, C["body"]), (0.2, C["barEnd"]), (0.3, C["bar"]),
                                                       (0.72, C["bar"]), (0.82, C["barEnd"]), (1.0, C["body"])])),
           LB(I(Sb(P, mv(P, 1.0, 0.0)), H(y0 + 5.0, below=False)), "a3a4ce", soft=0.4, clip=P),
           # the shadow on the lower edges: a crescent widest at the lower right, darkening downward
           LB(I(Sb(P, mv(P, -2.2, -4.6)), H(y0 + 4.0)),
              lin((0.0, y0 + 5.0), (0.0, y0 + 12.0), [(0.0, "8486b0"), (0.4, C["mid"]), (1.0, C["dk"])]), soft=0.8, clip=P),
           LB(I(Sb(edge_seg(cr, apex, 0.0, 0.7, 0.7, 1.4), D), P), C["lineR"], soft=0.5),
           # the down triangle: light top, the face darkening toward the lit right edge's white band
           LB(D, lin((12.0, 222.0 + o), (25.0, 228.0 + o),
                     [(0.0, C["dTop"]), (0.3, C["faceLt"]), (0.55, C["face"]), (1.0, C["shade"])])),
           LB(I(D, H(224.0 + o, below=False)), C["dTop"], soft=0.6, clip=D),
           LB(I(Sb(D, mv(D, -3.9, 0.0)), H(224.0 + o)), C["shade"], soft=0.6, clip=D),
           LB(I(Sb(D, mv(D, -2.8, 0.0)), H(223.0 + o)), C["dWhite"], soft=0.35, clip=D),
           ]
    return lay


HOW_B = ("B, clean: the plates with rounded corners, lit from the upper left (bright rim on the upper left, light rim upper "
         "right; the down plates with a dark bar and a shadow crescent along the lower edges darkening to near black at the "
         "lower right); the up triangle a raised wedge with a gradient face (light apex and left side down to a shaded base), "
         "a crisp white edge and a cream band along its left side, a soft groove on both sides and a bright lip under it; the "
         "down triangle with light top rows, its face darkening toward a crisp white band along its right edge and a light "
         "line beside it; the orange pair is the pressed state (triangles one row nearer the bar)")
CHANGED_B = ("redo, rejected twice with no note; against round 2: stronger baked light (white edges and cream bands on the "
             "triangles, shaded bases, a bright lip, a deeper shadow crescent on the down plates) drawn as continuous "
             "gradients instead of flat bands; rounded plate corners; grooves that fade at both ends")


def deliver(which):
    w3.apply()
    states = []
    for st, y0, kind, k in CELLS:
        box = (0, y0, 35, y0 + 12)
        if which == "a":
            parts = up_a(y0, k) if kind == "up" else down_a(y0, k)
            it = measured(box, parts, f"trans-small-arrows A {st}")
        else:
            lay = up_b(y0, k) if kind == "up" else down_b(y0, k)
            it = c.item(box, lay, f"trans-small-arrows B {st}")
        states.append((st, box, it))
    eid = "trans-small-arrows--" + which
    p = auto.deliver(eid, TP, states, z=8, how="")
    w3.record(eid, "done", HOW_A if which == "a" else HOW_B, CHANGED_A if which == "a" else CHANGED_B, "")
    return p


if __name__ == "__main__":
    todo = sys.argv[1:] or ["a", "b"]
    for k, w in enumerate(todo):
        print(f"[{k + 1}/{len(todo)}] trans-small-arrows {w} {deliver(w)}", flush=True)
