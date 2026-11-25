"""Fix round 3: courseinfo.pcx fun medal (laughing face on the blue glass bubble), cell a = lit (31 px), b = plain (32 px).

The bubble is round 2's measured paint unchanged (navy rim lit from the left, radial body, b's drop shadow): the cash and
skill medals that share it were approved, and a bubble lit differently from its neighbours is a rejection cause.
Round 2's glyph beside the art at 20x (work3/marks/read/fun_a.png, fun_b.png), the five largest differences:
 1. the eyes: the art's are short dark upright dashes (1-2 x 3 px at x 13-14 rows 6-8 and x 19-20 rows 7-9), round 2's were
    long thin squints leaning the other way;
 2. the mouth is a filled opening in the art: a dark cavity (darkest band rows 15-17), white upper teeth under the upper lip
    (rows 12-14, x 10-19), a lit tongue and lower teeth (rows 18-21) and a dark right wall (x 20-22); round 2 drew only
    outline strokes over the bubble colour, so it read as a line drawing;
 3. cell a's white is the teeth and the tongue's light, not a halo round every stroke (round 2 haloed the eyes and lips);
 4. weight: the art's lines change weight and tone (lip 1-2 px mid navy, cavity 3 px near black, lower lip thin and broken),
    round 2 used one even 1.25 px ink everywhere;
 5. the mouth's shape: its upper edge slants from the left corner (7, 9) to the right corner (24, 12) and its right side runs
    straight down x 22, the bottom is round at row 23 left of the middle; round 2's was a symmetric U.
Version A: the bubble model, then every patch where the art departs from it (darker ink, shade, light, white) as its own
shape in that patch's colour. Version B: a clean laughing mouth and eyes with the tones as layers.
"""
import numpy as np

import common as C
import el2_medals as M
import fit
import fix3
from fix2 import U, circ, ell, inter, lay, poly, vline

CELLS = M.CELLS["fun"]


def bubble(state):
    """Round 2's bubble layers (approved on the cash and skill medals) and its body circle."""
    q = M.measure("fun", state)
    (cx, cy), r, rw = q["c"], q["r"], q["rw"]
    outer = circ(cx, cy, r)
    body = circ(cx, cy, round(r - rw, 2))
    rim_paint = {"linear": {"a": [cx, cy], "b": [round(cx + M.RIM_DIR[0] * r, 2), round(cy + M.RIM_DIR[1] * r, 2)],
                            "stops": [[0, q["rim"][0]], [1, q["rim"][1]]]}}
    L = []
    if state == "b":
        L.append(lay(circ(cx + 1, cy + 1, r), [14, 14, 20]))
    L.append(lay(outer, rim_paint))
    L.append(lay(body, {"radial": {"c": q["light"], "r": q["R"], "stops": q["stops"]}}, soft=0.55))
    return q, body, L


MOUTH = poly([[7.4, 8.9], [9.6, 10.3], [12.4, 11.3], [15.6, 12.1], [19.4, 12.4], [22.6, 11.9], [24.4, 11.6],
         [23.4, 13.2], [22.9, 15.6], [22.4, 18.4], [21.4, 21.0], [19.6, 22.7], [17.0, 23.5], [14.6, 23.0],
         [12.6, 21.4], [11.1, 19.0], [10.2, 16.4], [9.6, 13.4], [8.6, 10.6]], smooth=2)
LOWER_LIP = vline([[11.6, 23.6], [13.2, 24.9], [15.4, 25.6], [17.6, 25.5], [19.6, 24.8], [21.3, 23.4]],
                  [0.9, 1.1, 1.15, 1.15, 1.1, 0.9])


# ----------------------------------------------------------------------------------------------------- version A
TONES = (("shade", -35, -18), ("light", 18, 40), ("ink", -999, -35), ("white", 40, 999))


