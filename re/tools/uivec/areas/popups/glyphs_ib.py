"""P41 popups area: the info buttons' glyphs, authored from the art read on its pixel grid (cell coordinates: x from the cell's
left, y from its top; pixel (i, j) covers [i, i+1) x [j, j+1)). Each function returns a list of layers (shape, colour) to draw
over the body, already in cell coordinates; infobuttons.py moves them to the cell and clips them to the body.
"""
NAVY = [40, 48, 88]
TAN = [216, 192, 120]


def seg(*pts, w=1.0, cap="butt", smooth=0):
    S = {"t": "stroke", "pts": [list(p) for p in pts], "w": w, "cap": cap}
    if smooth:
        S["smooth"] = smooth
    return S


def poly(*pts, smooth=0):
    S = {"t": "poly", "rings": [[list(p) for p in pts]]}
    if smooth:
        S["smooth"] = smooth
    return S


def ell(cx, cy, rx, ry, rot=0.0):
    return {"t": "ellipse", "c": [cx, cy], "r": [rx, ry], "rot": rot}


def flag():
    """A golf flag on a green. Read on rows 5-29 of the idle cell:
    - pole: one navy column at x 14 (x 13 a quarter covered), from y 6.8 (the flag's top) to 17.6 where it meets the green;
    - flag: a small waving pennant to the right of the pole top: left half x 14-18 spans y 6.5-9, the right half x 18-22 sits a
      pixel lower (y 7.5-9.1), and its lower edge dips in a short tail at x 18-19.6 down to y 10.7;
    - the green: one dark blob, a lens from the left tip (4.9, 23.5) to the right tip (30, 21.7) with its top edge at y 19.0-19.5,
      whose left half is deeper (the green's near side): the bottom runs from the left tip round to (16, 28.8) and up a nearly
      straight right side at x 19.4-20.6 to the lens' lower edge (y 23.5-23.9);
    - a 1 px lilac light line along the green's top edge, x 10-27;
    - the pole below the green's top edge and the cup, in the body's tan: a short light stroke x 14.4 y 18-20.8 and a small light
      oval at (14.8, 21.6)."""
    green = poly((8.5, 20.4), (10.5, 19.6), (12.2, 19.1), (16, 19.15), (21, 19.35), (25, 19.5), (27.2, 19.85), (28.7, 20.35),
                 (29.7, 21.0), (30.0, 21.8), (29.0, 22.9), (27.5, 23.4), (24, 23.6), (21.0, 23.8), (19.4, 24.0), (20.3, 25.8),
                 (20.6, 27.2), (19.6, 28.4), (17.5, 28.8), (14.5, 28.9), (11.5, 28.6), (9.4, 27.7), (7.7, 26.6), (6.3, 25.4),
                 (5.2, 24.3), (4.8, 23.3), (5.4, 22.3), (6.8, 21.3), smooth=2)
    pennant = poly((14.0, 6.45), (16.0, 6.4), (17.8, 6.7), (20.0, 7.3), (21.9, 7.55), (22.2, 8.4), (21.0, 9.1), (19.9, 9.2),
                   (19.6, 10.7), (18.2, 10.7), (17.9, 9.0), (14.0, 8.9))
    return [
        (green, NAVY, {}),
        (seg((10.0, 18.55), (14.0, 18.35), (19.0, 18.4), (23.5, 18.65), (27.3, 19.25), w=0.9, smooth=2), [164, 164, 204], {"alpha": 0.7}),
        (seg((14.45, 6.8), (14.45, 17.7), w=1.25), NAVY, {}),
        (pennant, NAVY, {}),
        (seg((14.45, 18.0), (14.55, 20.6), w=1.0), TAN, {"alpha": 0.8}),
        (ell(14.9, 21.65, 1.9, 1.05), TAN, {"alpha": 0.75}),
    ]


