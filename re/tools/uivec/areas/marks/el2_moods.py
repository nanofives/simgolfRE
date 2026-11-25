"""Fix round 2: GBUBBLES row 0x144 cells 0-4, the golfer mood faces (furious devil, unhappy, neutral, happy, delighted).

Read again from read/gb-mood-N.txt (crop pixel (i, j) covers [i, i+1) x [j, j+1)). Round 1 drew each face as a smooth
radial disc with flat features ("too simplified"). The art's faces are lit from the front: a bright ridge down the middle
(forehead and nose), cheek lights, eye sockets darker than the face around a dark eye, a light under-eye band, a shadow
band above the mouth, a lit chin, and a darker rim. Here each of those is its own layer, measured on the cell:
- furious: red head with two curved horns; thick brows slanting down to the middle; small grey eyes with dark pupils on
  the inner side; a wide rounded mouth (purple-brown rim) of clenched teeth: an upper and a lower row of 1 px teeth offset by one, as the
  art's grey checker shows; a shadow
  under the mouth and a lit chin. Features on x 7.0 (the art's horns, brows, eyes and mouth mirror about it).
- unhappy: orange; worried brows (dark red) over two dark square eyes with a light lower lid; a frown arc with a brown
  lower lip; cheek and chin tones. Features on x 8.0, as the art places them.
- neutral: yellow; bright ridge; eyes in olive sockets with dark sides; a bright band under the eyes, a darker band under
  it, a flat mouth; lit chin. Features on x 8.0.
- happy: green; bright nose ridge with olive shading each side; eyes in dark green sockets; a smile arc with an olive
  upper lip; lit chin. Features on x 7.5.
- delighted: bright green; tall eyes in dark sockets with a purple rim and a grey glint; an open grin (dark rim, grey
  inside, white upper and lower teeth) with smile lines out from both corners. Features on x 7.0.
"""
import common as C
from fix2 import U, circ, ell, inter, item, lay, mirror, mirror_layers, poly, rect, vline

STATES = ("furious", "unhappy", "neutral", "happy", "delighted")


def radial(c, r, stops):
    return {"radial": {"c": list(c), "r": r, "stops": stops}}


def both(layers, ax):
    """The layers and their mirror image about x = ax."""
    return layers + mirror_layers(layers, ax)


def soft(shape, col, s=0.5, a=1.0, clip=None):
    L = lay(shape, col, soft=s, alpha=a)
    if clip is not None:
        L["clip"] = clip
    return L


# ------------------------------------------------------------------------------------------------------------- furious
def furious():
    head = ell(7.5, 8.0, 7.4, 7.0)
    horn = poly([[0.8, 3.2], [0.85, 2.0], [1.4, 1.0], [2.4, 0.3], [3.7, -0.05], [3.55, 0.9], [3.5, 2.3]], smooth=1)
    horn_dark = inter(horn, {"t": "half", "p": [2.9, 0.0], "n": [-1.0, 0.0]})     # its inner side in shade
    L = both([lay(horn, [136, 16, 0]), lay(horn_dark, [96, 16, 12])], 7.0)
    L.append(lay(head, radial([7.0, 6.2], 8.6, [[0, [236, 0, 0]], [0.35, [214, 0, 0]], [0.62, [196, 4, 0]],
                                                [0.84, [170, 16, 0]], [1.0, [128, 16, 4]]])))
    L.append(soft(rect(6.1, 3.0, 7.9, 9.0, 0.9), [242, 0, 0], 0.6, 0.85, head))                       # forehead / nose ridge
    L += both([soft(ell(1.9, 6.8, 0.9, 0.9), [240, 0, 0], 0.5, 0.75, head)], 7.0)                       # cheek lights
    L += both([lay(vline([[1.8, 4.1], [3.6, 4.85], [5.5, 5.85]], [1.1, 1.5, 1.3]), [34, 20, 30])], 7.0)  # brows
    L += both([lay(ell(4.85, 6.5, 0.95, 0.55), [130, 131, 131]), lay(ell(5.55, 6.5, 0.5, 0.52), [18, 17, 33])], 7.0)
    mouth = {"t": "rrect", "c": [7.0, 10.0], "half": [4.9, 2.05], "r": 1.9}
    inner = {"t": "rrect", "c": [7.0, 10.0], "half": [4.0, 1.05], "r": 0.7}
    L += [soft(ell(7.0, 12.5, 3.2, 0.9), [120, 16, 6], 0.6, 0.9, head),                                 # shadow under the mouth
          soft(ell(7.0, 13.9, 1.2, 0.4), [230, 0, 0], 0.4, 0.55, head),                                 # lit chin
          lay(mouth, {"linear": {"a": [0, 8.5], "b": [0, 12.0], "stops": [[0, [84, 40, 58]], [0.7, [80, 40, 56]], [1, [80, 16, 16]]]}}),
          lay(inner, [204, 207, 209])]
    # clenched teeth: an upper and a lower row of 1 px teeth, the lower row offset by one (the art's grey checker)
    L.append(lay(vline([[2.8, 10.0], [11.2, 10.0]], 0.45), [130, 131, 131], clip=inner))
    for x in (4.5, 6.5, 8.5, 10.5):
        L.append(lay(rect(x - 0.3, 8.9, x + 0.3, 10.0), [130, 131, 131], clip=inner))
    for x in (3.5, 5.5, 7.5, 9.5):
        L.append(lay(rect(x - 0.3, 10.0, x + 0.3, 11.1), [130, 131, 131], clip=inner))
    return item(L)


