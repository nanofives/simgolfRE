"""GBUBBLES cells 5-16: a flat 15 px disc (pink or sage) with a dark copy of itself one pixel down-right as its shadow, and
a black glyph. Every number is read from the cell's pixel grid (log/roadmap/P41/work/marks/read/gb-NN.txt): crop pixel
(i, j) covers [i, i+1) x [j, j+1).
"""
import common as C

PINK = [168, 128, 136]
SAGE = [184, 200, 136]
SHADOW = [24, 24, 24]
BLACK = [0, 0, 0]
DISC = {"t": "circle", "c": [7.5, 7.5], "r": 7.5}         # rows 0..14 / cols 0..14 (5, 9, 11, 13, 13, 15 px wide rows)


def disc(col, glyph_layers, shadow=True):
    L = []
    if shadow:
        L.append({"shape": {"t": "move", "of": DISC, "d": [1, 1]}, "fill": SHADOW})
    L.append({"shape": DISC, "fill": col})
    L += glyph_layers
    return [{"box": [-1, -1, 17, 17], "layers": L}]


def ink(shape, col=BLACK):
    return {"shape": shape, "fill": col}


def cell(i):
    return (C.GB, 16 * i, 0x144, 16, 16)


ELEMENTS = {}


def E(eid, i, how, fn, unsure="", z=20):
    ELEMENTS[eid] = {"how": how, "unsure": unsure, "z": z, "states": [("idle",) + (cell(i)[0], cell(i)[1:], fn)]}


# ---- arrows: a right-angle triangular head and a 3 px shaft that rises straight and bends 45 degrees into the head.
ARROW_UL_HEAD = {"t": "poly", "rings": [[[4, 3], [11.3, 3], [4, 10.3]]]}           # corner at the top left
ARROW_UL_SHAFT = {"t": "poly", "rings": [[[8, 13], [11, 13], [11, 7.8], [6.5, 3.3], [6.5, 7.54], [8, 9.04]]]}


def mirror(S, ax=7.5):
    return {"t": "poly", "rings": [[[2 * ax - x, y] for x, y in r] for r in S["rings"]]}


E("gb-arrow-up-left", 5, "pink disc + shadow disc; arrow: right-angle triangular head (corner up-left) and a 3 px shaft rising "
  "from the bottom that bends 45 degrees into the head", lambda: disc(PINK, [ink(ARROW_UL_HEAD), ink(ARROW_UL_SHAFT)]))
E("gb-arrow-up-right", 6, "pink disc + shadow disc; the up-left arrow mirrored about the disc's axis (the art is its exact mirror)",
  lambda: disc(PINK, [ink(mirror(ARROW_UL_HEAD)), ink(mirror(ARROW_UL_SHAFT))]))

# ---- plus / minus: 3 px bars, 9 px long, flat ends
E("gb-plus", 7, "pink disc + shadow disc; plus: two 3 x 9 px bars with flat ends",
  lambda: disc(PINK, [ink({"t": "union", "of": [{"t": "rrect", "c": [7.5, 7.5], "half": [1.5, 4.5]},
                                                 {"t": "rrect", "c": [7.5, 7.5], "half": [4.5, 1.5]}]})]))
E("gb-minus", 8, "pink disc + shadow disc; minus: one 9 x 3 px bar with flat ends",
  lambda: disc(PINK, [ink({"t": "rrect", "c": [7.5, 7.5], "half": [4.5, 1.5]})]))

# ---- flag: a 1 px pole and a pennant flying right whose lower edge dips at the tip
E("gb-flag", 9, "sage disc + shadow disc; golf flag: 1 px pole (rows 2-12) and a pennant flying right, its tip dipping",
  lambda: disc(SAGE, [ink(C.bar([6.5, 2.0], [6.5, 13.0], 1.0)),
                      ink({"t": "poly", "rings": [[[6.5, 2.6], [8.6, 2.9], [11.2, 5.4], [11.1, 7.0], [9.0, 6.7], [6.5, 6.0]]], "smooth": 1})]))

# ---- speech bubble: an ellipse with a short tail down and to the right
E("gb-speech-bubble", 10, "sage disc + shadow disc; speech balloon: an 11 x 7 px ellipse and a tail pointing down-right of centre",
  lambda: disc(SAGE, [ink({"t": "union", "of": [{"t": "ellipse", "c": [7.5, 6.6], "r": [5.6, 3.6]},
                                                 {"t": "poly", "rings": [[[7.0, 8.6], [10.6, 8.6], [8.4, 12.0]]]}]})]))

# ---- trend line: a 1 px zig-zag rising to the right (a price chart)
E("gb-trend-line", 11, "sage disc + shadow disc; price chart: a 1 px zig-zag line rising left to right (up, dip, up, dip, up)",
  lambda: disc(SAGE, [ink(C.pline([[1.2, 10.8], [5.5, 7.4], [6.5, 8.3], [8.5, 6.4], [10.5, 8.4], [13.8, 5.2]], 1.0))]))

