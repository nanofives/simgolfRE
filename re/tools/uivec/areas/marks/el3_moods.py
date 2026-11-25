"""Fix round 3: GBUBBLES row 0x144 cells 0-4, the golfer mood faces (furious, unhappy, neutral, happy, delighted).

Rejected twice with no note. Round 2 beside the art at 20-32x (work3/marks/read/moods_all.png), the five largest
differences, which both versions remove:
 1. features too big and too regular: the art's eyes are 2 x 2 to 3 x 5 pixel blocks (round 2: 4 x 5 smooth ellipses);
 2. flat smooth fills: the art paints each face in three or four tones in patches (a 1 px darker rim all round, a lit ridge,
    lit cheeks and chin, shade beside the nose and round the features); round 2 had one soft radial per face;
 3. the horns: the art's are 2 px hooks inside the head's height (tips on row 0, curling inward); round 2's stuck out above;
 4. the mouths: the art's are compact and 1 px lines of varying darkness (a frown of a 4 px bar with corners dropping
    to row 11, a cup smile that ends level with the cheeks), round 2's were wide, thick, even arcs;
 5. invented parts: round 2 gave the unhappy face long slanted brows (the art has two 2 px red dashes, level, over the eyes'
    inner halves) and a single neutral ridge; the art's glints sit on the same side in both eyes (not mirrored).
Version A: every patch of the art's palette grid (read/*.txt) as its own shape (fix3.patch), in the art's colour.
Version B: a clean drawing at the art's sizes and places, with the tones baked as layers (rim band, ridge, cheek and chin
lights, sockets, glints, lips with thickness).
"""
import numpy as np

import common as C
import fix3
from fix2 import U, circ, ell, inter, item, lay, mirror_layers, poly, rect, sub, vline

STATES = ("furious", "unhappy", "neutral", "happy", "delighted")


def cell(k):
    return (16 * k, 0x144, 16, 16)


# ----------------------------------------------------------------------------------------------------- version A
def faithful(k):
    """Every same-colour patch of the art (8-connected) as its own shape in that colour; the base colour (the commonest)
    is the whole silhouette underneath."""
    x, y, w, h = cell(k)
    rgb, m, idx = C.sheet(C.GB)
    I = idx[y:y + h, x:x + w]
    pal = rgb[y:y + h, x:x + w]
    vals, cnt = np.unique(I[m[y:y + h, x:x + w]], return_counts=True)
    base = vals[np.argmax(cnt)]
    col = {v: [int(c) for c in pal[I == v][0]] for v in vals}
    M = m[y:y + h, x:x + w]
    items = item([lay(fix3.patch(M, grow=0.0), col[base])])
    parts = []
    for v in vals:
        if v == base:
            continue
        for p in fix3.components(M & (I == v), 8):
            parts.append((len(p), v, p))
    parts.sort(key=lambda t: -t[0])
    for n, v, p in parts:                  # each patch its own item, boxed tight (renders only where it is)
        jj, ii = zip(*p)
        items += item([lay(fix3.patch(fix3.part_mask(p, M.shape), smooth=1, grow=0.1), col[v])],
                      box=(min(ii) - 1, min(jj) - 1, max(ii) + 2, max(jj) + 2))
    return items


# ----------------------------------------------------------------------------------------------------- version B
def both(layers, ax):
    return layers + mirror_layers(layers, ax)


def soft(shape, col, s=0.5, a=1.0, clip=None):
    L = lay(shape, col, soft=s, alpha=a)
    if clip is not None:
        L["clip"] = clip
    return L


def rimband(face, w, stops_a, stops_b, stops):
    """A band w px wide inside the face's edge, painted with a linear gradient (a -> b), its inner edge soft."""
    band = sub(face, {"t": "grow", "of": face, "d": -w})
    return {"shape": band, "fill": {"linear": {"a": stops_a, "b": stops_b, "stops": stops}}, "soft": 0.45, "clip": face}


