"""P41 hud_panels fix round (2026-11-24): the ten 16 px mood faces (member-mood-faces) and the two extra rows
(member-mood-faces-extra, pixel copies of faces 5-8), redone from per-face colour maps of M 450,100 (one letter per
distinct colour, read pixel by pixel).

Every face: a disc round (8, 8) r 8 in the cell; a one-pixel dark rim in the face's own dark tone; the body lit (its
colour measured in rings round the face's light centre: the centroid of its brightest body pixels); then its parts:
 1 open grin full of teeth (white upper row, grey line, white lower middle), eyes 2 x 5 with a grey glint on the inner side
 2 smile line with corners up and a small open mouth with teeth under it; same eyes
 3 smile line (corners up, 7 px flat bottom); eyes 2 x 4
 4 smile line thicker in the middle; eyes 2 x 4 closer together
 5 shallow smile (ends higher than the middle); round eyes 3 x 4
 6 flat straight mouth; wide eyes 3 x 3
 7 frown (middle up, corners down); eyes 3 x 3
 8 open frown (an arch with its corners down and a short lower lip), angry brows slanting down to the middle; eyes 3 x 3
 9 wide frown arch; slanted angry eyes
10 grimace: two rows of teeth (alternating white and grey) in a dark outline; slanted eyes with a grey glint; angry brows;
   two horns on top of the head (the disc's top edge is notched between x 5 and 10)
Coordinates in the strings below are cell-relative (0..16).
"""
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import common as C  # noqa: E402
import kit as K  # noqa: E402
import el_bubbles as EB  # noqa: E402
import el_marks as EM  # noqa: E402
import figs  # noqa: E402
import fit  # noqa: E402
from kit import U, rect, circle, ellipse, poly, sub, inter  # noqa: E402,F401

limb = figs.limb
AXIS = 8.0    # every face is drawn mirror-symmetric about the cell's x 8 (the pixel 7 | 8 boundary)


def mir(pts):
    return [(2 * AXIS - p[0], p[1]) + tuple(p[2:]) for p in pts]


def eyes(cx, cy, rx, ry, rot=0.0):
    return U(ellipse(cx, cy, rx, ry, rot), ellipse(2 * AXIS - cx, cy, rx, ry, -rot))


def sym_limb(pts):
    return U(limb(pts), limb(mir(pts)))


