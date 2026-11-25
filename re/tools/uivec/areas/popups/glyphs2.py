"""P41 popups fix round: info-button glyphs redrawn after Mariano's notes (cell coordinates as in glyphs_ib.py: x from the
cell's left, y from its top, pixel (i, j) covers [i, i+1) x [j, j+1)).

spline(...) samples a Catmull-Rom curve through read points into a plain toolkit "stroke" (so the scene stays standard JSON):
one smooth path, its joins round (distance to a dense polyline), its ends flat (the stroke's butt caps cut at the end points).
"""
import numpy as np

NAVY = [40, 48, 88]
TAN = [216, 192, 120]


def cr_points(pts, n=10):
    """Catmull-Rom through pts (ends repeated), n samples per span."""
    P = [pts[0]] + list(pts) + [pts[-1]]
    out = []
    for i in range(1, len(P) - 2):
        p0, p1, p2, p3 = (np.asarray(P[j], float) for j in (i - 1, i, i + 1, i + 2))
        for t in np.linspace(0, 1, n, endpoint=False):
            t2, t3 = t * t, t * t * t
            q = 0.5 * ((2 * p1) + (-p0 + p2) * t + (2 * p0 - 5 * p1 + 4 * p2 - p3) * t2 + (-p0 + 3 * p1 - 3 * p2 + p3) * t3)
            out.append([round(float(q[0]), 3), round(float(q[1]), 3)])
    out.append([float(pts[-1][0]), float(pts[-1][1])])
    return out


def spline(*pts, w=2.0, cap="butt", n=10):
    return {"t": "stroke", "pts": cr_points(pts, n), "w": w, "cap": cap}


def poly(*pts, smooth=0):
    S = {"t": "poly", "rings": [[list(p) for p in pts]]}
    if smooth:
        S["smooth"] = smooth
    return S


def circ(cx, cy, r):
    return {"t": "circle", "c": [cx, cy], "r": r}


def ell(cx, cy, rx, ry, rot=0.0):
    return {"t": "ellipse", "c": [cx, cy], "r": [rx, ry], "rot": rot}


def money():
    """A dollar sign, read row by row on the idle cell (dark runs of InfoButtons.pcx rows 200-240):
    - the S, one smooth stroke 2.7 px wide with flat ends: top terminal end at x 23.0 (rows 9-11), the top arc along y 10.4
      (rows 9.3-11.8) to x 13, down the left bowl (centre x 10.6 at row 14.5; edges 8.6 / 12.6), the spine across rows 16-18
      (row 16 dark x 10-14, row 17 x 12.5-22), the right bowl (centre x 22.1 at row 21), the bottom arc along y 25.2
      (rows 24-26.6) to the bottom terminal end at x 8.7;
    - the bar: x 16.05, 2.2 px wide, from y 4.5 to 31.4, drawn first (the S goes over it)."""
    S = spline((23.1, 10.2), (20.0, 10.45), (16.0, 10.5), (12.8, 11.0), (11.0, 12.6), (10.7, 14.6), (12.2, 16.4),
               (16.0, 17.4), (19.8, 18.4), (21.9, 20.2), (22.0, 22.2), (20.6, 24.0), (17.5, 25.2), (13.0, 25.35), (8.7, 25.0),
               w=2.7)
    return [
        ({"t": "stroke", "pts": [[16.05, 4.5], [16.05, 31.4]], "w": 2.2, "cap": "butt"}, NAVY, {}),
        (S, NAVY, {}),
    ]


def mv(S, dx, dy):
    return {"t": "move", "of": S, "d": [dx, dy]}


