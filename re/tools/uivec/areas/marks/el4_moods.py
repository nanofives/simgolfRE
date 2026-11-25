"""Fix round 4: GBUBBLES row 0x144 cells 0-4, the golfer mood faces, one clean version.

Round 3 rejected both versions (no note); the member-list faces got "needs something in the middle of capturing the actual
emotions of the faces + look believable". This round draws each face from the shared recipe
log/roadmap/P41/work4/mood_recipe.md (written by the hud_panels area): the same emotion gets the same features in the same
layer order (disc, skin, cheeks, baked shadows, features with a darker core, glints). Numbers are the recipe's, relative to
the face centre for a radius-8 face, scaled by r / 8 with r = 7.5 (the gb faces span cols 0-14 and rows 0-14).
Colours are the gb art's own (text grids: py -3.12 -c "import common as C; print(C.textgrid(C.GB, 16*k, 0x144, 16, 16))").

Which recipe face each gb cell is (art evidence, gb cell pixels):
 cell 0 -> 10 furious:    horns rows 0-2; dark brows rows 4-5 falling to the middle; grey glints; dark upper lip row 8 over
                           two rows of clenched teeth rows 9-10 (cols 3-10)
 cell 1 -> 8 unhappy:     short brows row 2 over 3 x 3 black eyes rows 3-5; arch frown, bar on row 8 (cols 6-9), corners down
                           to rows 10-11; a lower lip under the apex (row 10, cols 6-9, (120,80,8))
 cell 2 -> 6 neutral:     2-3 px dark eye blocks rows 4-6; a straight mouth on row 10 (cols 2-12)
 cell 3 -> 3 happy:       eyes rows 4-6; closed smile, corners up on row 8 (cols 2 / 12), bottom row 11
 cell 4 -> 1 overjoyed:   tall eyes rows 2-6 with a grey glint; open grin rows 8-12 full of teeth (upper row 9, lower row 11)
"""
import common as C
from fix2 import circ, ell, inter, item, lay, mirror_layers, mv, poly, rect, sub, vline

STATES = ("furious", "unhappy", "neutral", "happy", "delighted")
EMOTIONS = ["furious", "unhappy", "neutral", "happy", "overjoyed"]
CX, CY = 7.5, 7.5              # the gb faces' centre (cols 0-14, rows 0-14)
K = 7.5 / 8.0                  # the recipe's numbers are for radius 8


def cell(k):
    return (16 * k, 0x144, 16, 16)


def T(x, y):
    return [round(CX + x * K, 4), round(CY + y * K, 4)]


def W(w):
    return round(w * K, 4)


def sym(pts):
    """Recipe points given for the left half (x <= 0), left to right; the right half mirrored, so the stroke runs
    left corner -> middle -> right corner."""
    right = [(-x, y) for x, y in reversed(pts) if x != 0]
    return [T(x, y) for x, y in list(pts) + right]


def dk(c, f):
    return [round(v * f) for v in c]


def hexc(h):
    return [int(h[i:i + 2], 16) for i in (1, 3, 5)]


def softl(shape, col, s, a, clip=None):
    L = lay(shape, col, soft=s, alpha=a)
    if clip is not None:
        L["clip"] = clip
    return L


def both(layers):
    return layers + mirror_layers(layers, CX)


def disc():
    return circ(*T(0, -0.1), W(7.7))


def skin_disc():
    return circ(*T(0, -0.1), W(6.9))


def base_layers(P, cheeks=False):
    """Recipe layers 1-3: the disc in the rim colour (its last 0.4 in the outer rim colour), the skin (radial from up-left:
    light -> base at 4.5 -> rim by 7.2, soft 0.8), and for the smiles raised cheeks."""
    L = [lay(disc(), P["rim"]),
         lay(sub(disc(), {"t": "grow", "of": disc(), "d": -W(0.4)}), P["outer"]),
         {"shape": skin_disc(), "fill": {"radial": {"c": T(-1.0, -2.4), "r": W(7.2),
                                             "stops": [[0, P["light"]], [4.5 / 7.2, P["base"]], [1, P["rim"]]]}},
          "soft": W(0.8)}]
    if cheeks:
        L += both([softl(ell(*T(-4.4, -0.4), W(1.5), W(1.0)), P["light"], W(0.6), 0.55, disc())])
    return L