def furious_b():
    head = ell(7.5, 8.0, 7.45, 6.95)
    horn = poly([[0.85, 3.3], [0.9, 1.5], [1.55, 0.45], [2.5, 0.0], [3.95, 0.05], [3.25, 0.95], [3.0, 1.9], [3.6, 2.9]], smooth=1)
    horn_lit = inter(horn, ell(2.6, 2.1, 0.9, 1.3))                                 # its inner side catches the light
    L = both([lay(horn, [136, 16, 0]), lay(horn_lit, [200, 0, 0], soft=0.5)], 7.0)
    L.append(lay(head, {"radial": {"c": [8.6, 6.6], "r": 8.4, "stops": [[0, [232, 0, 0]], [0.45, [204, 0, 0]],
                                                                          [0.8, [184, 12, 0]], [1, [150, 16, 0]]]}}))
    L.append(rimband(head, 1.0, [3.0, 4.0], [11.0, 13.0], [[0, [150, 16, 0]], [0.5, [170, 16, 0]], [1, [120, 16, 4]]]))
    L.append(soft(rect(5.9, 3.3, 8.1, 8.0, 1.0), [240, 0, 0], 0.5, 0.95, head))                       # forehead / nose ridge
    L.append(soft(ell(12.3, 6.9, 1.3, 1.6), [240, 0, 0], 0.5, 1.0, head))                              # right cheek (lit)
    L.append(soft(ell(2.3, 6.5, 0.8, 0.8), [236, 0, 0], 0.45, 0.8, head))                              # left cheek
    L += both([soft(vline([[3.1, 3.4], [4.6, 4.4]], 1.0), [80, 16, 16], 0.45, 0.8),                     # shade over the brow
               lay(vline([[1.9, 4.5], [3.4, 5.0], [4.8, 5.6], [5.7, 6.3]], [1.15, 1.2, 1.1, 0.9]), [30, 20, 28]),  # brow
               soft(rect(2.9, 5.9, 3.9, 7.0, 0.4), [136, 16, 0], 0.4, 0.9),                            # socket shade
               lay(ell(4.55, 6.5, 0.62, 0.5), [140, 141, 141]),                                        # eye white
               lay(circ(5.45, 6.5, 0.5), [18, 17, 33])], 7.0)                                         # pupil
    mouth = poly([[4.0, 8.0], [10.0, 8.0], [11.0, 9.0], [12.0, 10.0], [11.0, 11.0], [10.4, 12.0], [3.6, 12.0], [3.0, 11.0],
                  [2.0, 10.0], [3.0, 9.0]], smooth=2)
    teeth = poly([[4.0, 9.0], [10.0, 9.0], [11.0, 10.0], [11.0, 11.0], [3.0, 11.0], [3.0, 10.0]], smooth=1)
    L += [soft(rect(4.6, 12.0, 9.8, 13.0, 0.5), [136, 16, 0], 0.45, 0.9, head),                        # shadow under the mouth
          soft(rect(5.7, 13.0, 8.3, 14.0, 0.5), [240, 0, 0], 0.4, 0.9, head),                          # lit chin
          lay(mouth, {"linear": {"a": [0, 8.0], "b": [0, 12.0], "stops": [[0, [80, 40, 56]], [0.55, [80, 40, 56]],
                                                                          [0.75, [80, 16, 16]], [1, [80, 16, 16]]]}}),
          lay(teeth, {"linear": {"a": [0, 9.0], "b": [0, 11.0], "stops": [[0, [214, 216, 218]], [1, [194, 196, 198]]]}})]
    # clenched teeth: the gaps between them are the art's grey pixels; the upper row's teeth sit between the lower row's
    gap = [130, 131, 131]
    L.append(lay(vline([[3.2, 10.0], [10.8, 10.0]], 0.35), gap, clip=teeth))
    for gx in (4.5, 6.5, 8.5):
        L.append(lay(rect(gx - 0.42, 8.8, gx + 0.42, 10.0, 0.15), gap, clip=teeth))
    for gx in (3.5, 5.5, 7.5, 9.5):
        L.append(lay(rect(gx - 0.42, 10.0, gx + 0.42, 11.2, 0.15), gap, clip=teeth))
    return item(L)


