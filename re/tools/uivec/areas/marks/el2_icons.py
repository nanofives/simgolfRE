"""Fix round 2: GBUBBLES flag (cell 9), house (cell 24, roster "hand pointer"), two heads (cell 22, roster "theatre mask")
and white cup (cell 34). Crop pixel (i, j) covers [i, i+1) x [j, j+1); every number is read from the cell's pixel grid
(log/roadmap/P41/work/marks/read/gb-NN.txt and work2/marks/read/).
"""
import common as C
from fix2 import (DISC, DISC_SHADOW, SHADOW, U, cub, ell, inter, item, lay, mirror_layers, mv, poly, rect, sub, vline,
                  circ)

SAGE = [184, 200, 136]
BLACK = [0, 0, 0]


def cell(i):
    return (C.GB, (16 * i, 0x144, 16, 16))


ELEMENTS = {}


def E(eid, i, how, changed, fn, unsure="", z=20):
    ELEMENTS[eid] = {"how": how, "changed": changed, "unsure": unsure, "z": z, "states": [("idle",) + cell(i) + (fn,)]}


# ------------------------------------------------------------------------------------------------------------- flag
# Grid (cell 9): pole col 6 rows 2-12; pennant row 3 cols 6-8, row 4 cols 6-9, row 5 cols 6-10, row 6 cols 9-10.
# So a flag of constant depth whose free half droops: top edge flat to x 9 then down to y 5 at x 11; bottom edge at y 6
# to x 9, then y 7 to x 11; the free end is a straight vertical edge (x 11, y 5..7).
def flag():
    top = cub([6.9, 2.95], [8.7, 2.9], [9.9, 4.2], [11.25, 5.0])
    bot = cub([11.25, 7.05], [10.0, 7.05], [9.5, 6.0], [6.9, 6.0])
    pennant = poly(top + bot)
    return item([lay(mv(DISC, 1, 1), SHADOW), lay(DISC, SAGE),
                 lay(rect(6.0, 2.0, 7.0, 13.0), BLACK),
                 lay(pennant, BLACK)])


E("gb-flag", 9,
  "sage disc + shadow disc; golf flag: a 1 px pole (rows 2-12) and a waving pennant of constant depth attached to its top: "
  "the top edge runs level then bends down, the bottom edge runs level and steps down at the same place, the free end is a "
  "straight vertical edge (x 11, rows 5-6); crisp corners at the pole and the free end",
  "redrawn from a new read: the pennant is a cloth of even depth whose outer half droops (two parallel S-shaped edges, a "
  "straight free end, square corners), not the rounded blob of round 1 nor a triangle", flag)


# ------------------------------------------------------------------------------------------------------------- house
# Grid (cell 24): grey (72) gable: row 5 cols 6-8, row 6 cols 5-9, rows 7-8 cols 4-10; walls cols 4-5 and 9-10 rows 9-11
# with a 3 px doorway (cols 6-8) between them. Black roof lines on the lines x + y = 11 (left, pixels (5,5) .. (2,8)) and
# x - y = 4 (right, pixels (11,7), (12,8)): they meet above the ridge at (7.5, 3.5) and overhang the walls by 2 px.
GREY = [72, 72, 72]


def house():
    import common
    body = poly(common.rounded([[7.5, 4.0], [11.0, 7.5], [11.0, 12.0], [9.0, 12.0], [9.0, 9.0], [6.0, 9.0], [6.0, 12.0],
                                [4.0, 12.0], [4.0, 7.5]], [0.35, 0.15, 0.1, 0.1, 0.25, 0.25, 0.1, 0.1, 0.15]))
    roof = common.pline([[2.0, 9.0], [7.5, 3.5], [13.0, 9.0]], 1.1)        # one line, round join at the ridge
    return item([lay(DISC_SHADOW, SHADOW), lay(body, GREY), lay(roof, BLACK)])


E("gb-hand-pointer", 24,
  "small house (Mariano: 'it should be a small house'): a grey gable house, pointed roof (its ridge sits under the roof line), walls 2 px each side of a 3 px doorway, and the roof line: one black inverted V (round join at the ridge, "
  "where the art's two eave lines meet, x + y = 11 and x - y = 4) running down at 45 degrees to 2 px past each wall; the "
  "disc's shadow crescent",
  "redrawn as a house: the round dome of round 1 is now a pointed gable (the art's grey widens 1 px per row: a 45 degree "
  "roof), and the black lines are the roof's eaves on both sides, ending past the walls",
  house,
  unsure="the art draws the right eave only on its lower two pixels (rows 7-8) and leaves the ridge pixel row empty; drawn "
         "as one continuous roof line, mirrored from the left, so the roof reads as one")