# ------------------------------------------------------------------------------------------------------------- unhappy
def unhappy():
    face = circ(7.5, 7.5, 7.5)
    L = [lay(face, radial([8.0, 6.6], 8.6, [[0, [204, 76, 0]], [0.6, [200, 72, 0]], [0.8, [178, 44, 0]], [1.0, [136, 16, 0]]]))]
    L += both([soft(ell(2.6, 7.6, 0.9, 1.5), [144, 64, 32], 0.6, 0.7, face)], 8.0)                       # cheek tone
    L += [soft(ell(9.0, 13.4, 2.4, 0.6), [144, 64, 32], 0.5, 0.6, face)]                                 # chin tone
    L += both([soft(rect(4.0, 6.0, 7.0, 7.0, 0.5), [176, 88, 0], 0.35, 0.95, face),                      # lower lid light
               lay(rect(4.0, 3.0, 7.0, 6.0, 0.8), [44, 26, 22]),                                          # eye (soft-edged dark)
               lay(rect(4.35, 3.35, 6.65, 5.65, 0.6), [12, 12, 16]),
               lay(vline([[4.9, 2.6], [6.9, 2.25]], 1.0), [176, 16, 0])], 8.0)                            # worried brow
    L.append(lay(vline([[3.3, 11.5], [4.2, 10.0], [5.6, 9.0], [8.0, 8.55], [10.4, 9.0], [11.8, 10.0], [12.7, 11.5]],
                       [0.8, 1.0, 1.0, 1.05, 1.0, 1.0, 0.8]), [58, 40, 18]))                             # frown
    L.append(lay(vline([[6.3, 10.5], [9.7, 10.5]], 0.95), [120, 80, 8]))                                 # lower lip
    return item(L)


# ------------------------------------------------------------------------------------------------------------- neutral
def neutral():
    face = circ(7.5, 7.5, 7.5)
    L = [lay(face, radial([8.0, 5.6], 9.2, [[0, [226, 253, 0]], [0.35, [212, 237, 0]], [0.6, [208, 220, 2]],
                                            [0.82, [186, 196, 0]], [1.0, [150, 168, 0]]]))]
    L.append(soft(rect(7.0, 1.4, 9.0, 8.4, 0.9), [226, 253, 0], 0.55, 0.95, face))                       # ridge
    L.append(soft(rect(0.0, 9.0, 15.0, 10.0), [188, 198, 0], 0.35, 0.9, face))                           # shadow band
    L.append(lay(vline([[2.6, 8.5], [13.4, 8.5]], 1.0), [226, 253, 0], clip=face))                       # band under the eyes
    L.append(lay(vline([[5.4, 8.5], [10.6, 8.5]], 1.0), [241, 253, 77]))
    L += both([lay(ell(5.0, 5.5, 1.95, 2.45), [120, 175, 0]),                                             # socket
               lay(ell(5.0, 5.45, 1.5, 1.6), [86, 70, 30]),                                               # its dark sides
               lay(ell(5.0, 5.4, 0.98, 1.5), [18, 17, 28])], 8.0)                                         # eye
    L.append(lay(vline([[2.2, 10.55], [13.8, 10.55]], 1.0), [120, 175, 0]))                              # mouth ends in shade
    L.append(lay(vline([[3.1, 10.5], [12.9, 10.5]], 1.0), [88, 76, 18]))                                 # mouth
    L.append(soft(ell(8.0, 12.5, 2.1, 0.6), [226, 253, 0], 0.45, 0.95, face))                            # lit chin
    return item(L)


