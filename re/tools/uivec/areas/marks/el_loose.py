"""GBUBBLES loose icons (cells 13, 17-24, 27-34): small pictures on the transparent key, most with a dark copy one pixel
down-right as a drop shadow. Cells 22-24 also carry the lower-right shadow crescent of the 15 px disc they are drawn over
(the disc itself is not in the cell). Crop pixel (i, j) covers [i, i+1) x [j, j+1); numbers read from read/gb-NN.txt.
"""
import numpy as np

import common as C
import sdf

DISC = {"t": "circle", "c": [7.5, 7.5], "r": 7.5}
DISC_SHADOW = {"t": "sub", "a": {"t": "move", "of": DISC, "d": [1, 1]}, "b": DISC}


def cell(i):
    return (C.GB, (16 * i, 0x144, 16, 16))


def item(layers):
    return [{"box": [-1, -1, 17, 17], "layers": layers}]


def lay(shape, fill, **kw):
    d = {"shape": shape, "fill": fill}
    d.update(kw)
    return d


def mv(S, dx=1, dy=1):
    return {"t": "move", "of": S, "d": [dx, dy]}


def rect(x0, y0, x1, y1, r=0.0):
    return {"t": "rrect", "c": [(x0 + x1) / 2, (y0 + y1) / 2], "half": [(x1 - x0) / 2, (y1 - y0) / 2], "r": r}


def mirror(S, ax):
    """Mirror a shape about the vertical line x = ax (works for the shape types used here)."""
    t = S["t"]
    S = dict(S)
    if t in ("circle", "rrect", "ellipse"):
        S["c"] = [2 * ax - S["c"][0], S["c"][1]]
        if "rot" in S:
            S["rot"] = -S["rot"]
    elif t == "poly":
        S["rings"] = [[[2 * ax - x, y] for x, y in r] for r in S["rings"]]
    elif t == "marks.pline":
        S["pts"] = [[2 * ax - x, y] for x, y in S["pts"]]
    elif t in ("union", "inter"):
        S["of"] = [mirror(q, ax) for q in S["of"]]
    elif t == "sub":
        S["a"], S["b"] = mirror(S["a"], ax), mirror(S["b"], ax)
    elif t in ("move", "grow"):
        S["of"] = mirror(S["of"], ax)
        if t == "move":
            S["d"] = [-S["d"][0], S["d"][1]]
    else:
        raise ValueError(t)
    return S


def mirror_layers(layers, ax):
    out = []
    for L in layers:
        L = dict(L)
        L["shape"] = mirror(L["shape"], ax)
        if isinstance(L["fill"], dict) and "linear" in L["fill"]:
            l = dict(L["fill"]["linear"])
            l["a"], l["b"] = [2 * ax - l["a"][0], l["a"][1]], [2 * ax - l["b"][0], l["b"][1]]
            L["fill"] = {"linear": l}
        out.append(L)
    return out


# ------------------------------------------------------------------------------------------------ heart (my own shape)
def _heart(S, x, y):
    """marks.heart: the classic heart curve (x = 16 sin^3 t, y = -(13 cos t - 5 cos 2t - 2 cos 3t - cos 4t)) fitted to
    the box [x0, y0, x1, y1] (lobes' top at y0, tip at y1)."""
    t = np.linspace(0, 2 * np.pi, 97)[:-1]
    hx = 16 * np.sin(t) ** 3
    hy = -(13 * np.cos(t) - 5 * np.cos(2 * t) - 2 * np.cos(3 * t) - np.cos(4 * t))
    x0, y0, x1, y1 = S["box"]
    hx = x0 + (hx - hx.min()) / (hx.max() - hx.min()) * (x1 - x0)
    hy = y0 + (hy - hy.min()) / (hy.max() - hy.min()) * (y1 - y0)
    return sdf.sd({"t": "poly", "rings": [np.c_[hx, hy].tolist()]}, x, y)


sdf.EXTRA["marks.heart"] = _heart

ELEMENTS = {}


def E(eid, i, how, fn, unsure="", z=20):
    ELEMENTS[eid] = {"how": how, "unsure": unsure, "z": z, "states": [("idle",) + cell(i) + (fn,)]}