def notice():
    """A speech bubble with an exclamation mark. The bubble is one navy ellipse (left edge x 4.2, right 29.0, top 7.7, read row
    by row: c (16.6, 15.3), r (12.4, 7.6)); its tail leaves the bottom just right of the middle and hooks down and to the left
    like a comma: from (17.9, 22.3) through x 18.6 at row 25 to the tip (14.3, 28.7), about 2.6 px wide where it leaves the
    bubble and 1.3 px at the tip. The '!' is cut out of the bubble in the body's tan: a bar 1.6 px wide from y 9.6 to 16.4
    (x 16.0 at the top, 16.6 lower down) and a dot of radius 0.95 at (15.8, 19.1)."""
    bubble = ell(16.6, 15.3, 12.4, 7.6)
    tail = {"t": "qstroke", "p": [[17.8, 21.8], [19.7, 27.0], [14.2, 28.7]], "w": [2.7, 1.3]}
    return [
        (bubble, NAVY, {}),
        (tail, NAVY, {}),
        (seg((16.05, 9.6), (16.55, 16.4), w=1.55), TAN, {"alpha": 0.9}),
        ({"t": "circle", "c": [15.85, 19.05], "r": 0.95}, TAN, {"alpha": 0.85}),
    ]


def _golfer(dx, dy):
    """One golfer, as the figures on the pair button are drawn (the same figure three times; read on the front left one, head
    at (12.6, 15.9)): a ring head (radius 1.55 to the line's middle, line 1.1 px), a club held level behind the neck along the
    head's top (y 14.35, from 3.7 px right of the head to 7.6 px left of it, ending in a club head turned down to (3.4, 16.3)),
    a body line 1.35 px wide from the head down to the hip (13.4, 23.6), a back leg out to the left and down to (7.9, 27.7) and
    a front leg down to (10.8, 27.3) with the foot along y 27.55 to x 14.6. The art's lines are one pixel."""
    def m(x, y):
        return (x + dx, y + dy)
    return [
        ({"t": "outline", "of": {"t": "circle", "c": list(m(12.6, 15.85)), "r": 1.55}, "w": 1.1}, NAVY, {}),
        (seg(m(16.3, 14.35), m(5.0, 14.35), m(3.4, 16.3), w=0.85), NAVY, {"alpha": 0.9}),
        (seg(m(13.25, 17.4), m(13.45, 23.6), w=1.35), NAVY, {}),
        (seg(m(13.3, 23.5), m(9.3, 24.3), m(7.9, 27.7), w=1.1), NAVY, {}),
        (seg(m(13.3, 23.7), m(12.2, 25.7), m(10.8, 27.3), w=1.1), NAVY, {}),
        (seg(m(10.4, 27.55), m(14.6, 27.55), w=1.0), NAVY, {}),
    ]


def pair():
    """Golfers: the same figure three times, two in front (heads at (12.6, 15.9) and 11.9 px to the right of it) and one behind,
    6 px right of the left one and 6.95 px higher (its legs end among the front heads). The copies' pixels agree row for row
    (rows 14-17 of the front two; rows 7-20 of the back one against rows 14-27 of the front left one)."""
    return _golfer(6.0, -6.95) + _golfer(0.0, 0.0) + _golfer(11.9, 0.0)


RED = [176, 8, 56]


def pin():
    """A flag pin with a down arrow. Read on the idle cell:
    - pole: navy, x 15.4-17.0 at the top and 15.2-16.7 lower down, from y 7.6 into the arrow head;
    - flag: a red pennant on the pole's right, from the pole (x 17, y 7.8-13.6) to its tip (25.6, 10.5); crimson edges, a
      brighter red core along its middle row (x 19-22, y 10);
    - the arrow: an upside-down triangle in navy lines about 1.3 px wide: the top line y 18.3 from the ball to x 23, the left side
      from (8.3, 19.0) to (13.0, 26.0), the right side from (22.6, 18.4) to (17.5, 26.0) (2.5 px thick at its top corner), and
      a filled head pointing down: (12.4, 25.6), (18.9, 25.6), tip (15.7, 31.4);
    - the ball: a small lilac ball with a dark edge at the left end of the top line, c (7.3, 16.8), r 1.9."""
    return [
        (seg((16.15, 7.6), (15.95, 27.0), w=1.5), NAVY, {}),
        (poly((16.9, 7.8), (19.5, 8.6), (25.6, 10.3), (25.6, 10.7), (19.5, 12.3), (16.9, 13.6)), RED, {}),
        (poly((18.6, 9.7), (23.6, 10.4), (18.6, 11.2)), [214, 2, 68], {"alpha": 0.9}),
        (seg((6.0, 18.3), (16.0, 18.25), (21.6, 18.45), w=1.35), NAVY, {}),
        (seg((8.3, 18.9), (13.1, 26.1), w=1.35), NAVY, {}),
        (seg((22.2, 19.4), (17.4, 26.1), w=1.35), NAVY, {}),
        ({"t": "circle", "c": [21.75, 19.85], "r": 1.45}, NAVY, {}),
        ({"t": "grow", "of": poly((13.8, 26.6), (17.6, 26.6), (15.6, 29.9)), "d": 0.95}, NAVY, {}),
        ({"t": "circle", "c": [7.3, 16.85], "r": 1.95}, [104, 106, 140], {}),
        ({"t": "circle", "c": [7.1, 16.5], "r": 1.05}, [152, 152, 188], {}),
    ]


