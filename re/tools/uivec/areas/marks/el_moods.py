"""GBUBBLES row 0x144 cells 0-4: the golfer mood faces (furious devil, unhappy, neutral, happy, delighted). Each is a 15 px
face disc shaded from a bright centre to a dark rim, with features in dark ink. Read from read/gb-mood-N.txt; crop pixel
(i, j) covers [i, i+1) x [j, j+1). The features are as the art places them (the pixel art is not exactly symmetric: the
unhappy and neutral eyes sit half a pixel right of the disc's axis).
"""
import common as C
from el_loose import lay, rect, mirror

STATES = ("furious", "unhappy", "neutral", "happy", "delighted")


def face(c, r, stops):
    S = {"t": "circle", "c": c, "r": r}
    return lay(S, {"radial": {"c": [c[0] - 1.6, c[1] - 2.0], "r": r + 2.4, "stops": stops}})


def item(layers):
    return [{"box": [-1, -1, 17, 17], "layers": layers}]


def ell(cx, cy, rx, ry, rot=0.0):
    return {"t": "ellipse", "c": [cx, cy], "r": [rx, ry], "rot": rot}


def furious():
    horn = {"t": "poly", "rings": [[[1.1, 3.0], [2.0, 0.0], [3.8, 0.0], [3.9, 2.2]]]}     # a short point rising off the head
    L = [lay(horn, [136, 16, 0]), lay(mirror(horn, 7.0), [136, 16, 0]),
         face([7.5, 8.0], 7.0, [[0, [245, 0, 0]], [0.5, [225, 0, 0]], [0.8, [190, 8, 0]], [1.0, [120, 16, 8]]])]
    ink = [40, 24, 24]
    brow = C.pline([[1.9, 4.2], [5.7, 5.6]], 1.2)
    L += [lay(brow, ink), lay(mirror(brow, 7.0), ink)]
    for x, col in ((4.5, [130, 131, 131]), (5.5, [18, 17, 33]), (8.5, [18, 17, 33]), (9.5, [130, 131, 131])):
        L.append(lay(rect(x - 0.5, 6.0, x + 0.5, 7.0, 0.25), col))
    mouth = rect(2.2, 8.0, 12.0, 12.0, 1.6)
    teeth = rect(3.2, 9.0, 11.0, 11.0, 0.5)
    L += [lay(mouth, [80, 40, 56]), lay(teeth, [204, 207, 209])]
    L += [lay(C.bar([3.2, 10.0], [11.0, 10.0], 0.5), [130, 131, 131])]
    for x in (5.0, 7.0, 9.0):
        L.append(lay(C.bar([x, 9.0], [x, 10.0], 0.6), [130, 131, 131]))
    for x in (4.0, 6.0, 8.0, 10.0):
        L.append(lay(C.bar([x, 10.0], [x, 11.0], 0.6), [130, 131, 131]))
    return item(L)


def unhappy():
    L = [face([7.5, 7.5], 7.5, [[0, [210, 84, 0]], [0.6, [200, 72, 0]], [0.82, [176, 40, 0]], [1.0, [130, 16, 0]]])]
    for cx in (5.5, 10.5):
        L.append(lay(rect(cx - 1.5, 3.0, cx + 1.5, 6.0, 0.9), [22, 20, 24]))
    L.append(lay(C.pline([[3.5, 11.7], [4.6, 9.4], [6.5, 8.5], [9.5, 8.5], [11.4, 9.4], [12.5, 11.7]], 1.0, smooth=1), [62, 40, 18]))
    L.append(lay(C.bar([6.0, 10.5], [10.0, 10.5], 1.0), [120, 80, 8]))
    return item(L)


def neutral():
    L = [face([7.5, 7.5], 7.5, [[0, [230, 253, 0]], [0.45, [214, 236, 0]], [0.78, [190, 200, 0]], [1.0, [160, 168, 0]]])]
    for cx in (5.0, 11.0):
        L += [lay(ell(cx, 5.5, 1.9, 2.3), [112, 150, 12]), lay(ell(cx, 5.5, 1.05, 1.55), [20, 18, 28])]
    L.append(lay(C.bar([5.0, 8.5], [11.0, 8.5], 1.0), [241, 253, 77]))
    L.append(lay(C.bar([3.0, 10.5], [13.0, 10.5], 1.0), [88, 76, 18]))
    return item(L)


def happy():
    L = [face([7.5, 7.5], 7.5, [[0, [140, 234, 0]], [0.5, [128, 228, 0]], [0.78, [80, 205, 0]], [1.0, [41, 150, 4]]])]
    for cx in (5.0, 10.0):
        L += [lay(ell(cx, 5.0, 1.8, 1.7), [45, 105, 14]), lay(ell(cx, 4.9, 1.2, 1.0), [24, 26, 44])]
    L.append(lay(C.pline([[2.2, 8.0], [3.8, 9.8], [5.8, 11.2], [7.5, 11.5], [9.2, 11.2], [11.2, 9.8], [12.8, 8.0]], 1.0, smooth=1),
                 [28, 42, 40]))
    return item(L)


def delighted():
    L = [face([7.3, 7.5], 7.4, [[0, [20, 255, 0]], [0.55, [0, 250, 0]], [0.8, [40, 214, 0]], [1.0, [8, 176, 0]]])]
    for cx in (4.5, 9.5):
        L += [lay(ell(cx, 4.4, 1.9, 2.8), [30, 110, 16]), lay(ell(cx, 4.4, 1.25, 2.1), [26, 22, 22]),
              lay(ell(cx, 4.5, 0.55, 0.55), [130, 131, 131])]
    lower = {"t": "half", "p": [7.0, 8.2], "n": [0, -1]}
    L.append(lay({"t": "inter", "of": [ell(7.0, 8.2, 3.7, 4.4), lower]}, [56, 64, 48]))
    L.append(lay({"t": "inter", "of": [ell(7.0, 8.9, 2.9, 3.3), {"t": "half", "p": [7.0, 9.0], "n": [0, -1]}]}, [204, 207, 209]))
    L.append(lay(rect(5.0, 9.0, 9.0, 10.0), [249, 249, 231]))
    L.append(lay(rect(6.0, 11.0, 8.0, 12.0), [249, 249, 231]))
    corner = C.pline([[1.2, 9.2], [3.6, 10.2]], 1.0)
    L += [lay(corner, [40, 56, 36]), lay(mirror(corner, 7.0), [40, 56, 36])]
    return item(L)


ELEMENTS = {"gb-mood": {
    "how": "five mood faces, each a 15 px disc shaded bright centre to dark rim: furious = red, two horns, brows slanting down "
           "to the middle, grey eye + dark pupil each, a rounded mouth of clenched teeth (light grey, grey grid); unhappy = "
           "orange, two dark 3 px eyes, a frown arc and a brown lower lip; neutral = yellow, dark eyes in olive sockets, a "
           "light line and a flat 10 px mouth; happy = green, eyes in dark sockets, a smile arc; delighted = bright green, "
           "tall eyes with a grey glint, an open grin (dark rim, white upper and lower teeth) with smile lines at the corners",
    "unsure": "pixel art at 15 px: the unhappy and neutral features sit half a pixel right of the face's axis, drawn as read",
    "z": 20,
    "states": [(s, C.GB, (16 * i, 0x144, 16, 16), fn) for i, (s, fn) in enumerate(zip(STATES, (furious, unhappy, neutral, happy, delighted)))]}}