# 13 white ball: a ball lit from the upper left (white, a grey rim, a mauve shadow side) with a small dark shadow under it
E("gb-white-ball", 13, "golf ball: a 9 x 8 px ellipse painted white on the upper left through grey to mauve on the lower right, "
  "and a small dark shadow ellipse under it",
  lambda: item([lay({"t": "ellipse", "c": [4.6, 8.3], "r": [1.9, 0.75]}, [104, 88, 88]),
                lay({"t": "ellipse", "c": [4.5, 4.0], "r": [4.4, 4.0]},
                    {"radial": {"c": [3.0, 2.6], "r": 6.6, "stops": [[0, [255, 255, 255]], [0.5, [255, 255, 255]],
                                                                    [0.66, [191, 191, 191]], [0.8, [168, 128, 136]],
                                                                    [1.0, [104, 88, 88]]]}})]))

# 17 star: five points, lit from the upper left, with a black drop shadow
STAR = {"t": "marks.star", "c": [4.5, 4.6], "r": 4.6, "ri": 1.85, "a0": -90.0}
E("gb-star", 17, "five-pointed star (outer radius 4.6 px, inner 1.85, point up) shaded light yellow at the upper left to olive "
  "at the lower right, over a black copy one pixel down-right",
  lambda: item([lay(mv(STAR), [24, 24, 24]),
                lay(STAR, {"linear": {"a": [1.5, 1.0], "b": [7.5, 8.0], "stops": [[0, [253, 250, 150]], [0.45, [248, 248, 25]],
                                                                                  [1.0, [164, 164, 16]]]}})]))

# 18 green dollar: a 1 px S and two bars, bright green, with a dark olive copy one pixel down-right
G_S = C.pline([[7.0, 1.5], [1.4, 1.5], [0.8, 2.6], [1.8, 3.6], [4.6, 4.4], [5.6, 5.3], [5.0, 6.5], [0.0, 6.5]], 1.15, smooth=1)
G_BARS = {"t": "union", "of": [C.bar([2.5, 0.0], [2.5, 8.0], 1.0), C.bar([4.5, 0.0], [4.5, 8.0], 1.0)]}
E("gb-green-dollar", 18, "dollar sign in bright green: a 1 px S (flat ends, top bar 7 px, bottom bar 6 px) with two 1 px bars "
  "through it, over a dark olive copy one pixel down-right",
  lambda: item([lay(mv(G_S), [40, 48, 16]), lay(mv(G_BARS), [40, 48, 16]), lay(G_S, [0, 255, 0]), lay(G_BARS, [0, 255, 0])]))

# 19 purple bar: a 5 x 5 block over a 5 x 2 block (an exclamation mark), lilac, navy copies one pixel down-right
B1, B2 = rect(2, 0, 7, 5), rect(2, 6, 7, 8)
E("gb-purple-bar", 19, "lilac exclamation mark: a 5 x 5 px block over a 5 x 2 px block one pixel below it, each over a navy "
  "copy one pixel down-right",
  lambda: item([lay(mv(B1), [56, 48, 136]), lay(mv(B2), [56, 48, 136]), lay(B1, [160, 136, 239]), lay(B2, [160, 136, 239])]),
  unsure="read as an exclamation mark (tall block, short block); the roster calls it a purple bar")

# 21 crossed clubs: two clubs in an X (mirror images), black in front, dark olive behind
CLUB = [({"t": "ellipse", "c": [4.5, 3.2], "r": [1.5, 1.15], "rot": -25.0}, None),          # head: a rounded blob
        (C.bar([3.9, 3.8], [5.3, 5.6], 1.25), None),                                          # neck down into the shaft
        (C.bar([4.6, 5.0], [10.0, 10.4], 1.0), None),
        (C.bar([9.4, 10.8], [11.6, 13.0], 1.6), None)]


def club(col, flip=False):
    L = []
    for S, _ in CLUB:
        L.append(lay(mirror(S, 7.5) if flip else S, col))
    return L


E("gb-crossed-clubs", 21, "two golf clubs crossed in an X (mirror images): a rounded head (3 x 2.3 px oval) with a short neck at the top, a 1 px shaft, a 1.6 px grip at "
  "the bottom; the black one (top left to bottom right) in front of the dark olive one",
  lambda: item(club([56, 56, 39], True) + club([0, 0, 0])))