# ------------------------------------------------------------------------------------------------------------- happy
def happy():
    face = circ(7.5, 7.5, 7.5)
    L = [lay(face, radial([7.5, 6.0], 9.0, [[0, [134, 232, 0]], [0.4, [132, 230, 0]], [0.62, [91, 216, 0]],
                                            [0.82, [48, 184, 0]], [1.0, [41, 150, 4]]]))]
    L += both([soft(vline([[6.45, 3.4], [6.45, 6.4]], 0.8), [121, 141, 1], 0.45, 0.6, face)], 7.5)       # olive beside the nose
    L.append(soft(vline([[7.5, 1.3], [7.5, 9.6]], 1.0), [136, 234, 0], 0.3, 1.0, face))                  # nose ridge
    L += both([soft(ell(4.8, 5.0, 2.05, 1.85), [53, 120, 16], 0.4, 0.7),                                  # socket rim
               lay(ell(4.8, 5.0, 1.75, 1.55), [27, 72, 16]),                                              # socket
               lay(ell(4.75, 4.85, 1.25, 0.95), [24, 26, 42]),                                            # eye
               lay(circ(5.45, 5.35, 0.42), [80, 40, 64])], 7.5)                                           # its purple glint
    L.append(soft(vline([[5.5, 10.45], [9.5, 10.45]], 0.9), [121, 141, 1], 0.3, 0.75))                   # upper lip shade
    L.append(lay(vline([[2.3, 8.3], [3.4, 9.5], [4.6, 10.6], [6.2, 11.4], [7.5, 11.55], [8.8, 11.4], [10.4, 10.6],
                        [11.6, 9.5], [12.7, 8.3]], [0.7, 0.95, 1.0, 1.05, 1.05, 1.05, 1.0, 0.95, 0.7]), [32, 30, 38]))  # smile
    L.append(soft(ell(7.0, 13.4, 1.8, 0.55), [134, 232, 0], 0.45, 0.9, face))                            # lit chin
    return item(L)


# ------------------------------------------------------------------------------------------------------------- delighted
def delighted():
    face = circ(7.3, 7.5, 7.4)
    L = [lay(face, radial([7.0, 5.6], 9.2, [[0, [8, 255, 0]], [0.55, [0, 252, 0]], [0.78, [44, 216, 0]], [1.0, [8, 180, 0]]]))]
    L += both([lay(ell(4.5, 4.2, 1.8, 2.75), [30, 100, 16]),                                              # socket
               lay(ell(4.5, 4.25, 1.2, 2.2), [28, 22, 24]),                                               # eye
               lay(ell(4.45, 4.55, 0.75, 1.1), [80, 40, 56]),                                             # purple rim of the glint
               lay(circ(4.5, 4.5, 0.48), [130, 131, 131])], 7.0)                                          # glint
    mouth = poly([[4.6, 8.0], [9.4, 8.0], [10.6, 9.0], [10.6, 10.2], [9.8, 11.6], [8.9, 12.95], [5.1, 12.95], [4.2, 11.6],
                  [3.4, 10.2], [3.4, 9.0]], smooth=1)
    inner = {"t": "grow", "of": mouth, "d": -0.95}
    L += both([lay(vline([[3.7, 10.35], [2.5, 9.95], [1.3, 9.45]], [1.0, 0.9, 0.7]), [40, 42, 32])], 7.0)  # smile lines
    L += [lay(mouth, [56, 64, 48]), lay(inner, [204, 207, 209]),
          lay(inter(inner, rect(4.9, 8.9, 9.1, 10.0, 0.2)), [249, 249, 231]),                             # upper teeth
          lay(inter(inner, rect(5.9, 11.0, 8.1, 12.2, 0.2)), [249, 249, 231])]                            # lower teeth
    return item(L)


ELEMENTS = {"gb-mood": {
    "how": "five mood faces, lit from the front, each shading part its own layer: furious = red head with curved horns (inner "
           "side shaded), forehead/nose ridge light, cheek lights, thick brows slanting to the middle, grey eyes with inner "
           "pupils, a rounded mouth of clenched teeth (upper and lower rows offset), shadow under the mouth, lit chin; unhappy = orange, "
           "worried brows, dark square eyes with a light lower lid, frown with a brown lower lip, cheek and chin tones; "
           "neutral = yellow, bright ridge, eyes in olive sockets with dark sides, bright under-eye band over a shadow band, "
           "flat mouth with shaded ends, lit chin; happy = green, nose ridge with olive either side, eyes in dark green "
           "sockets with a purple glint, smile arc under an olive upper lip, lit chin; delighted = bright green, tall eyes "
           "in dark sockets with a purple-rimmed grey glint, open grin (dark rim, grey inside, white upper and lower teeth) "
           "with smile lines out from the corners",
    "changed": "redone with the art's lighting and parts: round 1 had one smooth radial per face and flat features; now each "
               "face carries its measured ridge light, sockets, under-eye and shadow bands, cheek and chin lights, and the "
               "features as the art draws them (interlocking clenched teeth, curved shaded horns, worried brows, socketed eyes, glints)",
    "unsure": "pixel art at 15 px: the unhappy and neutral features sit half a pixel right of the face's axis (x 8.0), the "
              "furious and delighted ones half a pixel left (x 7.0); drawn where the art puts them",
    "z": 20,
    "states": [(s, C.GB, (16 * i, 0x144, 16, 16), fn) for i, (s, fn) in enumerate(zip(STATES, (furious, unhappy, neutral, happy,
                                                                                               delighted)))]}}