def feature(shape, col):
    """Recipe layer 5: a dark feature in its colour with an inner core 0.3 inside it, 25 % darker."""
    return [lay(shape, col), lay({"t": "grow", "of": shape, "d": -W(0.3)}, dk(col, 0.75))]


def brow_shadow(shape, P):
    return softl(mv(shape, 0, W(0.7)), dk(P["base"], 0.75), W(0.6), 0.5, disc())


def mouth_shadow(shape, P):
    return softl(mv(shape, 0, W(0.9)), dk(P["base"], 0.8), W(0.7), 0.35, disc())


def glint(ex, ey, eye, col, r=0.45):
    """Recipe layer 6: one glint per eye at its upper inner side (0.35 toward the axis, 0.7 up), clipped to the eye."""
    s = 1 if ex < 0 else -1
    return lay(inter(circ(*T(ex + s * 0.35, ey - 0.7), W(r)), eye), col)


def oval(a, b, t):
    """A slanted oval: long axis a -> b, thickness t, round ends."""
    return vline([T(*a), T(*b)], W(t))


def stroke(pts, w):
    return vline(pts, W(w))


# ------------------------------------------------------------------------------------------- gb art colours per face
PAL = {
    "furious": {"light": [240, 0, 0], "base": [200, 0, 0], "rim": [150, 16, 0], "outer": [136, 16, 0], "ink": [24, 20, 30],
                "line": [80, 16, 16], "teeth": [204, 207, 209], "gap": [130, 131, 131], "glint": [130, 131, 131],
                "horn": [136, 16, 0], "horn_edge": [80, 16, 16]},
    "unhappy": {"light": [214, 92, 4], "base": [200, 72, 0], "rim": [150, 30, 0], "outer": [136, 16, 0], "ink": [12, 12, 12],
                "line": [52, 30, 14], "lip": [132, 84, 10], "brow": [40, 24, 24], "glint": [200, 200, 200]},
    "neutral": {"light": [232, 253, 30], "base": [208, 220, 0], "rim": [166, 173, 0], "outer": [120, 175, 0], "ink": [18, 17, 28],
                "line": [80, 70, 18], "glint": [200, 200, 200]},
    "happy": {"light": [140, 236, 10], "base": [118, 226, 0], "rim": [45, 184, 0], "outer": [41, 154, 4], "ink": [24, 27, 48],
              "line": [40, 44, 54], "glint": [200, 200, 200]},
    "overjoyed": {"light": [70, 255, 40], "base": [10, 250, 0], "rim": [8, 184, 0], "outer": [32, 140, 16], "ink": [16, 16, 14],
                  "line": [56, 64, 48], "mouth": [38, 44, 30], "teeth": [204, 207, 209], "tooth_hi": [249, 249, 231],
                  "tooth_line": [160, 160, 160], "glint": [200, 200, 200]},
}


# ------------------------------------------------------------------------------------------------------------ faces
def furious():                                                  # recipe face 10
    global CY
    CY = 7.95                  # the gb art's red head sits half a pixel lower (rows 1-14) to leave the horns room
    try:
        return _furious()
    finally:
        CY = 7.5


def _furious():
    P = PAL["furious"]
    L = []
    # horns: curved triangles standing on the crown, bases between x -2.2 and -5.6, tips at (-4.6, -8.2), leaning outward
    horn = poly([T(-2.0, -6.9), T(-2.9, -7.5), T(-3.9, -8.0), T(-4.6, -8.2), T(-5.0, -7.6), T(-5.4, -6.6), T(-5.8, -5.2),
                 T(-4.0, -5.6)], smooth=2)
    L += both([lay(horn, P["horn_edge"]),
               lay({"t": "grow", "of": horn, "d": -W(0.5)}, {"linear": {"a": T(-5.4, -6.0), "b": T(-3.0, -7.6),
                                                                      "stops": [[0, P["horn"]], [1, P["light"]]]}})])
    L += base_layers(P)
    brow = [oval((-5.2, -5.6), (-1.3, -3.6), 1.3), oval((5.2, -5.6), (1.3, -3.6), 1.3)]
    L += [brow_shadow(b, P) for b in brow]
    # gritted teeth: a rounded box x -4.4..4.4, y 0.5..3.5 (corner r 1.1), top bowed up 0.4, corners pulled down 0.3
    box = poly([T(-4.4, 0.8), T(-2.2, 0.3), T(0, 0.1), T(2.2, 0.3), T(4.4, 0.8), T(4.4, 3.8), T(2.2, 3.6), T(0, 3.5),
                T(-2.2, 3.6), T(-4.4, 3.8)], smooth=3)
    L.append(mouth_shadow(box, P))
    for b in brow:
        L += feature(b, P["ink"])
    for a, b in (((-4.5, -3.1), (-1.6, -2.0)), ((4.5, -3.1), (1.6, -2.0))):
        e = oval(a, b, 1.2)
        L += feature(e, P["ink"])
        L.append(glint((a[0] + b[0]) / 2, (a[1] + b[1]) / 2 + 0.35, e, P["glint"], 0.35))
    teeth = {"t": "grow", "of": box, "d": -W(0.65)}
    L += [lay(box, P["line"]), lay(teeth, P["teeth"]),
          lay(inter(stroke([T(-5, 2.0), T(5, 2.0)], 0.4), teeth), P["gap"])]
    for gx in (-3.0, -1.0, 1.0, 3.0):
        L.append(lay(inter(stroke([T(gx, 0.0), T(gx, 4.0)], 0.35), teeth), P["gap"]))
    return item(L)