# The glyph's parts, read by hand off cell a's luminance grid (digit = lum / 25.6, read/fun_a.png): (name, x0, x1, y0, y1,
# lo, hi): the part is the pixels of that box whose digit is in [lo, hi]. The two cells put the glyph on the same pixels, so
# both use cell a's pixel sets, each painted in its own cell's colour (the mean of those pixels).
PARTS = [("tongue shade", 10, 19, 17, 21, 4, 6), ("tongue", 11, 20, 17, 21, 7, 7), ("lower teeth", 11, 20, 17, 21, 8, 9),
         ("upper teeth shade", 12, 22, 13, 15, 5, 7), ("upper teeth", 9, 22, 12, 14, 8, 9),
         ("lower lip light", 13, 20, 22, 24, 6, 7),
         ("throat and right wall", 9, 21, 13, 21, 0, 2), ("mouth bottom", 11, 21, 18, 23, 2, 3),
         ("upper lip", 7, 17, 9, 12, 0, 4), ("lip right corner and wall", 22, 24, 11, 17, 0, 3),
         ("lower lip", 11, 22, 23, 26, 0, 3), ("left eye", 12, 15, 5, 9, 0, 3), ("right eye", 18, 21, 6, 10, 0, 3)]
GLOW = ("glow (cell a)", 4, 13, 6, 12, 8, 9)


def part_sets():
    x, y, w, h = CELLS[0]
    art, m = C.crop(C.CI, x, y, w, h)
    D = np.minimum(9, (fit.lum(art) / 25.6).astype(int))
    taken = np.zeros(D.shape, bool)
    out = []
    for name, x0, x1, y0, y1, lo, hi in PARTS + [GLOW]:
        s = np.zeros(D.shape, bool)
        s[y0:y1 + 1, x0:x1 + 1] = True
        s &= m & (D >= lo) & (D <= hi) & ~taken
        if name != GLOW[0]:
            taken |= s
        out.append((name, s))
    return out


def faithful(state):
    """Version A: the bubble, then each hand-read part of the glyph as its pixel outline (corners cut once) in the colour
    those pixels have in this cell."""
    q, body, L = bubble(state)
    x, y, w, h = CELLS[0 if state == "a" else 1]
    art, m = C.crop(C.CI, x, y, w, h)
    L = list(L)
    # the in-between tones of the glyph: every other pixel of the mouth and lips (the footprint of version B's mouth,
    # grown 0.7 px) that departs from the bubble by 10+ levels, cut into the cell's own tone steps, each patch its own shape
    model = C.render_1x([{"box": [-1, -1, 34, 34], "layers": L}], w, h, np.zeros((h, w, 3)), ss=4)
    d = fit.lum(art) - fit.lum(model)
    jj, ii = np.mgrid[0:h, 0:w]
    import sdf
    F = sdf.sd({"t": "grow", "of": U(MOUTH, LOWER_LIP), "d": 0.7}, ii + 0.5, jj + 0.5) < 0
    sets = part_sets()
    used = np.zeros(m.shape, bool)
    for name, s in sets:
        used[:s.shape[0], :s.shape[1]] |= s[:m.shape[0], :m.shape[1]]
    D = np.minimum(9, (fit.lum(art) / 25.6).astype(int))
    mid = F & m & ~used & (np.abs(d) >= 10)
    for t in np.unique(D[mid]):
        for p in fix3.components(mid & (D == t), 8):
            pm = fix3.part_mask(p, m.shape)
            L.append(lay(fix3.patch(pm, smooth=1, grow=0.1), [int(round(v)) for v in art[pm].mean(0)], clip=body))
    for name, s in sets:
        if name == GLOW[0]:
            if state != "a":
                continue
        t = np.zeros(m.shape, bool)
        t[:s.shape[0], :s.shape[1]] = s[:m.shape[0], :m.shape[1]]
        s = t & m
        if not s.any():
            continue
        for p in fix3.components(s, 8):           # each separate patch of the part its own shape, own colour
            pm = fix3.part_mask(p, s.shape)
            L.append(lay(fix3.patch(pm, smooth=1, grow=0.1), [int(round(v)) for v in art[pm].mean(0)], clip=body))
    return [{"box": [-1, -1, 34, 34], "layers": L}]