# 22 two heads back to back: yellow faces, hair cap + back of the head, fringe, eyes, ear, mouth, jaw line; the right head is
# the left one mirrored with orange hair
def head(hair, face):
    return [lay(rect(0.0, 4.8, 5.0, 11.0, 1.0), face),                                         # face
            lay({"t": "union", "of": [rect(1.0, 2.0, 6.0, 5.0, 1.2), rect(5.0, 3.0, 6.0, 12.0),     # hair cap, back of head
                                      rect(0.0, 4.6, 3.0, 6.0, 0.4), rect(0.0, 5.0, 1.0, 8.0)]}, hair),   # fringe
            lay(rect(5.0, 7.0, 7.0, 8.0, 0.4), hair),                                              # ear
            lay(rect(2.0, 6.0, 3.0, 7.0, 0.2), hair), lay(rect(4.0, 6.0, 5.0, 7.0, 0.2), hair),   # eyes
            lay(C.bar([3.0, 9.5], [5.0, 9.5], 1.0), hair),                                          # mouth
            lay(C.bar([2.0, 11.5], [6.0, 11.5], 1.0), hair),                                        # jaw / neck line
            lay(rect(2.0, 11.0, 3.0, 13.0), hair)]                                                  # neck


YEL = [248, 200, 40]
E("gb-theatre-mask", 22, "two faces back to back (mirror images): yellow face, hair cap running down the back of the head, a "
  "fringe, two eye dots, ear, mouth and jaw lines in the hair colour (brown on the left, orange on the right); the disc's "
  "shadow crescent at the lower right",
  lambda: item([lay(DISC_SHADOW, [24, 24, 24])] + head([80, 48, 24], YEL) + mirror_layers(head([232, 80, 24], YEL), 7.0)),
  unsure="the roster calls it a theatre mask; the art shows two yellow faces with brown / orange hair facing away from each other")

# 23 eye: a flattened hexagon (almond) outlined black, dark grey inside, a lash tick above and below the middle
EYE = {"t": "poly", "rings": [[[0.8, 8.5], [4.6, 6.0], [10.4, 6.0], [14.2, 8.5], [10.4, 11.0], [4.6, 11.0]]]}
E("gb-eye", 23, "eye: a pointed almond (flat-topped hexagon, 13 x 5 px) with a 1 px black outline and dark grey inside, a 1 px "
  "lash tick above and below its middle; the disc's shadow crescent",
  lambda: item([lay(DISC_SHADOW, [24, 24, 24]), lay(EYE, [0, 0, 0]), lay({"t": "grow", "of": EYE, "d": -1.0}, [72, 72, 72]),
                lay(C.bar([7.5, 5.0], [7.5, 6.2], 1.0), [0, 0, 0]), lay(C.bar([7.5, 10.8], [7.5, 12.0], 1.0), [0, 0, 0])]))

# 24 shelter: a grey dome on two pillars (door between), black eave lines sloping out from the dome
DOME = {"t": "sub", "a": {"t": "union", "of": [{"t": "circle", "c": [7.5, 8.5], "r": 3.5}, rect(4, 8.5, 11, 12)]},
        "b": rect(6, 9, 9, 12.5)}
E("gb-hand-pointer", 24, "arched shelter: a dark grey dome (semicircle r 3.5) on two 2 px pillars with a 3 px opening, a 1 px "
  "black eave line sloping down-left from the dome's top and a short one down-right; the disc's shadow crescent",
  lambda: item([lay(DISC_SHADOW, [24, 24, 24]), lay(DOME, [72, 72, 72]),
                lay(C.bar([5.9, 5.1], [2.1, 8.9], 1.0), [0, 0, 0]), lay(C.bar([10.9, 6.9], [12.9, 8.9], 1.0), [0, 0, 0])]),
  unsure="the roster calls it a hand pointer; the art is an arch on two pillars with black lines sloping out (a shelter or "
         "a bridge); drawn as read")

# 27-29 dashes: 7 x 3 capsules
PILL = {"t": "poly", "rings": [[[0, 1.5], [1.2, 0], [5.8, 0], [7, 1.5], [5.8, 3], [1.2, 3]]]}
E("gb-red-dash", 27, "red dash: a 7 x 3 px bar with pointed ends (flat-topped hexagon)", lambda: item([lay(PILL, [221, 38, 38])]))
E("gb-grey-dash", 28, "grey dash: a 7 x 3 px bar with pointed ends (flat-topped hexagon)", lambda: item([lay(PILL, [191, 191, 191])]))
E("gb-blue-dots", 29, "purple dash: a 7 x 3 px bar with pointed ends (flat-topped hexagon; the roster's 'dots' is one dash)", lambda: item([lay(PILL, [88, 72, 180])]))

# 30 / 32 hearts: the heart curve, lit at the upper left, over a near-black copy shifted right
HEART = {"t": "marks.heart", "box": [0.0, 0.0, 11.0, 9.2]}
HEART_PAINT = {"linear": {"a": [2.0, 1.0], "b": [8.0, 8.0], "stops": [[0, [233, 29, 28]], [0.4, [200, 0, 0]], [1.0, [150, 2, 1]]]}}
E("gb-heart", 30, "heart (11 x 9 px, the classic heart curve) shaded bright red at the upper left to dark red at the tip, over "
  "a near-black copy one pixel right",
  lambda: item([lay(mv(HEART, 1, 0.4), [17, 2, 2]), lay(HEART, HEART_PAINT)]))