def unhappy():                                                  # recipe face 8
    P = PAL["unhappy"]
    L = base_layers(P)
    brows = [stroke([T(-4.0, -5.3), T(-1.5, -5.9)], 1.15), stroke([T(4.0, -5.3), T(1.5, -5.9)], 1.15)]
    # the arch tapers to its corners (an even-width arch reads as a moustache at 12x)
    # a rounded arch through the art's apex (row 8) and corners (rows 10-11, cols 3 / 12): the recipe's flat-topped arch
    # with steep legs read as a moustache at every size
    frown = vline(sym([(-5.0, 4.1), (-4.2, 2.7), (-2.7, 1.45), (0, 0.95)]), [W(w) for w in (1.0, 1.25, 1.3, 1.35, 1.3, 1.25, 1.0)])
    lip = stroke([T(-1.3, 2.4), T(1.3, 2.4)], 0.9)
    pout = ell(*T(0, 2.15), W(1.5), W(0.65))
    # the frown's shadow falls under its lower lip only (moved under the arch it reads as a moustache)
    L += [brow_shadow(b, P) for b in brows] + [mouth_shadow(lip, P)]
    for b in brows:
        L += feature(b, P["brow"])
    for ex in (-2.5, 2.5):                                      # heavy lids: cut flat on top at y -4.4
        e = sub(ell(*T(ex, -3.6), W(1.4), W(1.3)), {"t": "half", "p": T(0, -4.4), "n": [0, 1]})
        L += feature(e, P["ink"])
        L.append(glint(ex, -3.6 + 0.15, e, P["glint"]))
    # the pout: the lower lip lit (the art's (120,80,8) under the apex), its upper edge dark where it meets the frown
    # the pout: the lower lip as a lit bulge hanging from the apex (the art's (120,80,8) under it), its lower edge the
    # recipe's 0.9 lip line at y 2.4, so it joins the frown instead of standing apart as a second mouth
    L += [softl(ell(*T(0, 3.0), W(1.6), W(0.5)), dk(P["base"], 0.72), W(0.5), 0.6, disc()),    # shadow under the pout
          softl(pout, P["light"], W(0.55), 0.6, disc()),                                          # the lit lower lip
          lay(inter(lip, mv(pout, 0, W(0.25))), dk(P["base"], 0.8), alpha=0.5)]                  # its lower edge
    L += feature(frown, P["line"])
    return item(L)


def neutral():                                                  # recipe face 6
    P = PAL["neutral"]
    L = base_layers(P)
    mouth = stroke([T(-4.6, 2.5), T(4.6, 2.5)], 1.25)
    L.append(mouth_shadow(mouth, P))
    for ex in (-3.0, 3.0):
        e = circ(*T(ex, -2.6), W(1.45))
        L += feature(e, P["ink"])
        L.append(glint(ex, -2.6, e, P["glint"]))
    L += feature(mouth, P["line"])
    return item(L)


def happy():                                                    # recipe face 3
    P = PAL["happy"]
    L = base_layers(P, cheeks=True)
    smile = stroke(sym([(-5.6, 1.4), (-4.4, 2.6), (-3.0, 3.5), (-1.2, 3.65)]), 1.3)
    L.append(mouth_shadow(smile, P))
    for ex in (-3.0, 3.0):
        e = ell(*T(ex, -3.1), W(1.0), W(2.0))
        L += feature(e, P["ink"])
        L.append(glint(ex, -3.1, e, P["glint"]))
    L += feature(smile, P["line"])
    return item(L)