def faithful_auto(state):
    q, body, L = bubble(state)
    x, y, w, h = CELLS[0 if state == "a" else 1]
    art, m = C.crop(C.CI, x, y, w, h)
    model = C.render_1x([{"box": [-1, -1, 34, 34], "layers": L}], w, h, np.zeros((h, w, 3)), ss=4)
    d = fit.lum(art) - fit.lum(model)
    (cx, cy), r, rw = q["c"], q["r"], q["rw"]
    jj, ii = np.mgrid[0:h, 0:w]
    dist = np.hypot(ii + 0.5 - cx, jj + 0.5 - cy)
    inner = m & (dist < r - rw - 0.8)
    core = m & (dist < r - rw - 1.6)          # faint tones only away from the rim (there they are the rim's own blend)
    G = inner & ((np.abs(d) >= 35) | (core & (np.abs(d) >= 18)))     # where the art departs from the bubble: the glyph
    La = fit.lum(art)
    tone = np.where(d < 0, -1, 1) * (1 + np.floor(La / 22.0))       # the art's own tone steps, darker / lighter side apart
    parts = []
    for t in np.unique(tone[G]):
        for p in fix3.components(G & (tone == t), 8):
            pm = fix3.part_mask(p, G.shape)
            if len(p) < 2 and np.abs(d[pm]).max() < 35:
                continue                       # a lone pixel of a faint tone is the gradient's dither, not a patch
            parts.append((len(p), p, pm))
    parts.sort(key=lambda t: -t[0])
    items = [{"box": [-1, -1, 34, 34], "layers": L}]
    for n, p, pm in parts:
        col = [int(round(v)) for v in art[pm].mean(0)]
        py_, px_ = zip(*p)
        items.append({"box": [min(px_) - 1, min(py_) - 1, max(px_) + 2, max(py_) + 2],
                      "layers": [lay(fix3.patch(pm, smooth=1, grow=0.1), col, clip=body)]})
    return items


# ----------------------------------------------------------------------------------------------------- version B
def measure_tones(state):
    """Ink, cavity, teeth and tongue colours read off the art cell (medians over the pixels of each read part)."""
    x, y, w, h = CELLS[0 if state == "a" else 1]
    art, m = C.crop(C.CI, x, y, w, h)
    pick = lambda pts: [int(v) for v in np.median(np.array([art[j, i] for i, j in pts]), 0)]  # noqa: E731
    return {"ink": pick([(13, 7), (14, 7), (14, 6), (20, 8), (19, 8), (20, 9), (8, 9), (9, 10)]),
            "lip": pick([(11, 11), (12, 11), (14, 12), (15, 12), (23, 12), (24, 12)]),
            "cavity": pick([(i, 16) for i in range(11, 21)]),
            "wall": pick([(21, 17), (21, 18), (20, 19), (21, 19), (20, 20)]),
            "teeth": pick([(10, 12), (11, 12), (13, 13), (14, 13), (15, 14), (16, 14), (17, 14)]),
            "tongue": pick([(13, 19), (14, 19), (15, 20), (16, 20), (17, 20), (16, 21)]),
            "lower_teeth": pick([(12, 18), (13, 18), (17, 19), (18, 19)]),
            "bottom": pick([(13, 22), (16, 23), (17, 23), (18, 22), (12, 21)]),
            "chinlip": pick([(12, 24), (13, 24), (14, 25), (19, 25), (21, 24)])}