def flag():
    """A golf flag on a green, its light baked as layers (read on the idle cell's RGB rows 5-29):
    - the green: the first round's lens outline (glyphs_ib.flag), navy 283058;
    - its lit upper rim: the band between the green and the green moved 1.1 px up, lilac 9898b8 (row 18: 9090a0..a0a0a0 from
      x 10 to 24, b0b0d8 at the left tip);
    - under it a 1.3 px band of lighter navy along the top and right edges (rows 19-20: 505870 on the left rising to 7878a0 on the
      right; the right lobe's upper rows 586078): the green minus itself moved 0.5 px left and 1.25 px down, darker on the left, kept to the rows above y 21.2 (the green's lower edge has no light);
    - the pole: navy above the green's top edge (x 14.45, 1.3 px); in front of the dark green it is tan c8b078 (rows 18-20 at
      x 14) with its shadow side one column darker on the left, kept to the rows above y 21.2 (the green's lower edge has no light) (686060 at x 13);
    - the cup: an oval x 11.8-17.6, y 20.8-23.8, lit on its far wall: tan 988868..d8c078 on its top row, grey a09888 in the
      middle, blue-grey 606880 at the bottom (a linear paint over those three);
    - the pennant as in the first round."""
    import glyphs_ib
    g = glyphs_ib.flag()
    green, pennant = g[0][0], g[3][0]
    top = {"t": "sub", "a": mv(green, 0.0, -0.95), "b": green}
    side = {"t": "inter", "of": [{"t": "sub", "a": green, "b": mv(green, -0.5, 1.25)},
                                 {"t": "half", "p": [0.0, 21.2], "n": [0.0, 1.0]}]}
    cup = ell(14.7, 22.3, 2.6, 1.3)
    return [
        (green, [40, 48, 88], {}),
        (side, {"linear": {"a": [8.0, 19.5], "b": [28.0, 19.5], "stops": [[0.0, [64, 72, 108]], [1.0, [112, 118, 158]]]}},
         {"clip": green}),
        (top, [152, 152, 184], {}),
        ({"t": "stroke", "pts": [[14.45, 6.8], [14.45, 18.3]], "w": 1.3, "cap": "butt"}, NAVY, {}),
        (pennant, NAVY, {}),
        (cup, {"linear": {"a": [14.7, 21.0], "b": [14.7, 23.7],
                          "stops": [[0.0, [180, 164, 112]], [0.5, [150, 142, 124]], [1.0, [92, 100, 128]]]}}, {}),
        ({"t": "stroke", "pts": [[14.75, 18.2], [14.75, 21.2]], "w": 1.1, "cap": "butt"}, [204, 180, 120], {}),
        ({"t": "stroke", "pts": [[13.85, 18.2], [13.85, 21.0]], "w": 0.7, "cap": "butt"}, [104, 96, 96], {}),
    ]


GLOBE = (17.1, 17.95, 11.7)


def globe():
    """The globe of the first round (glyphs_ib.globe: four continents, ocean and green each lit by a bevel paint measured on
    their own pixels) on a corrected sphere, every continent clipped to it.
    Why the continent stood outside: the first round's glyph tuner replaced each continent's clip (the sphere) with the
    disc body, so the tall right continent (drawn to x 28.6) was cut only by the tan disc and showed past the ocean's edge.
    The sphere is also re-read: its edges are x 5.2 (row 17: 9898b8, mostly covered) and x 29.0 (x 28 = 303040, the dark limb;
    x 29 tan), y 6.5 (row 6: AA 9090c0) and y 29.4 (row 29: AA 808078), so c (17.1, 17.95), r 11.7 against the old
    (17.2, 17.65, 11.15) that left the art's limb uncovered on the right and at the bottom."""
    import glyphs_ib
    old = glyphs_ib.GLOBE
    glyphs_ib.GLOBE = GLOBE
    try:
        out = glyphs_ib.globe()
    finally:
        glyphs_ib.GLOBE = old
    sph = out[0][0]
    return [out[0]] + [(S, P, {"clip": sph}) for S, P, _ in out[1:]]


def ball(cx, cy, r, base, light, dark, hx=-0.35, hy=-0.4):
    """A small lit ball: the base disc, a dark rim on the lower right (the disc minus itself moved up-left), and a soft light
    spot on the upper left."""
    D = circ(cx, cy, r)
    return [
        (D, base, {}),
        ({"t": "sub", "a": D, "b": circ(cx - 0.45 * r, cy - 0.45 * r, r)}, dark, {"soft": 0.35, "clip": D}),
        (D, light, {"falloff": {"c": [cx + hx * r, cy + hy * r], "r": 0.95 * r, "pow": 1.2}}),
    ]