def unhappy_b():
    face = circ(7.5, 7.5, 7.5)
    L = [lay(face, {"radial": {"c": [7.6, 6.8], "r": 8.2, "stops": [[0, [204, 76, 0]], [0.62, [200, 72, 0]],
                                                                     [0.85, [182, 60, 8]], [1, [150, 30, 4]]]}})]
    L.append(rimband(face, 1.05, [2.0, 2.0], [13.0, 13.0], [[0, [136, 16, 0]], [0.6, [140, 30, 10]], [1, [150, 36, 16]]]))
    L.append(soft(ell(14.4, 7.4, 0.9, 1.6), [176, 16, 0], 0.4, 0.9, face))                             # right rim, redder
    L.append(soft(ell(9.5, 13.6, 2.8, 0.6), [144, 64, 32], 0.4, 0.95, face))                             # brown shade at the chin
    L += both([soft(circ(5.5, 1.5, 0.55), [176, 88, 0], 0.4, 0.9),                                     # lit forehead above the brow
               lay(vline([[5.0, 2.5], [6.95, 2.5]], 0.95), [176, 16, 0]),                               # brow: a short level dash
               soft(rect(4.0, 6.0, 7.0, 7.0, 0.5), [176, 88, 0], 0.35, 1.0),                            # lower lid, lit
               lay(rect(4.0, 3.0, 7.0, 6.0, 0.75), [12, 11, 12]),                                        # eye
               soft(ell(4.6, 3.4, 0.5, 0.4), [80, 16, 16], 0.35, 0.9)], 8.0)                             # its maroon upper corner
    L += [lay(circ(5.5, 4.5, 0.45), [22, 22, 40]), lay(circ(10.5, 4.5, 0.45), [22, 22, 40]),             # navy gleam (same side)
          soft(ell(6.5, 5.6, 0.5, 0.4), [72, 56, 16], 0.3, 0.9), soft(ell(11.4, 5.4, 0.45, 0.45), [40, 30, 22], 0.3, 0.9)]
    L.append(soft(rect(6.0, 9.0, 10.0, 10.0, 0.4), [176, 88, 0], 0.35, 0.8))                            # lit under the bar
    L.append(lay(rect(6.0, 9.9, 10.0, 10.95, 0.45), [120, 80, 8]))                                      # lower lip
    L.append(lay(vline([[3.4, 11.6], [3.9, 10.5], [4.7, 9.5], [5.7, 8.65], [8.0, 8.5], [10.3, 8.65], [11.3, 9.5],
                        [12.1, 10.5], [12.6, 11.6]], [0.85, 1.0, 1.0, 1.0, 1.05, 1.0, 1.0, 1.0, 0.85]),
                  {"linear": {"a": [5.0, 0], "b": [11.0, 0], "stops": [[0, [36, 24, 22]], [0.25, [72, 56, 16]],
                                                                       [0.75, [72, 56, 16]], [1, [34, 26, 20]]]}}))  # frown
    L += both([soft(ell(5.9, 11.5, 0.9, 0.45), [176, 88, 0], 0.3, 0.9)], 8.0)                           # lit chin either side
    return item(L)


def neutral_b():
    face = circ(7.5, 7.5, 7.5)
    L = [lay(face, {"radial": {"c": [8.0, 6.0], "r": 8.4, "stops": [[0, [214, 238, 0]], [0.55, [210, 226, 2]],
                                                                     [0.85, [190, 200, 0]], [1, [170, 178, 0]]]}})]
    L.append(rimband(face, 1.05, [0.0, 7.0], [15.0, 9.0], [[0, [120, 175, 0]], [0.35, [188, 198, 0]], [1, [166, 173, 0]]]))
    L.append(soft(ell(0.6, 7.5, 0.9, 1.6), [120, 175, 0], 0.4, 1.0, face))                             # green left rim
    L.append(soft(rect(6.0, 13.0, 10.0, 15.0), [166, 173, 0], 0.4, 0.9, face))                         # darker bottom
    L.append(soft(rect(7.0, 1.6, 9.0, 8.0, 0.8), [226, 253, 0], 0.4, 1.0, face))                       # nose ridge
    L.append(soft(rect(1.5, 9.0, 14.5, 10.0), [188, 198, 0], 0.3, 1.0, face))                          # shade band over the mouth
    L.append(lay(rect(1.8, 8.0, 13.2, 9.0, 0.45), [226, 253, 0], clip=face))                           # cheek band under the eyes
    L.append(lay(rect(5.0, 8.0, 11.0, 9.0, 0.45), [241, 253, 77]))                                      # its brightest middle
    L += both([lay(rect(3.9, 3.0, 6.1, 8.0, 0.8), [120, 175, 0]),                            # socket
               lay(rect(3.05, 4.0, 4.1, 7.0, 0.45), [86, 80, 20]),                                       # olive sides
               lay(rect(5.9, 4.0, 6.95, 7.0, 0.45), [112, 98, 14]),
               lay(rect(4.0, 4.0, 6.0, 7.0, 0.7), [22, 22, 24])], 8.0)                                   # eye
    L += [lay(rect(4.1, 4.0, 5.9, 4.95, 0.45), [18, 17, 33]), lay(rect(10.1, 4.0, 11.9, 4.95, 0.45), [18, 17, 33]),  # navy top
          soft(circ(3.5, 5.5, 0.45), [85, 42, 52], 0.3, 0.9), soft(circ(9.5, 5.5, 0.45), [85, 42, 52], 0.3, 0.9)]
    L.append(lay(vline([[2.6, 10.5], [13.4, 10.5]], 1.0), [120, 175, 0]))                               # mouth ends in shade
    L.append(lay(vline([[3.5, 10.5], [12.5, 10.5]], 1.0), {"linear": {"a": [3.5, 0], "b": [12.5, 0], "stops": [
        [0, [80, 71, 19]], [0.15, [93, 80, 18]], [0.8, [93, 80, 18]], [1, [80, 71, 19]]]}}))             # mouth
    L.append(soft(rect(5.6, 12.0, 10.4, 13.6, 0.7), [226, 253, 0], 0.4, 1.0, face))                    # lit chin
    return item(L)