def clean(state):
    q, body, L = bubble(state)
    T = measure_tones(state)
    lit = state == "a"
    mouth = MOUTH
    cavity_band = inter(mouth, poly([[8, 14.6], [24, 15.2], [24, 18.2], [8, 17.6]]))
    teeth = inter(mouth, vline([[9.6, 11.7], [12.6, 12.7], [15.8, 13.5], [19.2, 13.9]], [1.6, 2.2, 2.3, 1.8]))
    tongue = inter(mouth, ell(15.4, 20.0, 4.6, 2.3, -8))
    wall = inter(mouth, vline([[22.6, 13.4], [22.2, 16.5], [21.4, 19.4], [20.2, 21.4]], [1.6, 2.0, 2.0, 1.6]))
    L += [lay(mouth, T["bottom"], clip=body),                                # the opening
          lay(cavity_band, T["cavity"], soft=0.4),                                                         # its dark throat
          lay(tongue, T["tongue"], soft=0.5),                                                              # the lit tongue
          lay(inter(tongue, ell(13.0, 18.6, 1.2, 0.8)), T["lower_teeth"], soft=0.4),                       # lower teeth
          lay(inter(tongue, ell(17.8, 19.6, 1.1, 0.9)), T["lower_teeth"], soft=0.4),
          lay(wall, T["wall"], soft=0.4),                                                                  # the right wall in shade
          lay(teeth, T["teeth"], soft=0.35),                                                               # upper teeth
          lay({"t": "outline", "of": mouth, "w": 0.9}, T["bottom"], clip=body),                            # the opening's edge
          lay(vline([[7.3, 9.0], [9.6, 10.35], [12.4, 11.35], [15.6, 12.15], [19.4, 12.45], [22.6, 12.0], [24.5, 11.7]],
                    [1.6, 1.4, 1.3, 1.25, 1.25, 1.3, 1.1]), T["lip"]),                                    # upper lip
          lay(LOWER_LIP, T["chinlip"])]                                     # lower lip
    for ex, ey in ((13.75, 7.45), (19.85, 8.45)):                                                         # eyes: short upright dashes
        L.append(lay(ell(ex, ey, 0.95, 1.65, 12), T["ink"]))
        if lit:
            L.append(lay(ell(ex - 1.25, ey - 0.2, 0.45, 1.1, 12), q["glow"], soft=0.4, clip=body))       # their lit left edge
    return [{"box": [-1, -1, 34, 34], "layers": L}]


ELEMENTS = {
    "course-fun-medal--a": {
        "how": "version A, faithful: round 2's measured bubble (unchanged, as on the approved cash and skill medals), then every "
               "patch where the art departs from that bubble (ink darker by 35+ levels, shade 18-35, light 18-40, white 40+) "
               "as its own shape: its pixel outline with the corners cut once, in that patch's own colour; the glyph is "
               "nothing but those patches (eyes, lip, teeth, cavity, tongue, walls, lower lip as the art paints them)",
        "changed": "round 2 drew the face as even outline strokes with a halo; A takes the art's tone patches one by one: the "
                   "upright eye dashes, the slanting lip, the white teeth, the black throat, the lit tongue and lower teeth, "
                   "the dark right wall, the broken lower lip",
        "unsure": "lone faint pixels (one-pixel shade or light inside the gradient) are left to the bubble paint",
        "z": 10, "states": [("a", C.CI, CELLS[0], lambda: faithful("a")), ("b", C.CI, CELLS[1], lambda: faithful("b"))]},
    "course-fun-medal--b": {
        "how": "version B, clean: round 2's measured bubble unchanged; a laughing face: two short upright eye dashes (lit on "
               "their left in cell a), a wide open mouth as one filled shape (upper edge slanting from the left corner to the "
               "right corner, right side straight down, round bottom left of the middle) with its tones as layers: a dark "
               "throat band, white upper teeth under the lip, a lit tongue with two lower teeth, the right wall in shade, a "
               "dark edge, the upper lip 1.6 to 1.1 px, and the lower lip as a thin curve below; colours read off each cell",
        "changed": "against round 2: the eyes are the art's short upright dashes (not long squints); the mouth is a filled "
                   "opening with teeth, throat and tongue instead of outline strokes; no halo round the strokes (cell a's white "
                   "is the teeth); line weights vary as the art's; the mouth is the art's asymmetric shape",
        "unsure": "",
        "z": 10, "states": [("a", C.CI, CELLS[0], lambda: clean("a")), ("b", C.CI, CELLS[1], lambda: clean("b"))]},
}