def pin():
    """A flag pin with a down arrow, redrawn from a new reading of the idle cell (row centroids of the dark pixels):
    - the down arrow: an upside-down triangle in navy lines 1.4 px wide, one closed path with round joins: a straight top side
      from the left ball (7.6, 18.35) to the right ball (21.5, 19.35) (column centres y 18.1-18.5 at x 11-14, 19.0 at x 17-19);
      straight left and right sides through the row centres (8.8, 19.5) .. (12.4, 25.5) and (21.7, 20.5) .. (19.3, 25.5); they
      run into the tip (16.0, 30.1), solid from row 26 down (rows 27-29 dark from x 13.4 to 17.9, row 30 x 14.3-16.9);
    - the pole: x 16.1, 1.55 px wide (rows 14-24 carry 1.3-1.6 px of dark), from y 7.8 down into the tip;
    - the pennant on the pole's right: top edge (17, 8.1) .. (19.5, 8.3) .. (23, 9.2), tip (26.1, 10.5), lower edge (23, 11.4) ..
      (20.5, 12.4) .. (17, 13.9); dark red 980830 with a bright wave along its middle row (row 10, x 20-23: c80840, e00048);
    - the left ball: c (7.5, 16.6), r 1.95, lit: 9898b8 on its upper middle, 606880 / 404868 on its lower right (rows 15-17);
    - the right ball: c (21.5, 19.3), r 1.45, navy with a grey-lilac top (row 18: 586078, 707880)."""
    NV = [40, 48, 88]
    # straight sides: the left one is x = 8.84 + 0.593 (y - 19.5) through its row centres, the right one x = 21.7 - 0.488 (y - 20.5);
    # both run into the solid tip, which hides the turn to the tip point
    tri = {"t": "stroke", "pts": [[7.6, 18.35], [21.5, 19.35], [21.95, 20.0], [18.5, 27.05], [16.0, 30.1], [13.3, 27.0],
                                  [8.3, 18.6], [7.6, 18.35]], "w": 1.4, "cap": "round"}
    tip = {"t": "grow", "of": poly((14.6, 26.9), (17.2, 26.9), (15.95, 29.3)), "d": 1.0}
    pennant = poly((17.0, 8.05), (19.5, 8.25), (23.0, 9.15), (26.1, 10.5), (23.0, 11.45), (20.5, 12.4), (17.0, 13.95))
    out = [
        (tri, NV, {}),
        (tip, NV, {}),
        ({"t": "stroke", "pts": [[16.1, 7.8], [16.1, 29.0]], "w": 1.55, "cap": "butt"}, NV, {}),
        (pennant, [160, 10, 50], {}),
        (ell(21.6, 10.35, 2.4, 0.6, -6.0), [212, 4, 66], {"soft": 0.45}),
    ]
    out += ball(7.5, 16.6, 1.95, [100, 104, 140], [160, 160, 196], [56, 64, 100])
    out += ball(21.5, 19.3, 1.45, [40, 48, 88], [112, 120, 144], [32, 40, 80], hx=0.0, hy=-0.55)
    return out


def tiles():
    """Stacked terrain tiles: four IDENTICAL slabs (the first round drew each block with its own corners, so the four came out
    irregular). One slab = a top rhombus with edges a = (5.7, 2.0) and b = (-6.7, 3.25) from its top vertex, and two side faces
    h = 3.3 px tall under its front edges (C's left face reaches row 21-22, D's right face row 25-26). Top vertices read on the face map of the idle cell (light top a898c8 / b0b0d8, mid
    left face 7860b0 / 8870b8, dark right face 382858 / 403060): back A (15.0, 8.4), right B (22.0, 11.7), left C (11.5, 14.8),
    front D (19.3, 17.6); drawn back to front (A, B, C, D) so each slab hides the one behind it. Lit from the upper left: a light
    line along the top face's front-left edge (the art's paler b0b0d8 / c0c0e0 pixels there) and a darker foot row on the left
    face (604898)."""
    a, b, h = (5.7, 2.0), (-6.7, 3.25), 3.3
    TOP, LEFT, RIGHT = [168, 152, 200], [120, 96, 176], [56, 40, 88]
    out = []
    for tx, ty in ((15.0, 8.4), (22.0, 11.7), (11.5, 14.8), (19.3, 17.6)):
        T = (tx, ty)
        R = (tx + a[0], ty + a[1])
        L = (tx + b[0], ty + b[1])
        B = (tx + a[0] + b[0], ty + a[1] + b[1])
        top = poly(T, R, B, L)
        left = poly(L, B, (B[0], B[1] + h), (L[0], L[1] + h))
        right = poly(B, R, (R[0], R[1] + h), (B[0], B[1] + h))
        out += [
            (left, LEFT, {}),
            ({"t": "stroke", "pts": [[L[0], L[1] + h - 0.35], [B[0], B[1] + h - 0.35]], "w": 0.7, "cap": "butt"}, [96, 72, 152],
             {"clip": left}),
            (right, RIGHT, {}),
            (top, TOP, {}),
            ({"t": "stroke", "pts": [list(L), list(B)], "w": 1.3, "cap": "butt"}, [190, 188, 226], {"clip": top}),
        ]
    return out