def happy_b():
    face = circ(7.5, 7.5, 7.5)
    L = [lay(face, {"radial": {"c": [7.4, 6.2], "r": 8.2, "stops": [[0, [134, 232, 0]], [0.5, [132, 230, 0]],
                                                                     [0.72, [91, 216, 0]], [1, [45, 184, 0]]]}})]
    L.append(rimband(face, 1.0, [1.0, 2.0], [14.0, 13.0], [[0, [41, 154, 4]], [0.5, [45, 184, 0]], [1, [41, 154, 4]]]))
    L += both([soft(vline([[6.5, 3.3], [6.5, 6.6]], 0.9), [121, 141, 1], 0.35, 1.0, face)], 7.5)        # olive beside the nose
    L.append(soft(vline([[7.5, 2.0], [7.5, 9.5]], 0.9), [134, 232, 0], 0.3, 1.0, face))                # nose ridge
    for ex in (4.75, 9.75):
        L += [soft(ell(ex, 4.9, 1.7, 1.75), [53, 120, 16], 0.4, 1.0),                                   # socket
              lay(ell(ex, 5.35, 1.05, 0.95), [27, 72, 16]),                                              # its dark lower half
              lay(rect(ex - 1.0, 4.0, ex + 1.0, 5.95, 0.6), [24, 27, 46]),                               # eye
              lay(rect(ex - 1.0, 4.95, ex + 0.1, 5.95, 0.4), [32, 26, 22]),
              lay(circ(ex + 0.55, 5.45, 0.42), [80, 40, 64])]                                            # purple glint, same side
    L.append(soft(ell(7.3, 10.3, 2.0, 0.55), [121, 141, 1], 0.3, 1.0))                                 # upper lip shade
    L.append(soft(circ(5.5, 10.4, 0.45), [88, 65, 16], 0.3, 0.9))
    L.append(lay(vline([[2.4, 8.5], [3.5, 9.5], [4.6, 10.45], [6.0, 11.35], [7.5, 11.55], [9.0, 11.35], [10.5, 10.45],
                        [11.5, 9.5], [12.4, 8.5]], [0.75, 0.95, 1.0, 1.0, 1.0, 1.0, 1.0, 0.95, 0.75]),
                  {"linear": {"a": [2.0, 0], "b": [13.0, 0], "stops": [[0, [27, 72, 16]], [0.25, [24, 27, 48]],
                                                                       [0.5, [45, 48, 57]], [0.75, [40, 24, 24]], [1, [27, 72, 16]]]}}))
    L.append(soft(rect(4.6, 12.0, 11.4, 13.0, 0.5), [91, 216, 0], 0.4, 0.9, face))                     # shade under the smile
    L.append(soft(ell(6.9, 13.5, 1.6, 0.55), [134, 232, 0], 0.4, 1.0, face))                           # lit chin
    return item(L)