# ------------------------------------------------------------------------------------------------------------- two heads
# Grid (cell 22): two heads back to back, exact mirror images about x = 7.5 (col i <-> col 14 - i). The left head faces
# left: hair cap rows 2-4 (cols 1-5, col 2-5 on row 2), a lock over the forehead (row 5 cols 0-2, col 0 rows 6-7), hair down
# the back of the head (col 5 rows 5-11) with the ear sticking out behind (col 6 row 7); face cols 0-4 rows 5-11 with the
# nose at x 0 (rows 8-9) and the chin at col 1 (row 11); eyes at (2, 6) and (4, 6); mouth row 9 cols 3-4; the line under the
# jaw row 11 cols 2-5 and a neck pixel (2, 12), all in the hair colour.
YEL = [248, 200, 40]
FACE_SHADE = [222, 160, 30]


def head(hair):
    face = poly([[1.4, 4.9], [1.05, 5.6], [1.0, 7.9]]                                   # forehead under the bangs
                + cub([1.0, 7.9], [0.55, 8.2], [-0.1, 8.7], [-0.15, 9.15])[1:]          # nose: out 1 px below the bangs
                + cub([-0.15, 9.15], [0.2, 9.5], [0.75, 9.55], [0.9, 9.85])[1:]          # under the nose
                + cub([0.9, 9.85], [0.8, 10.3], [1.05, 10.5], [0.95, 10.85])[1:]         # lips
                + cub([0.95, 10.85], [0.95, 11.4], [1.2, 11.65], [1.8, 11.65])[1:]       # chin
                + cub([1.8, 11.65], [3.0, 11.75], [4.6, 11.5], [5.6, 10.5])[1:]           # jaw rising to the back
                + [[5.6, 4.9]])
    cap = poly(cub([0.0, 8.1], [-0.2, 6.6], [0.0, 4.4], [1.2, 3.0])
               + cub([1.2, 3.0], [1.9, 2.1], [2.6, 1.95], [3.6, 1.95])[1:]
               + cub([3.6, 1.95], [5.0, 1.95], [6.15, 2.4], [6.2, 4.3])[1:]                # rounded crown
               + cub([6.2, 4.3], [6.25, 7.5], [6.15, 10.4], [5.95, 11.2])[1:]              # back of the head
               + cub([5.95, 11.2], [5.8, 11.9], [5.3, 12.05], [4.9, 11.8])[1:]             # nape
               + [[4.95, 6.0]]
               + cub([4.95, 6.0], [4.6, 5.2], [3.8, 5.0], [3.1, 5.05])[1:]              # hairline over the forehead
               + cub([3.1, 5.05], [2.0, 5.3], [1.35, 5.7], [1.2, 6.6])[1:]               # the bangs sweeping down the temple
               + cub([1.2, 6.6], [1.15, 7.6], [0.9, 8.2], [0.45, 8.15])[1:]              # rounded tip
               + cub([0.45, 8.15], [0.1, 8.1], [0.0, 8.0], [0.0, 7.6])[1:])
    ear = U(ell(6.15, 7.5, 0.95, 0.72))
    L = [lay(face, {"linear": {"a": [1.5, 0.0], "b": [5.2, 0.0], "stops": [[0, YEL], [0.7, YEL], [1.0, FACE_SHADE]]}}),
         lay(cap, hair), lay(ear, hair),
         lay(ell(2.5, 6.55, 0.42, 0.5), hair), lay(ell(4.45, 6.55, 0.42, 0.5), hair),                          # eyes
         lay(vline([[3.05, 9.5], [3.85, 9.58], [4.65, 9.5]], 0.7), hair),                                        # mouth
         lay(vline([[1.5, 11.75], [3.2, 11.85], [4.6, 11.6], [5.7, 10.7]], 0.75), hair),                       # under the jaw
         lay(poly([[2.15, 11.8], [3.15, 11.8], [3.35, 13.0], [2.05, 13.0]]), hair)]                                                  # neck
    return L


def heads():
    return item([lay(DISC_SHADOW, SHADOW)] + head([80, 48, 24]) + mirror_layers(head([232, 80, 24]), 7.5))


E("gb-theatre-mask", 22,
  "two people's heads back to back, mirror images about the disc's axis (x 7.5): each a face in three-quarter view with a "
  "forehead, a nose that sticks out, lips and a rounded chin, a hair cap with a rounded crown that runs down the back of the "
  "head to the nape and sweeps a lock down over the temple, an ear behind, two eye dots, a mouth line, the line under the jaw "
  "and a short neck (brown hair on the left, orange on the right; the face shades a little darker toward the back)",
  "people shape: the boxy faces of round 1 are now head silhouettes (nose, lips, chin, jaw curve, rounded crown, hair to the "
  "nape, a real ear); the right head now mirrors about x 7.5 like the art (round 1 mirrored about 7.0, 1 px off)",
  heads,
  unsure="the roster calls it a theatre mask; the art shows two heads facing away from each other. Two tries this round; the "
         "silhouette now has nose, lips, chin and hair to the nape, but the neck and the line under the jaw are in the hair "
         "colour as in the art, which keeps the faces flat at this size. If it still does not read as people, a shaded skin "
         "neck would be the next step (that departs from the art's colours)")