SHEART = {"t": "marks.heart", "box": [1.0, 1.0, 8.0, 7.1]}
E("gb-small-heart", 32, "small heart (7 x 6 px), same drawing as the heart, over a near-black copy one pixel right",
  lambda: item([lay(mv(SHEART, 1, 0.4), [17, 2, 2]),
                lay(SHEART, {"linear": {"a": [2.0, 1.5], "b": [6.0, 6.5], "stops": [[0, [217, 12, 12]], [0.45, [196, 0, 0]],
                                                                                   [1.0, [142, 2, 1]]]}})]))

# 31 green screen: a lilac-framed monitor with a green screen showing a white arc over a black stroke, grey foot, shadow
FRAME = rect(0, 0, 9, 7, 0.3)
SCREEN = rect(1, 1, 8, 6)
E("gb-green-screen", 31, "monitor: a lilac 9 x 7 px frame, a green 7 x 5 px screen showing a white arc (left, over the top) and "
  "a black stroke under it, a light grey foot in the frame's bottom edge, a dark shadow one pixel down-right",
  lambda: item([lay(mv(FRAME), [104, 104, 80]), lay(FRAME, [232, 200, 248]), lay(SCREEN, [0, 255, 0]),
                lay(C.pline([[1.5, 6.0], [1.6, 4.0], [3.0, 2.5], [5.0, 2.4], [6.6, 3.6]], 1.0, smooth=1), [255, 255, 255], clip=SCREEN),
                lay(C.pline([[2.0, 5.5], [4.0, 4.6], [6.9, 4.5]], 0.95, smooth=1), [0, 0, 0], clip=SCREEN),
                lay(rect(3, 6, 6, 7), [191, 191, 191])]),
  unsure="what the screen shows is not clear at 7 x 5 px (a white arc over a black stroke); drawn as read")


# 33 trophy: read on axis x = 5.0: bowl 6 px wide (rows 0-4) narrowing to a 2 px stem (row 6), a 4 px collar, a 6 px foot and an
# 8 px dark base; a 1 px C-shaped handle each side from the rim down to the bowl's bottom; shiny: olive at the left edge, a
# white highlight left of centre, sage, a pale yellow glint, tan at the right
TROPHY_BOWL = {"t": "lathe", "x": 5.0, "prof": [[0.0, 3.0], [4.2, 3.0], [5.3, 2.0], [6.0, 1.0]]}
METAL = {"linear": {"a": [2.0, 0], "b": [8.0, 0], "stops": [[0, [120, 103, 72]], [0.28, [238, 242, 233]], [0.5, [184, 200, 136]],
                                                          [0.72, [240, 245, 160]], [1.0, [140, 128, 88]]]}}
HANDLE = C.pline([[2.2, 1.5], [0.5, 1.6], [0.5, 3.4], [1.3, 4.5], [2.2, 4.5]], 1.0, smooth=1)
E("gb-trophy", 33, "trophy cup on axis x 5: a bowl (one profile mirrored, 6 px wide, narrowing under row 4) on a 2 px stem, a "
  "4 px collar, a 6 px foot and an 8 px dark base; a 1 px C handle each side (tan left, dark right); metal paint: olive edge, "
  "white highlight left of centre, sage, a pale glint, tan",
  lambda: item([lay(HANDLE, [120, 103, 72]), lay(mirror(HANDLE, 5.0), [59, 64, 32]),
                lay(TROPHY_BOWL, METAL), lay(rect(2.0, 0.0, 8.0, 1.0), {"linear": {"a": [2, 0], "b": [8, 0], "stops": [
                    [0, [41, 37, 23]], [0.3, [175, 205, 129]], [0.6, [210, 222, 152]], [1.0, [56, 34, 21]]]}}),
                lay(rect(4.0, 5.6, 6.0, 7.2), METAL),
                lay(rect(3.0, 7.0, 7.0, 8.0), {"linear": {"a": [3, 0], "b": [7, 0], "stops": [[0, [31, 9, 3]], [0.35, [196, 189, 160]],
                                                                                            [0.7, [175, 205, 129]], [1.0, [56, 34, 21]]]}}),
                lay(rect(2.0, 8.0, 8.0, 9.0), {"linear": {"a": [2, 0], "b": [8, 0], "stops": [[0, [27, 13, 8]], [0.3, [120, 112, 113]],
                                                                                            [0.45, [255, 255, 255]], [0.65, [170, 190, 140]],
                                                                                            [1.0, [27, 13, 8]]]}}),
                lay(rect(1.0, 9.0, 9.0, 10.0), [36, 34, 40])]))