def trophy():
    """A trophy cup, centred and mirrored (Mariano: "center it").
    Measured: the disc's centre is (17.23, 17.90) in the idle cell; the art's cup has its axis at x 16.8 (bowl 11.3-22.4,
    stem 15.4-17.4, foot 12.6-21.0), its handles reach x 6.3 and 26.35 (10.5 and 9.55 px from that axis) and its whole box
    is x 6-26, y 10-30: centre (16.0, 20.0), 1.2 px left of and 2.1 px below the disc's centre (the other nine buttons' glyph
    boxes sit within 1 px of their centre). The first round copied the unequal handles, so it read off-centre.
    Here: one profile mirrored about the disc's axis x = AX, both handles the same ring (outer reach 9.95 px from the axis, the
    mean of the two; outer y 10.0-19.0, hole about 2.5 x 4.6 px: centre line 6.55 px out, radii 2.4 x 3.45, line 1.8), and the whole cup moved up DY so its box is centred on the disc (y 8.3 .. 27.9).
    Profile (from the art's rows, relative to the mouth at y 0): half width 5.55 to 5.0 down, 4.3 at 7, 3.2 at 8, 2.2 at 9,
    the stem 0.9 from 10 to 14, a foot flaring to 4.0 at 15-16.7 with a rounded bottom at 17.6. The mouth's first row is a
    pale lip (144, 144, 160) and the next a lighter navy (the inside), as in the art."""
    AX, DY = 17.23, -1.7
    y0 = 11.9 + DY

    def Y(v):
        return y0 + v
    bowl = {"t": "lathe", "x": AX, "prof": [[Y(0.0), 5.55], [Y(1.5), 5.65], [Y(5.0), 5.6], [Y(6.2), 5.2], [Y(7.1), 4.4],
                                            [Y(8.0), 3.3], [Y(8.9), 2.15], [Y(9.6), 1.0], [Y(10.0), 0.9], [Y(14.1), 0.9]]}
    foot = {"t": "lathe", "x": AX, "prof": [[Y(13.9), 0.9], [Y(14.6), 2.6], [Y(15.1), 4.0], [Y(16.6), 4.1], [Y(17.2), 3.4],
                                            [Y(17.6), 2.4]], "smooth": 1}
    off, rx, ry, w = 6.55, 2.4, 3.45, 1.8
    hl = {"t": "outline", "of": ell(AX - off, Y(2.75), rx, ry), "w": w}
    hr = {"t": "outline", "of": ell(AX + off, Y(2.75), rx, ry), "w": w}
    lip = {"t": "stroke", "pts": [[AX - 5.15, Y(0.5)], [AX + 5.15, Y(0.5)]], "w": 1.0, "cap": "butt"}
    inner = {"t": "stroke", "pts": [[AX - 5.35, Y(1.5)], [AX + 5.35, Y(1.5)]], "w": 1.0, "cap": "butt"}
    return [
        (hl, NAVY, {}),
        (hr, NAVY, {}),
        (bowl, NAVY, {}),
        (foot, NAVY, {}),
        (inner, [80, 88, 112], {"clip": bowl, "alpha": 0.7}),
        (lip, [144, 144, 160], {"clip": bowl, "alpha": 0.75}),
    ]


GLYPHS = {"money": money, "flag": flag, "globe": globe, "pin": pin, "tiles": tiles, "trophy": trophy}