def delighted_b():
    face = circ(7.3, 7.5, 7.4)
    L = [lay(face, {"radial": {"c": [7.0, 5.8], "r": 8.4, "stops": [[0, [0, 255, 0]], [0.6, [4, 250, 0]],
                                                                     [0.82, [48, 216, 0]], [1, [8, 184, 0]]]}})]
    L.append(rimband(face, 1.0, [0.0, 2.0], [14.0, 14.0], [[0, [8, 184, 0]], [1, [30, 200, 0]]]))
    L += both([lay(ell(4.5, 4.5, 1.55, 2.6), [30, 110, 16]),                                              # socket
               lay(ell(4.5, 4.5, 1.1, 2.2), [26, 22, 22]),                                                # eye
               lay(ell(4.2, 4.9, 0.7, 0.9), [80, 40, 56]),                                                # purple round the glint
               lay(circ(4.55, 4.5, 0.5), [140, 141, 141])], 7.0)                                         # glint
    mouth = poly([[5.0, 8.0], [9.0, 8.0], [10.2, 9.0], [10.5, 10.5], [9.8, 12.0], [9.0, 13.0], [5.0, 13.0], [4.2, 12.0],
                  [3.5, 10.5], [3.8, 9.0]], smooth=2)
    inner = {"t": "grow", "of": mouth, "d": -0.75}
    L += both([lay(vline([[3.9, 10.3], [2.7, 9.9], [1.4, 9.3]], [1.0, 0.95, 0.75]), [40, 40, 30])], 7.0)  # smile lines
    L += [lay(mouth, [56, 64, 48]),
          lay(inner, [204, 207, 209]),
          lay(inter(inner, rect(4.6, 8.6, 9.4, 9.95, 0.3)), [249, 249, 231]),                            # upper teeth
          lay(inter(inner, rect(5.8, 10.95, 8.2, 12.3, 0.3)), [249, 249, 231]),                          # lower teeth
          soft(ell(7.0, 13.6, 2.5, 0.5), [8, 184, 0], 0.4, 0.9, face)]                                   # shade under the chin
    return item(L)


B = (furious_b, unhappy_b, neutral_b, happy_b, delighted_b)

ELEMENTS = {
    "gb-mood--a": {
        "how": "version A, faithful: each face is its silhouette in the face's commonest colour with every same-colour patch "
               "of the art (8-connected) laid on it as its own shape: the patch's pixel outline, corners cut twice (a "
               "staircase becomes a slope, a single pixel a round dot), in the art's colour. Nothing merged, nothing mirrored",
        "changed": "round 2 had idealised smooth faces with features too big; A keeps the art's every tone patch: the 1 px rim "
                   "tones, the lit ridge and cheeks, the 2 x 2 and 3 x 3 eye blocks with their gleams, the 1 px mouths, the "
                   "small hooked horns, the checker of the clenched teeth",
        "unsure": "the faces' painted look survives, the pixels' squareness does not; at 16 px on screen A and the art differ "
                  "only at patch corners",
        "z": 20, "states": [(s, C.GB, cell(k), (lambda k=k: faithful(k))) for k, s in enumerate(STATES)]},
    "gb-mood--b": {
        "how": "version B, clean: the same five faces drawn at the art's sizes and places with the light baked as layers: a "
               "1 px darker rim band, the face's radial tone, the lit ridge, cheeks and chin, sockets and eye blocks (2 x 3 "
               "to 3 x 5 px) with a gleam on the same side in both eyes, mouths as 1 px lines of varying darkness with a lip "
               "under them; furious: small hooked horns inside the head's height, V brows over a grey eye and inner pupil, "
               "a clenched-teeth mouth (two offset rows of tiles) with the shadow under it",
        "changed": "against round 2: features shrunk to the art's blocks (eyes 2-3 px wide, not 4-5); every face now has the "
                   "art's darker rim and two or three tones instead of one soft radial; horns are the art's 2 px hooks; the "
                   "frown, smile and grin are the art's size and 1 px weight; the invented slanted brows on the unhappy face "
                   "are now the art's short level red dashes; glints sit on the same side in both eyes",
        "unsure": "",
        "z": 20, "states": [(s, C.GB, cell(k), fn) for k, (s, fn) in enumerate(zip(STATES, B))]},
}