def money():
    """A dollar sign: an S stroke 3 px wide with flat ends (top end cut at x 23 around y 10.5, bottom end at x 8.3 around y 24.5)
    whose bowls were read row by row (top arc y 8.5-11.5, left side x 9.4-12.6 at rows 12-15, the middle crossing rows 15-18,
    right side x 20.5-23.7 at rows 19-22, bottom arc rows 23-26), and a vertical bar through it, x 16.0, 2.2 px wide, from y 4.6
    to 31.4. The light gaps between bar and S (x 13.5 at rows 12-14, x 18.5 at rows 19-22) come out of the geometry."""
    S = seg((23.2, 10.5), (19.5, 9.9), (14.5, 10.0), (11.3, 11.6), (10.6, 14.2), (12.6, 16.3), (17.5, 17.4), (21.2, 19.2),
            (22.0, 22.3), (19.6, 24.7), (14.5, 25.4), (8.2, 24.4), w=3.0, smooth=3)
    return [
        (seg((16.0, 4.6), (16.0, 31.4), w=2.2), NAVY, {}),
        (S, NAVY, {}),
    ]


ORANGE = [248, 110, 30]


def chart():
    """An orange line chart: navy axes (a vertical line x 6.55-8.85 from y 8.3 down to the corner, a horizontal line y 24.7-26.5
    from x 6.5 to 27.9, square ends) and an orange zigzag 2 px wide with flat ends through the centres read row by row:
    (9.9, 23.8) up to (10.2, 19.7), (12.6, 17.6), down to (14.8, 19.8), up to (20.0, 10.9), down to (22.5, 14.8), up to
    (25.7, 9.2)."""
    return [
        (seg((7.7, 8.3), (7.7, 26.5), w=2.3), NAVY, {}),
        (seg((6.55, 25.6), (27.9, 25.6), w=1.85), NAVY, {}),
        (seg((9.9, 23.9), (10.2, 19.7), (12.6, 17.6), (14.8, 19.8), (20.0, 10.9), (22.5, 14.8), (25.8, 9.1), w=2.0), ORANGE, {}),
    ]


GREEN = [0, 200, 64]


def checklist():
    """A green check in a box. The box: a navy square outline 1.5-1.8 px (outer edges x 9.2 / 25.0, y 10.5 / 26.0), square
    corners. The check, drawn over it: a short thick arm from the upper left (top edge y 19.8 from x 6.2 to 9.0) down to the
    bottom point (11.8, 28.6), and a long arm from there up to a flat top end at y 8.8 (x 25.3-28.0); the long arm's lower edge
    runs from (28.0, 11.5) to (12.6, 28.6) and its upper edge from the notch (11.2, 21.6) to (25.3, 8.8). A thin pale green
    light line runs along both arms' upper-left edges."""
    box = {"t": "rrect", "c": [17.1, 18.25], "half": [7.1, 7.0], "r": 0.3}
    check = poly((6.1, 19.8), (9.1, 19.7), (11.2, 21.6), (25.3, 8.8), (28.05, 8.8), (28.05, 11.4), (12.7, 28.7), (11.1, 28.7),
                 (9.9, 26.6), (6.9, 23.2), (6.0, 21.6))
    light = seg((6.4, 20.4), (9.0, 20.3), (11.3, 22.4), (25.6, 9.4), w=0.9)
    return [
        ({"t": "outline", "of": box, "w": 1.65}, NAVY, {}),
        (check, GREEN, {}),
        (light, [150, 228, 168], {"alpha": 0.85, "clip": check}),
    ]