def overjoyed():                                                # recipe face 1
    P = PAL["overjoyed"]
    L = base_layers(P, cheeks=True)
    # open grin: corners (-5.6, 0.9), upper lip through (-3.2, 0.2) and flat at 0.1, lower edge (-4.0, 3.2), (-2.6, 4.6), 4.9
    mouth = poly([T(-5.6, 0.9), T(-4.4, 0.45), T(-3.2, 0.2), T(-1.5, 0.1), T(0, 0.1), T(1.5, 0.1), T(3.2, 0.2), T(4.4, 0.45),
                  T(5.6, 0.9), T(4.0, 3.2), T(2.6, 4.6), T(1.3, 4.85), T(0, 4.9), T(-1.3, 4.85), T(-2.6, 4.6), T(-4.0, 3.2)],
                 smooth=2)
    L.append(mouth_shadow(mouth, P))
    for ex in (-3.0, 3.0):
        e = ell(*T(ex, -3.4), W(1.05), W(2.3))
        L += feature(e, P["ink"])
        L.append(glint(ex, -3.4, e, P["glint"]))
    teeth = inter({"t": "grow", "of": mouth, "d": -W(0.7)}, {"t": "half", "p": T(0, 3.4), "n": [0, 1]})
    L += [lay(mouth, P["line"]),
          lay({"t": "grow", "of": mouth, "d": -W(0.35)}, P["mouth"]),
          lay(teeth, P["teeth"]),
          lay(inter(teeth, {"t": "half", "p": T(0, 1.9), "n": [0, 1]}), P["tooth_hi"]),
          lay(inter(stroke([T(-6, 2.5), T(6, 2.5)], 0.4), teeth), P["tooth_line"])]
    return item(L)


FACES = (furious, unhappy, neutral, happy, overjoyed)

ELEMENTS = {
    "gb-mood": {
        "emotions": EMOTIONS,
        "how": "one clean version from the shared recipe (work4/mood_recipe.md): the five gb faces are the recipe's faces 10 "
               "furious, 8 unhappy, 6 neutral, 3 happy and 1 overjoyed, read from the art (horns, V brows and gritted teeth; "
               "arch frown with the lower lip under it; straight mouth; closed smile with corners on row 8; open grin with "
               "both teeth rows). Each face: disc in the rim colour, skin as a radial lit from the upper left (lighter "
               "centre, darker rim), raised cheeks on the two smiles, baked shadows under the brows and the mouth, then "
               "brows, eyes and mouth as filled shapes and round-capped strokes with a 25 % darker core, a glint per eye. "
               "Numbers are the recipe's scaled by 7.5 / 8 about the cell centre (7.5, 7.5); colours are the gb art's",
        "changed": "against round 3: no pixel patches and no art-sized 1 px features; each face now says its emotion with "
                   "its brows, eyes and mouth (furious V brows over narrowed slanted eyes and bared teeth; unhappy worried "
                   "brows, heavy flat-topped lids and an arch frown with a pout; neutral round eyes and a straight line; "
                   "happy tall eyes, raised cheeks, a wide U smile; overjoyed tall bright eyes and an open grin full of "
                   "teeth); features have thickness and round ends; the same family as the member-list faces",
        "unsure": "departures from the recipe, each because the recipe's shape read wrong at 12x: (1) unhappy: the frown is a "
                  "rounded arch through the art's apex (row 8) and corners (rows 10-11), not the recipe's flat-topped arch "
                  "with steep legs, and the pout is a lit bulge with a shadow under it, not a 0.9 line in the mouth colour: "
                  "both recipe forms together read as a moustache over a second mouth; (2) furious: the face is centred on "
                  "y 7.95 (the art's red head sits on rows 1-14) so the horns are not cut by the cell top, and the horns are "
                  "lit on their inner side so they read against the head; (3) colours are the gb art's, not the member "
                  "panel's. Mapping: gb cell 3 is drawn as recipe face 3 happy (its wide smile); its eyes alone are nearer "
                  "face 4 content. gb cell 4 is named delighted in the roster but shows both teeth rows, so it is face 1 "
                  "overjoyed",
        "states": [(s, C.GB, cell(k), fn) for k, (s, fn) in enumerate(zip(STATES, FACES))]},
}