# ---- dollar: a 2 px S with two 1 px bars through it that stick out a pixel above and below
DOLLAR_S = C.pline([[12.0, 4.0], [8.5, 3.9], [5.0, 4.1], [3.9, 5.3], [4.6, 6.6], [7.5, 7.1], [10.4, 7.5], [11.3, 8.8],
                                   [10.6, 10.3], [7.0, 10.6], [3.0, 10.6]], 2.0, smooth=2)
E("gb-dollar", 12, "sage disc + shadow disc; dollar sign: a 2 px S with flat ends and two 1 px vertical bars through it, "
  "one pixel beyond the S above and below",
  lambda: disc(SAGE, [ink(DOLLAR_S), ink(C.bar([6.5, 2], [6.5, 13], 1.0)),
                      ink(C.bar([8.5, 2], [8.5, 13], 1.0))]))

# ---- info: a 3 x 2 dot over a 3 x 6 stem
E("gb-info-i", 15, "sage disc + shadow disc; info letter i: a 3 x 2 px dot over a 3 x 6 px stem, flat ends",
  lambda: disc(SAGE, [ink({"t": "rrect", "c": [7.5, 4.0], "half": [1.5, 1.0], "r": 0.3}),
                      ink({"t": "rrect", "c": [7.5, 9.0], "half": [1.5, 3.0]})]))

# ---- question mark: a 3 px hook (flat end at the lower left) that narrows into a short stem, and a 3 x 2 dot
E("gb-question-mark", 16, "sage disc + shadow disc; question mark: a 3 px hook from a flat end at its lower left over the top "
  "and down the right into a short stem, and a 3 x 2 px dot",
  lambda: disc(SAGE, [ink(C.pline([[4.5, 6.0], [4.5, 4.4], [7.5, 3.8], [10.4, 4.4], [10.5, 6.0], [9.0, 7.4], [7.6, 8.6],
                                                  [7.5, 10.0]], 2.7, smooth=1)),
                      ink({"t": "rrect", "c": [7.5, 12.0], "half": [1.5, 1.0], "r": 0.3})]),
  unsure="the art's shadow skips rows 5-9 on this one cell; drawn with the same shadow as every other disc")

# ---- note page: a page in a 1 px black frame (dark grey, shaded warmer at the lower right) with three light text lines on
# its left, and a big dark grey question mark over it whose hook rises above the frame and whose dot sits below it; inside
# the frame the question mark has a 1 px black outline (it separates it from the text lines and from the dot)
PAGE = {"t": "rrect", "c": [7.5, 7.5], "half": [5.5, 3.5]}                 # x 2..13, y 4..11
PAGE_IN = {"t": "rrect", "c": [7.5, 7.5], "half": [4.5, 2.5]}              # x 3..12, y 5..10
Q_HOOK = C.pline([[4.5, 4.9], [4.6, 3.4], [7.5, 2.7], [10.3, 3.3], [10.5, 5.2], [8.7, 6.6], [7.5, 7.6], [7.5, 9.0]], 2.1, smooth=1)
Q_DOT = {"t": "rrect", "c": [7.5, 11.0], "half": [1.5, 1.0]}
QGREY = [71, 75, 71]


def note_page():
    return disc(SAGE, [
        ink(PAGE, BLACK),
        ink(PAGE_IN, {"linear": {"a": [7.0, 6.0], "b": [11.5, 9.5], "stops": [[0, [71, 75, 71]], [1.0, [86, 72, 72]]]}}),
        ink({"t": "rrect", "c": [5.0, 7.5], "half": [2.0, 2.5]}, [191, 191, 191]),                     # text block x 3..7
        ink(C.bar([3.0, 6.5], [6.6, 6.5], 1.0), [120, 112, 113]), ink(C.bar([3.0, 8.5], [6.6, 8.5], 1.0), [120, 112, 113]),
        {"shape": {"t": "union", "of": [{"t": "grow", "of": Q_HOOK, "d": 0.75}, {"t": "grow", "of": Q_DOT, "d": 0.75}]},
         "fill": BLACK, "clip": PAGE_IN},
        ink(Q_HOOK, QGREY), ink(Q_DOT, QGREY)])


E("gb-note-page", 14, "sage disc + shadow disc; a page in a 1 px black frame (11 x 7 px, dark grey shaded warmer to the lower "
  "right) with a light grey text block crossed by two mid-grey lines on its left, and a big dark grey question mark (2.2 px "
  "hook rising above the frame, dot below it) outlined 1 px black inside the frame", note_page,
  unsure="read as 'a note with a question mark' (an unread hint); the art's shading inside the page is a few pixels only")