def trophy():
    """A trophy cup in navy. Read row by row on the idle cell:
    - the bowl: a profile mirrored about x 16.75 (the middle of every row of bowl, stem and base): flat mouth at y 12.0, half
      width 5.6 down to y 17, then rounding in (4.5 at y 19, 3.3 at y 20, 2.2 at y 21) into the stem;
    - the mouth: its first row (y 12-13) is a pale grey-lilac line, the next row a lighter navy (the cup's inside);
    - two ring handles whose inner side the bowl hides; they are not mirror images in the art: the left one reaches out to
      x 6.2 (hole x 8.4-11.0), the right one to x 26.2 (hole x 22.4-24.2); both from y 10.6 to 18.8 with the hole y 12.4-17.0,
      line 1.8-2.2 px;
    - the stem: 1.6 px wide from y 21.5 to 26.5;
    - the base: a flat foot x 12.6-21.0, y 26.6-29.4, rounded ends."""
    ax = 16.75
    bowl = {"t": "lathe", "x": ax, "prof": [[12.0, 5.55], [13.5, 5.65], [17.0, 5.6], [18.2, 5.2], [19.1, 4.5], [20.0, 3.4],
                                            [20.9, 2.2], [21.5, 1.0], [21.9, 0.8], [26.8, 0.8]], "fixed": True}
    base = {"t": "rrect", "c": [16.8, 28.0], "half": [4.2, 1.4], "r": 1.3}
    hl = {"t": "outline", "of": ell(9.65, 14.7, 2.35, 3.2), "w": 2.0, "fixed": True}
    hr = {"t": "outline", "of": ell(23.3, 14.7, 1.9, 3.2), "w": 1.9, "fixed": True}
    lip = seg((11.6, 12.5), (21.9, 12.5), w=1.0)
    lip["fixed"] = True
    inner = seg((11.4, 13.5), (22.1, 13.5), w=1.0)
    inner["fixed"] = True
    return [
        (hl, NAVY, {}),
        (hr, NAVY, {}),
        (bowl, NAVY, {}),
        (base, NAVY, {}),
        (inner, [80, 88, 112], {"clip": bowl, "alpha": 0.7}),
        (lip, [144, 144, 160], {"clip": bowl, "alpha": 0.75}),
    ]


GLOBE = (17.2, 17.65, 11.15)       # the sphere on the globe button (cell coordinates): left edge x 6.05, right 28.35, top 6.5


def globe():
    """A green and blue globe: a sphere (c, r = GLOBE, read on its four sides) whose ocean is lit from the upper left (near
    white at (16, 15), lilac, then deep blue-violet toward the lower left edge) and four continents in green lit the same way
    (light green under the highlight, bright green on the left, dark green on the right):
    - the upper left one from (10.2, 7.8) down the left side x 8 to (9.6, 18.2), its right edge stepping down from an arm
      that reaches up to (18.7, 7.9);
    - a tall one on the right edge, from (21.5, 7) widening to x 22-28 by row 13, with a bay of ocean at rows 15-16 x 23-27, and a
      thin tail at x 25-26 down to row 25;
    - a round one at the bottom, rows 21-28, x 12.3-20.5;
    - a speck at (10.4, 18.9).
    Both paints are the toolkit's bevel paint on the sphere (colour by depth and by direction), measured on the idle cell's
    ocean and green pixels (paint.fit_bevel)."""
    import numpy as np
    import common as c
    import paint
    cx, cy, R = GLOBE
    sph = {"t": "circle", "c": [cx, cy], "r": R}
    box = (0, 400, 46, 440)
    a = c.art("InfoButtons.pcx", box)
    jj, ii = np.mgrid[0:40, 0:46]
    x, y = ii + 0.5, jj + 0.5 + 400
    inside = np.hypot(x - cx, y - cy - 400) < R - 0.7
    green = inside & (a[..., 1] > a[..., 2] + 40) & (a[..., 1] > a[..., 0] + 40)
    ocean = inside & (a[..., 2] > a[..., 1] + 25)
    stops = [0.0, 0.1, 0.22, 0.38, 0.58, 0.8, 1.0]
    sph_abs = {"t": "circle", "c": [cx, cy + 400], "r": R}
    po, _ = paint.fit_bevel(sph_abs, a, (0, 400), stops, R, ocean, radial_c=(cx, cy + 400))
    pg, _ = paint.fit_bevel(sph_abs, a, (0, 400), stops, R, green, radial_c=(cx, cy + 400))
    po["bevel"]["of"] = sph
    pg["bevel"]["of"] = sph
    lands = [
        poly((10.2, 7.8), (12.4, 7.8), (13.8, 9.3), (14.0, 10.8), (15.1, 10.7), (16.7, 9.8), (16.8, 7.9), (18.8, 7.9), (18.9, 10.3),
             (17.4, 11.1), (16.9, 12.4), (15.6, 13.6), (14.4, 14.6), (13.6, 15.6), (12.8, 16.7), (10.4, 16.9), (9.6, 18.2),
             (7.9, 17.9), (7.8, 12.0), (8.8, 10.3), (9.4, 9.2)),
        {"t": "sub", "a": poly((21.0, 6.9), (22.0, 6.9), (23.3, 8.6), (24.6, 9.2), (26.9, 10.0), (28.5, 12.5), (28.6, 22.5), (27.0, 25.8),
                               (25.0, 26.0), (24.9, 23.4), (21.0, 23.0), (20.9, 20.0), (20.7, 15.3), (21.6, 12.5), (21.9, 10.6), (21.2, 9.0)),
         "b": poly((22.8, 14.8), (27.4, 14.6), (27.4, 16.9), (25.0, 17.0), (22.8, 16.2))},
        poly((13.0, 20.8), (16.4, 20.8), (18.6, 21.9), (20.6, 24.0), (20.4, 25.6), (19.4, 26.6), (17.8, 27.6), (16.3, 28.7),
             (14.4, 28.7), (13.8, 27.4), (13.1, 25.9), (12.3, 24.5), (12.3, 22.8), smooth=1),
        ell(10.4, 18.9, 1.0, 0.75),
    ]
    out = [(sph, po, {})]
    for s in lands:
        out.append((s, pg, {"clip": sph}))
    return out