def face_parts(n):
    """[(name, shape in cell coords, paint key)] paint keys: 'dark' (the face's darkest tone), 'teeth', 'grey',
    'brow', 'mid' (the face's dark olive / mouth tone)."""
    P = []
    if n in (1, 2):
        P.append(("eyes", eyes(5.0, 4.5, 1.05, 2.55), "dark"))
        P.append(("glints", U(rect(5.0, 4.0, 6.0, 5.0, 0.3), rect(10.0, 4.0, 11.0, 5.0, 0.3)), "grey"))
    if n == 1:
        mouth = poly((2.2, 9.6), (3.6, 8.9), (5.0, 8.1), (11.0, 8.1), (12.4, 8.9), (13.8, 9.6), (12.4, 11.0), (11.2, 12.2),
                     (10.4, 12.95), (5.6, 12.95), (4.8, 12.2), (3.6, 11.0), smooth=1)
        teeth = poly((5.0, 9.0), (11.0, 9.0), (11.0, 10.95), (10.0, 11.95), (6.0, 11.95), (5.0, 10.95))
        P += [("mouth", mouth, "mid"), ("teeth", teeth, "teeth"), ("teeth line", rect(5.0, 10.0, 11.0, 11.0), "grey")]
    if n == 2:
        P += [("mouth", poly((4.3, 10.3), (11.7, 10.3), (11.4, 11.6), (10.5, 12.95), (5.5, 12.95), (4.6, 11.7), smooth=1), "mid"),
              ("smile", sym_limb([(2.2, 9.3, 1.05), (3.4, 10.4, 1.05), (5.5, 10.5, 1.05), (AXIS, 10.5, 1.05)]), "mid"),
              ("teeth", rect(6.0, 11.0, 10.0, 12.0, 0.2), "teeth"),
              ("teeth ends", U(rect(6.0, 11.0, 6.9, 12.0), rect(9.1, 11.0, 10.0, 12.0)), "grey")]
    if n == 3:
        P += [("eyes", eyes(5.0, 5.0, 1.0, 2.05), "dark"),
              ("smile", sym_limb([(2.3, 9.4, 1.05), (3.6, 10.5, 1.05), (5.2, 11.45, 1.1), (AXIS, 11.5, 1.1)]), "mid")]
    if n == 4:
        P += [("eyes", eyes(5.0, 5.0, 1.0, 2.0), "dark"),
              ("smile", sym_limb([(3.4, 9.4, 1.05), (4.6, 10.4, 1.3), (6.4, 11.0, 1.55), (AXIS, 11.15, 1.6)]), "mid")]
    if n == 5:
        P += [("eyes", eyes(5.1, 5.9, 1.45, 2.0), "dark"),
              ("mouth", sym_limb([(1.6, 10.55, 1.0), (4.6, 10.65, 1.0), (6.2, 11.45, 1.05), (AXIS, 11.5, 1.05)]), "mid")]
    if n == 6:
        P += [("eyes", eyes(5.0, 5.5, 1.55, 1.5), "dark"),
              ("mouth", rect(3.0, 10.0, 13.0, 11.0, 0.45), "dark")]
    if n == 7:
        P += [("eyes", eyes(4.6, 4.5, 1.45, 1.5), "dark"),
              ("mouth", sym_limb([(2.4, 11.4, 1.0), (3.5, 10.5, 1.05), (5.0, 9.6, 1.05), (AXIS, 9.45, 1.05)]), "mid")]
    if n == 8:
        P += [("eyes", eyes(5.0, 4.5, 1.4, 1.5), "dark"),
              ("brows", sym_limb([(3.6, 1.4, 1.0), (6.6, 2.5, 1.05)]), "brow"),
              ("mouth", sym_limb([(2.9, 11.6, 1.0), (3.8, 10.6, 1.0), (5.0, 9.6, 1.05), (6.4, 8.55, 1.05), (AXIS, 8.5, 1.05)]), "mid"),
              ("lower lip", limb([(6.6, 10.5, 1.0), (9.4, 10.5, 1.0)]), "mid")]
    if n == 9:
        P += [("eyes", U(poly((4.0, 4.0), (5.0, 3.0), (7.0, 3.0), (7.0, 5.0), (6.0, 6.0), (4.0, 6.0), smooth=1),
                         poly(*[(2 * AXIS - x, y) for x, y in ((4.0, 4.0), (5.0, 3.0), (7.0, 3.0), (7.0, 5.0), (6.0, 6.0),
                                                                (4.0, 6.0))], smooth=1)), "dark"),
              ("mouth", sym_limb([(2.4, 11.5, 1.05), (3.4, 10.4, 1.05), (4.6, 9.3, 1.05), (5.8, 8.5, 1.05), (AXIS, 8.5, 1.05)]), "mid"),
              ("lip shade", rect(6.0, 7.0, 10.0, 8.0, 0.4), "brow")]
    if n == 10:
        P += [("eyes", U(poly((3.0, 4.0), (5.0, 4.0), (7.0, 6.0), (7.0, 7.0), (5.0, 7.0), (3.6, 5.2), smooth=1),
                         poly(*[(2 * AXIS - x, y) for x, y in ((3.0, 4.0), (5.0, 4.0), (7.0, 6.0), (7.0, 7.0), (5.0, 7.0),
                                                                (3.6, 5.2))], smooth=1)), "dark"),
              ("glints", U(rect(5.0, 6.0, 6.0, 7.0, 0.3), rect(10.0, 6.0, 11.0, 7.0, 0.3)), "grey"),
              ("brows", sym_limb([(4.4, 2.6, 1.0), (6.6, 3.5, 1.1)]), "brow"),
              ("mouth", poly((3.0, 10.6), (4.4, 9.0), (5.4, 8.05), (10.6, 8.05), (11.6, 9.0), (13.0, 10.6), (12.0, 11.4),
                             (10.6, 12.0), (5.4, 12.0), (4.0, 11.4), smooth=1), "mid"),
              ("teeth", poly((4.0, 9.95), (5.0, 9.0), (11.0, 9.0), (12.0, 9.95), (12.0, 11.0), (4.0, 11.0)), "grey")]
        whites = [rect(x, 9.0, x + 1.0, 10.0) for x in (6.0, 8.0, 10.0)] + [rect(x, 10.0, x + 1.0, 11.0) for x in (5.0, 7.0, 9.0, 11.0)]
        P.append(("teeth white", U(*whites), "teeth"))
    return P


HORNS = [(2.0, 0.0, 5.0, 2.0), (11.0, 0.0, 14.0, 2.0)]


def silhouette(n):
    disc = circle(AXIS, 8.0, 8.0)
    if n != 10:
        return disc
    # face 10: the top edge is notched: horn tips at x 2-4 and 11-13 on row 0, the edge between them a row lower
    notch = poly((5.2, -0.5), (10.8, -0.5), (10.2, 0.9), (5.8, 0.9))
    horns = U(poly((2.2, 1.6), (3.6, 0.05), (5.2, 0.2), (5.4, 1.6)), poly((10.6, 1.6), (10.8, 0.2), (12.4, 0.05), (13.8, 1.6)))
    return U(sub(disc, notch), horns)