# ------------------------------------------------------------------------------------------------------------- white cup
# Grid (cell 34), light from the upper left, axis x 5 (the dark olive column on the right is the cup's own shadow side):
#   row 0  white 0-8, pink 9                 (the rim's top face)
#   row 1  pink 0-7, tan 8, olive 9          (the rim's front face)
#   row 2  white 1, grey 2-5, tan 6-7, olive 8   (bowl top in the rim's shadow)
#   rows 3-4 white 1, pink 2-6, tan 7, olive 8   row 5 white 2, pink 3-5, tan 6, olive 7
#   row 6  grey 3, tan 4-5, olive 6 (stem top under the bowl)   row 7 pink 3-4, tan 5, olive 6
#   row 8  white 1-3, pink 4, grey 5-6, tan 7 (foot)            row 9 olive 2-8 (cast shadow under the foot)
WHITE = [255, 255, 255]
PINK = [248, 216, 248]
LGREY = [204, 207, 209]
TAN = [179, 167, 151]
OLIVE = [104, 104, 80]


def lit_bands(S, lit=True):
    """The measured light edge (1 px white on the left), dark edge (tan) and core shadow (1 px olive on the right) of a
    part, as layers that follow its outline."""
    L = []
    if lit:
        L.append(lay(sub(S, mv(S, 1.0, 0)), WHITE))
    L.append(lay(sub(S, mv(S, -2.0, 0)), TAN))
    L.append(lay(sub(S, mv(S, -1.0, 0)), OLIVE))
    return L


def cup():
    bowl = {"t": "lathe", "x": 5.0, "prof": [[2.0, 4.0], [4.6, 4.0], [5.4, 3.4], [6.0, 2.6]]}
    stem = rect(3.0, 5.6, 7.0, 8.0)
    foot = rect(1.0, 8.0, 8.0, 9.0, 0.3)
    rim = rect(0.0, 0.0, 10.0, 2.0, 0.3)
    L = [lay(rect(2.0, 9.0, 9.0, 10.0, 0.3), OLIVE)]                                       # cast shadow under the foot
    # stem: pink, its top row in the bowl's shadow (grey to tan), tan and olive down its right side
    L += [lay(stem, PINK), lay(inter(stem, rect(3.0, 5.6, 7.0, 7.0)), {"linear": {"a": [3.5, 0], "b": [4.5, 0],
                                                                                   "stops": [[0, LGREY], [1, TAN]]}})]
    L += lit_bands(stem, lit=False)
    # bowl: pink with the light edge, dark edge and core shadow; its top row is in the rim's shadow
    L += [lay(bowl, PINK)] + lit_bands(bowl)
    L += [lay(inter(bowl, rect(2.0, 2.0, 6.0, 3.0)), LGREY), lay(inter(bowl, rect(6.0, 2.0, 8.0, 3.0)), TAN)]
    # foot: white on the lit left, pink, grey, tan on the right
    L += [lay(foot, {"linear": {"a": [1.0, 0], "b": [8.0, 0],
                                "stops": [[0, WHITE], [3 / 7, WHITE], [3 / 7 + 0.01, PINK], [4 / 7, PINK], [4 / 7 + 0.01, LGREY],
                                          [6 / 7, LGREY], [6 / 7 + 0.01, TAN], [1, TAN]]}})]
    # rim: top face white (its right end turning pink), front face pink, tan, olive
    L += [lay(rim, PINK),
          lay(inter(rim, rect(0.0, 0.0, 10.0, 1.0)), {"linear": {"a": [8.6, 0], "b": [9.4, 0], "stops": [[0, WHITE], [1, PINK]]}}),
          lay(inter(rim, rect(8.0, 1.0, 9.0, 2.0)), TAN), lay(inter(rim, rect(9.0, 1.0, 10.0, 2.0)), OLIVE)]
    return item(L)


E("gb-white-cup", 34,
  "goblet lit from the upper left, axis x 5: a rim slab (top face white turning pink at its right end, front face pink, tan, "
  "olive), a bowl (pink, 1 px white light edge down the left, tan dark edge and 1 px olive core shadow down the right, its "
  "top row grey in the rim's shadow), a stem (top row grey to tan under the bowl, pink, tan, olive), a foot (white, pink, "
  "grey, tan from left to right) and its olive cast shadow below",
  "lighting baked as the art's bands: every part carries its own light edge, dark edge and core shadow that follow its "
  "outline (round 1 used one soft left-to-right gradient for the whole cup, so the stem and foot were lit wrong and the "
  "edges were blurred); the rim's shadow on the bowl, the bowl's shadow on the stem and the cast shadow are now real layers",
  cup)