TL, TM, TD = [168, 152, 200], [120, 96, 176], [56, 40, 88]


def tiles():
    """Stacked terrain tiles: four purple blocks in a 2 x 2 isometric layout (back A, right B, left C, front D: top vertices
    (15.0, 8.4), (22.0, 11.7), (11.5, 14.8), (19.3, 17.6)), each a light lilac top face, a mid purple left face and a dark
    purple right face, flat colours, no outline; drawn back to front so each block hides the one behind it. Every corner was
    read on the face map of the idle cell (top / left / right face colour per pixel)."""
    A = [[(15.0, 8.4), (20.2, 10.4), (14.2, 13.0), (8.4, 11.8)],
         [(8.4, 11.8), (14.2, 13.0), (14.6, 15.3), (8.2, 15.1)],
         [(14.2, 13.0), (20.2, 10.4), (20.2, 12.6), (15.0, 15.2)]]
    B = [[(22.0, 11.7), (26.2, 14.3), (21.0, 16.4), (16.8, 14.6)],
         [(16.8, 14.6), (21.0, 16.4), (21.0, 18.1), (16.3, 17.9)],
         [(21.0, 16.4), (26.2, 14.3), (28.0, 14.9), (28.0, 17.8), (26.6, 18.9), (21.2, 19.1)]]
    C = [[(11.5, 14.8), (16.0, 16.8), (12.6, 18.9), (5.2, 18.0)],
         [(5.2, 18.0), (12.6, 18.9), (11.1, 22.7), (8.8, 22.1), (5.0, 20.9)],
         [(12.6, 18.9), (16.0, 16.8), (18.1, 17.4), (12.3, 22.6), (11.1, 22.7)]]
    D = [[(19.3, 17.6), (23.1, 20.2), (17.8, 22.9), (13.6, 20.5)],
         [(13.6, 20.5), (17.8, 22.9), (18.4, 26.7), (16.0, 25.7), (12.2, 23.7), (12.2, 21.3)],
         [(17.8, 22.9), (23.1, 20.2), (25.7, 21.4), (25.8, 22.9), (19.7, 26.7), (18.4, 26.7)]]
    out = []
    for blk in (A, B, C, D):
        for pts, col in zip(blk, (TL, TM, TD)):
            out.append((poly(*pts), col, {}))
    return out


GLYPHS = {"flag": flag, "notice": notice, "pair": pair, "pin": pin, "money": money, "chart": chart, "checklist": checklist,
          "trophy": trophy, "globe": globe, "tiles": tiles}