def measure_body(key, ox, oy, parts):
    """The face's body paint: a radial paint round its light centre (centroid of the brightest quarter of body pixels),
    stops = the median colour of body pixels in 1.5 px rings."""
    import sdf
    rgb, m = C.sheet(key)
    jj, ii = np.mgrid[0:16, 0:16]
    X, Y = ii + 0.5, jj + 0.5
    a = rgb[oy:oy + 16, ox:ox + 16]
    inside = np.hypot(X - AXIS, Y - 8.0) < 6.6
    feat = np.zeros((16, 16), bool)
    for _, s, _ in parts:
        feat |= sdf.sd({"t": "grow", "of": s, "d": 0.8}, X, Y) < 0
    body = inside & ~feat
    L = fit.lum(a)
    thr = np.quantile(L[body], 0.75)
    br = body & (L >= thr)
    lx, ly = float(X[br].mean()), float(Y[br].mean())
    d = np.hypot(X - lx, Y - ly)
    stops = []
    for r0 in np.arange(0, 12, 1.5):
        sel = body & (d >= r0) & (d < r0 + 1.5)
        if sel.sum() >= 2:
            stops.append([round(float(r0 + 0.75), 2), [round(float(v), 1) for v in np.median(a[sel], axis=0)]])
    R = stops[-1][0]
    return {"radial": {"c": [ox + lx, oy + ly], "r": R, "stops": [[s[0] / R, s[1]] for s in stops]}}


def face_layers(key, ox, oy, n):
    rgb, m = C.sheet(key)
    a = rgb[oy:oy + 16, ox:ox + 16]
    L = fit.lum(a)
    parts = face_parts(n)
    sil = silhouette(n)
    jj, ii = np.mgrid[0:16, 0:16]
    rd = np.hypot(ii + 0.5 - AXIS, jj + 0.5 - 8.0)
    ring = (rd > 6.9) & (rd < 7.9) & m[oy:oy + 16, ox:ox + 16]
    rim = [round(float(v), 1) for v in np.median(a[ring & (L <= np.quantile(L[ring], 0.3))], axis=0)]
    dark = [round(float(v), 1) for v in np.median(a[L <= np.quantile(L[rd < 7], 0.04)], axis=0)]
    col = {"dark": dark}

    def at(px):     # median colour of the listed pixels (cell-relative)
        return [round(float(v), 1) for v in np.median(np.array([a[y, x] for x, y in px]), axis=0)]
    TEETH = {1: [(6, 9), (7, 9), (8, 9), (9, 9)], 2: [(7, 11), (8, 11)], 10: [(6, 9), (8, 9), (5, 10), (7, 10)]}
    GREY = {1: [(5, 4), (10, 4)], 2: [(5, 4), (10, 4)], 10: [(5, 6), (10, 6), (5, 9), (7, 9)]}
    MID = {1: [(6, 8), (7, 8), (8, 8), (6, 12), (7, 12)], 2: [(4, 10), (5, 10), (6, 10), (7, 10)],
           3: [(5, 11), (6, 11), (7, 11), (8, 11)], 4: [(6, 11), (7, 11), (8, 11), (5, 10)], 5: [(5, 11), (7, 11), (8, 11)],
           7: [(4, 9), (5, 9), (6, 9), (7, 9)], 8: [(6, 8), (7, 8), (8, 8)], 9: [(5, 8), (6, 8), (7, 8), (8, 8)],
           10: [(6, 8), (7, 8), (8, 8), (9, 8)]}
    BROW = {8: [(5, 2), (6, 2), (9, 2), (10, 2)], 9: [(6, 7), (7, 7), (8, 7)], 10: [(5, 3), (6, 3), (9, 3), (10, 3)]}
    if n in TEETH:
        col["teeth"] = at(TEETH[n])
    if n in GREY:
        col["grey"] = at(GREY[n]) if n != 1 else at([(5, 10), (6, 10), (7, 10), (8, 10)])
    if n in MID:
        col["mid"] = at(MID[n])
    if n in BROW:
        col["brow"] = at(BROW[n])
    glint = {1: [(5, 4), (10, 4)], 2: [(5, 4), (10, 4)], 10: [(5, 6), (10, 6)]}
    body = measure_body(key, ox, oy, parts)
    out = [K.lay(K.shift(sil, ox, oy), rim),
           K.lay(K.shift({"t": "grow", "of": sil, "d": -0.85}, ox, oy), body)]
    for name, s, pk in parts:
        c = col[pk]
        if name == "glints":
            c = at(glint[n])
        out.append({"name": name, "shape": K.shift(s, ox, oy), "fill": c})
    return out


# one 16 x 16 cell per face, so the comparison image shows each face at 8x (the strip itself is 160 x 16)
C.CELLS["member-mood-faces"] = [(f"face {k + 1}", "M", (450 + 16 * k, 100, 16, 16)) for k in range(10)]
C.CELLS["member-mood-faces-extra"] = ([(f"row 1 face {k + 5}", "M", (631 + 16 * k, 127, 16, 16)) for k in range(4)] +
                                      [(f"row 2 face {k + 5}", "M", (597 + 16 * k, 166, 16, 16)) for k in range(4)])


def build_faces(el):
    scenes = {}
    for st, key, rct in C.CELLS[el]:
        x0, y0, w, h = rct
        n = int(st.split()[-1])
        L = [K.lay(rect(x0, y0, x0 + w, y0 + h), EM.cell_bg(key, rct))]
        L += face_layers(key, x0, y0, n)
        scenes[st] = [K.item(L, box=[x0, y0, x0 + w, y0 + h])]
    return scenes


EB.BUILD.update({"member-mood-faces": build_faces, "member-mood-faces-extra": build_faces})