# 34 white cup: a goblet on axis x 4.5 seen a little from above: a 9 px rim two rows deep (white top edge, pink inside), a light
# grey inner lip, a 7 px bowl narrowing to a 3 px stem, a 7 px foot; white down the lit left, pink, grey-brown on the right;
# a dark olive copy one pixel down-right as the shadow
GOBLET = {"t": "lathe", "x": 4.5, "prof": [[0.0, 4.6], [2.0, 4.6], [2.0, 3.5], [4.8, 3.5], [6.0, 1.5], [8.0, 1.5], [8.0, 3.5],
                                          [9.0, 3.5]]}
CUPPAINT = {"linear": {"a": [1.0, 0], "b": [8.0, 0], "stops": [[0, [255, 255, 255]], [0.14, [255, 255, 255]], [0.2, [248, 216, 248]],
                                                               [0.82, [248, 216, 248]], [1.0, [179, 167, 151]]]}}
E("gb-white-cup", 34, "goblet on axis x 4.5 (one profile mirrored): a 9 px rim two rows deep, a 7 px bowl narrowing to a 3 px "
  "stem, a 7 px foot; white top edge, pink inside the rim, light grey inner lip; white down the lit left, pink, grey-brown on "
  "the right; dark olive shadow one pixel down-right",
  lambda: item([lay(mv(GOBLET), [104, 104, 80]), lay(GOBLET, CUPPAINT),
                lay(rect(0.0, 0.0, 9.6, 1.0), [255, 255, 255]),
                lay(rect(0.0, 1.0, 9.0, 2.0), {"linear": {"a": [0, 0], "b": [9, 0], "stops": [[0, [248, 216, 248]], [0.85, [248, 216, 248]],
                                                                                             [1.0, [179, 167, 151]]]}}),
                lay(rect(2.0, 2.0, 6.0, 3.0), [204, 207, 209])]))


# 20 flag pin: a white post (grey shaded side, dark shadow, cream foot) carrying a signboard to its right: a grey top edge, a
# pink band, white, a grey band, a red-orange-yellow stripe over a salmon stripe, a dark right edge and a shadow under it
def flag_pin():
    return item([
        lay(rect(3, 3, 4, 9), [57, 73, 61]),                                   # the post's shadow on the board and below
        lay(rect(3, 0.6, 10, 6), [243, 233, 228]),                             # board
        lay(rect(4, 0, 10, 1), {"linear": {"a": [4, 0], "b": [10, 0], "stops": [[0, [130, 131, 131]], [0.4, [253, 254, 205]],
                                                                                  [0.55, [130, 131, 131]], [1, [130, 131, 131]]]}}),
        lay(rect(5, 1, 8, 2), [247, 191, 178]),                                # pink band
        lay(rect(4, 3, 9, 4), [125, 118, 118]),                                # grey band
        lay(rect(4, 4, 9, 5), {"linear": {"a": [4.5, 0], "b": [8.5, 0], "stops": [[0, [251, 215, 223]], [0.25, [255, 20, 0]],
                                                                                  [0.5, [255, 154, 0]], [0.75, [255, 212, 6]],
                                                                                  [1, [253, 252, 154]]]}}),
        lay(rect(5, 5, 8, 6), [246, 158, 140]),                                # salmon stripe
        lay(rect(9, 1, 10, 6), [72, 76, 81]),                                  # dark right edge
        lay(rect(4, 6, 10, 7), [57, 73, 61]),                                  # shadow under the board
        lay(rect(1, 0.5, 2, 8.5), [243, 233, 228]),                            # post
        lay(rect(2, 1, 3, 8), [130, 131, 131]),                                # its shaded side
        lay(rect(0, 8, 3, 9, 0.3), [253, 254, 205])])                          # foot


E("gb-flag-pin", 20, "signboard on a post: a 1 px white post (grey shaded side, dark shadow, cream foot) and a 7 x 6 px white "
  "board on its right with a grey top edge, a pink band, a grey band, a red-orange-yellow stripe over a salmon stripe, a dark "
  "right edge and a shadow under it", flag_pin,
  unsure="what the board shows is not clear at 9 px (the roster calls it a flag pin; the stripes could be a hot dog on a "
         "snack-stand sign); drawn as read, mostly rectangles")
