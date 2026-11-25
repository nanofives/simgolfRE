"""P41 hud_panels fix round (2026-11-24): the tan bubble glyphs Mariano sent back (redo: mood, photo, hire, fire, resume,
thumbs-up, trophy; fix "a little bit sharper": club, crossed).

Every glyph was re-read from a luminance dump of its reference cell (common.dump; sheet x printed modulo 100) and checked
with figtool.show (coverage side by side). Round 1's smiley sat 1.6 px right of the art's: these are read again from
scratch. Thin dark strokes take the darkest third of the art under them ("dark"); coloured parts are sampled inside.
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import kit as K  # noqa: E402
import el_golfer  # noqa: E402
import el_employee  # noqa: E402
import figs  # noqa: E402
from kit import U, rect, circle, ellipse, poly, sub, inter  # noqa: E402,F401

limb = figs.limb


def mirror(pts, axis):
    return [(2 * axis - p[0], p[1]) + tuple(p[2:]) for p in pts]


# ------------------------------------------------------------------------------------------------------------ mood
def glyph_smiley():
    """A smiling face (G 500,50 idle), symmetric about x 519.05 (the eyes' and the smile's common axis).
    Eyes: upright ovals 3.3 px wide, rows 62.5-69.8, round (514.95, 66.1) and (523.15, 66.1).
    Smile: one line 1.6 px thick from (509.3, 72.6) down to its lowest point (519.05, 79.8) and up to (528.8, 72.6); at
    each end a short tick across the line (the dimple: rows 71-73, x 508-509 and 528-529)."""
    ax = 519.05
    left = [(509.4, 72.8, 1.6), (510.3, 74.6, 1.6), (511.1, 76.0, 1.65), (512.3, 77.3, 1.65), (513.7, 78.35, 1.7),
            (515.6, 79.25, 1.7), (517.4, 79.7, 1.75), (ax, 79.85, 1.75)]
    smile = left + mirror(left[::-1][1:], ax)
    tick = [(508.25, 72.9, 1.3), (509.1, 72.0, 1.45), (509.85, 71.2, 1.2)]
    return [
        ("left eye", ellipse(ax - 4.1, 66.1, 1.62, 3.75), "dark"),
        ("right eye", ellipse(ax + 4.1, 66.1, 1.62, 3.75), ("as", "left eye")),
        ("smile", limb(smile, smooth=1), ("as", "left eye")),
        ("dimple left", limb(tick), ("as", "left eye")),
        ("dimple right", limb(mirror(tick, ax)), ("as", "left eye")),
    ]


el_golfer.REF["golfer-tab-mood"] = (glyph_smiley, "golfer-tab-mood", "idle")


# ------------------------------------------------------------------------------------------------------------ photo
def glyph_camera():
    """A camera (G 500,150 idle), re-read: round 1's parts were right but sat 1.5 px right of the art.
    Body: a dark block x 508-529, rows 164-178, corners rounded r 1; lit grey on its top-left (rows 164-165, x 508-513, the
    top face catching the light) and a softer grey patch low on the left (rows 171-174, x 510-514).
    Two light lines one pixel in from each end, x 509.5 and 527.5, rows 167-176.
    Prism on top: a trapezoid, top edge y 159 from x 516 to 521, foot y 164 from x 513.4 to 523.6, lit grey at its upper
    left going dark to the lower right.
    Shutter button: x 525.6-528, rows 162.8-164.5.
    Lens: a light ring round (518.0, 170.0), outer r 4.5, round a dark core r 2.45."""
    lens_c = (518.0, 170.0)
    return [
        ("body", rect(508.0, 164.0, 529.0, 178.0, 1.0), ("sample_in", rect(513, 176, 527, 178))),
        ("top light", inter(rect(508.0, 164.0, 529.0, 178.0, 1.0), poly((507.5, 163.5), (513.6, 163.5), (512.8, 166.0),
                                                                       (507.5, 167.0))),
         ("sample_in", rect(508.5, 164.2, 512, 165.8))),
        ("low light", ellipse(511.8, 172.4, 1.9, 1.6), ("sample_in", rect(511, 171, 513, 173))),
        ("prism", poly((513.4, 164.2), (516.0, 159.0), (521.0, 159.0), (523.6, 164.2)),
         {"linear": {"a": [515.5, 159.5], "b": [522.0, 164.0], "stops": []}}),
        ("button", rect(525.6, 162.8, 528.0, 164.6, 0.45), ("sample_in", rect(526, 163, 528, 164))),
        ("left light", limb([(509.5, 166.6, 0.95), (509.5, 175.6, 0.95)], cap="butt"), ("sample_in", rect(509, 168, 510, 175))),
        ("right light", limb([(527.5, 166.6, 0.95), (527.5, 175.6, 0.95)], cap="butt"), ("sample_in", rect(527, 168, 528, 175))),
        ("lens ring", circle(*lens_c, 4.5), ("sample_in", rect(514, 169, 515, 171))),
        ("lens", circle(*lens_c, 2.45), ("sample_in", rect(517, 169, 519, 171))),
    ]


def _prism_paint(key, dx, dy):
    import kit
    a = (515.5 + dx, 159.5 + dy)
    b = (522.0 + dx, 164.0 + dy)
    c0 = kit.sample(key, rect(516 + dx, 159 + dy, 518 + dx, 161 + dy))
    c1 = kit.sample(key, rect(518 + dx, 161 + dy, 520 + dx, 162 + dy))
    c2 = kit.sample(key, rect(520 + dx, 162 + dy, 523 + dx, 164 + dy))
    return {"linear": {"a": list(a), "b": list(b), "stops": [[0.0, c0], [0.5, c1], [1.0, c2]]}}


def glyph_camera_layers():
    out = []
    for n, s, p in glyph_camera():
        out.append((n, s, "SHADE" if n == "prism" else p))
    return out


el_golfer.REF["golfer-tab-photo"] = (glyph_camera_layers, "golfer-tab-photo", "idle")


# ------------------------------------------------------------------------------------------------------------ builders
import common as C  # noqa: E402
import bubble as B  # noqa: E402
import el_bubbles as EB  # noqa: E402

def _cap_paint(key, dx, dy):
    """The hire man's cap: lit lavender-grey at its front (left, (415-416, 7)) going to the navy of its back (418-419, 7)."""
    c0 = K.sample(key, rect(415 + dx, 7 + dy, 417 + dx, 8 + dy))
    c1 = K.sample(key, rect(418 + dx, 7 + dy, 420 + dx, 8 + dy))
    return {"linear": {"a": [415.2 + dx, 0], "b": [418.6 + dx, 0], "stops": [[0.0, c0], [1.0, c1]]}}


SHADE_FN = {"golfer-tab-photo": _prism_paint, "employee-hire": _cap_paint}


def glyph_layers(parts, key, dx, dy, shade=None):
    """el_bubbles.glyph_layers plus ("paint", f): a part whose paint f(key, dx, dy) measures on this state's art."""
    fixed = []
    for n, sh, how in parts:
        if isinstance(how, tuple) and how[0] == "paint":
            fixed.append((n, sh, how[1](key, dx, dy)))
        else:
            fixed.append((n, sh, how))
    return EB.glyph_layers(fixed, key, dx, dy, shade=shade)


def build_big(el):
    """el_golfer.build with a per-element SHADE paint (a part whose paint is measured as a gradient)."""
    gl, ref_el, ref_st = el_golfer.REF[el]
    rg = EB.GEOM[ref_el][ref_st]
    scenes = {}
    for st, key, rct in C.CELLS[el]:
        g = EB.GEOM[el][st]
        dx, dy = (0, 0) if (el, st) == (ref_el, ref_st) else K.match_offset((rg["key"], rg["body"]), (key, g["body"]))
        if key == "G":
            ground, gshape = el_golfer.ground_tab(rct, key)
        else:
            ground, gshape = el_golfer.ground_big(rct, key)
        lay = glyph_layers(gl(), key, dx, dy, shade=SHADE_FN.get(el))
        EB.GU[(el, st)] = U(*[L["shape"] for L in lay])
        fam = g["family"]
        it = B.item(g["c"], g["body"], EB.PAINT[fam], glyph=lay, shadow=EB.SHADOWS.get(fam), ground=gshape)
        scenes[st] = [K.item(ground, box=[rct[0], rct[1], rct[0] + rct[2], rct[1] + rct[3]]), it]
    return scenes


for _k in ("golfer-tab-mood", "golfer-tab-photo"):
    EB.BUILD[_k] = build_big


# ------------------------------------------------------------------------------------------------------------ employee
def glyph_hire():
    """A running man in a cap holding a green arrow that points right (E 400,0 idle), re-read from the hex dump.
    Arrow head: a triangle, top vertex (426.6, 4.8), tip (431.6, 14.5), bottom vertex (426.6, 23.6), its left side bowing
    out to x 424.9 (the dark column x 425, rows 7-21); dark-green outline one pixel wide, green 18d621 inside, a light green
    streak down the inside of the left side (x 426, rows 7-13).
    Shaft: a thick stub x 421.5-425, rows 11.5-17.5 (dark-green top and bottom edges, a pale green line under the top edge
    at row 12, green below); the man's hand grips it (navy x 422-423, rows 13-15).
    Man: head x 415.5-418.5, rows 8.5-11.8; a lavender-grey cap over the front of it (rows 7-8, x 414-417) with its peak
    out to the left (414, 8.5) and a dark back edge (418-419, 7-8); a thin neck (row 12); torso x 415-418.5 from the
    sloping shoulders (row 14-15) to the hips (row 20.5), leaning back; back arm a straight diagonal from the shoulder
    (414.5, 14.6) to its elbow (412, 17.2), the forearm coming back along row 18 to the waist (415.2, 18.6); front arm along row 16 from x 419 to the hand
    on the shaft; back leg from the hip down-left (415.5, 21) -> (413.4, 24.4) -> the foot (411.4, 26.9); front leg (418.6,
    21) -> (419.3, 26) and its foot along row 26.5 to x 422.4."""
    tri = poly((426.5, 4.6), (431.8, 14.5), (426.5, 23.9), (424.85, 21.4), (424.75, 7.4))
    tri_in = poly((426.6, 6.5), (430.4, 14.5), (426.6, 22.0), (425.8, 20.7), (425.75, 8.1))
    return [
        ("arrow outline", tri, ("sample_in", rect(425, 18, 426, 21))),
        ("arrow", tri_in, ("sample_in", rect(427, 14, 429, 17))),
        ("arrow streak", limb([(426.5, 7.2, 1.0), (426.5, 13.2, 1.0)], cap="butt"), ("sample_in", rect(426, 8, 427, 12))),
        ("shaft outline", rect(421.4, 11.0, 425.6, 18.0, 0.4), ("sample_in", rect(423, 17, 425, 18))),
        ("shaft", rect(422.0, 12.0, 425.6, 17.0), ("sample_in", rect(424, 15, 425, 17))),
        ("shaft light", rect(422.0, 12.0, 425.6, 13.0), ("sample_in", rect(422, 12, 425, 13))),
        ("head", ellipse(417.3, 10.1, 1.6, 1.75), "dark"),
        ("cap", inter(ellipse(417.1, 8.25, 2.55, 1.45, -18), {"t": "half", "p": [0, 8.9], "n": [0, 1]}), "SHADE"),
        ("cap peak", limb([(415.4, 8.7, 1.15), (413.9, 8.95, 0.95)]), ("sample_in", rect(414, 8, 415, 9))),
        ("neck", limb([(417.3, 11.6, 0.8), (417.2, 13.0, 1.1)]), ("as", "head")),
        ("torso", poly((414.9, 15.0), (416.2, 13.9), (417.8, 13.5), (419.0, 14.6), (419.2, 16.6), (418.9, 19.4),
                       (419.0, 21.2), (415.2, 21.2), (415.3, 18.8), (414.9, 16.6), smooth=1), ("as", "head")),
        ("back arm", limb([(415.0, 15.0, 1.7), (413.4, 16.0, 1.6), (412.1, 17.1, 1.5), (413.2, 18.3, 1.35), (415.2, 18.6, 1.3)]), ("as", "head")),
        ("front arm", limb([(418.6, 15.6, 1.7), (420.8, 16.2, 1.5), (422.2, 15.4, 1.6)]), ("as", "head")),
        ("hand", K.rect(421.6, 12.9, 423.9, 16.2, 0.8), ("as", "head")),
        ("back leg", limb([(415.6, 20.8, 2.0), (415.3, 22.8, 1.85), (413.7, 24.4, 1.75), (412.4, 25.6, 1.75),
                           (411.6, 27.2, 1.6)]), ("as", "head")),
        ("front leg", limb([(418.5, 20.8, 2.0), (419.2, 23.2, 1.8), (419.9, 25.6, 1.7)]), ("as", "head")),
        ("front foot", limb([(419.8, 26.6, 1.9), (421.6, 26.9, 1.7), (422.6, 27.0, 1.35)]), ("as", "head")),
    ]


el_employee.GLYPHS["employee-hire"] = glyph_hire


def build_mid(el):
    """el_employee.build_mid with SHADE_FN paints."""
    scenes = {}
    for st, key, rct in C.CELLS[el]:
        g = EB.GEOM[el][st]
        dx, dy = EB.offset(el, st)
        ground, gshape = el_employee.ground_mid(el, rct, g["c"], key)
        lay = glyph_layers(el_employee.GLYPHS[el](), key, dx, dy, shade=SHADE_FN.get(el))
        EB.GU[(el, st)] = U(*[L["shape"] for L in lay])
        fam = g["family"]
        it = B.item(g["c"], g["body"], EB.PAINT[fam], glyph=lay, shadow=EB.SHADOWS.get(fam), ground=gshape)
        scenes[st] = [K.item(ground, box=[rct[0], rct[1], rct[0] + rct[2], rct[1] + rct[3]]), it]
    return scenes


for _k in ("employee-hire", "employee-fire", "employee-resume"):
    EB.BUILD[_k] = build_mid


def glyph_fire():
    """A red thumbs-down with a green cuff (E 400,50 idle), read from the hex dump: three reds (ce1818 lit, 8c1010 mid,
    631018 dark), four greens on the cuff.
    Cuff: x 416.9-418.6, rows 57.7-65.8, lit bright green at rows 59-60, darker down to the bottom, its right column darker.
    Fist: x 418.4-430.9, rows 57.7-68.3, with a knuckle bump on top (x 423-427, row 57); mid red, lit on the back of the
    hand (x 419.6-424, rows 58-61 and 63-70) with a crease across it (rows 61-62); a divider down x 424 (rows 58-71) between
    the back of the hand and the four fingers on the right, each finger a lit band (rows 59, 61, 64, 66) and dark
    separators at their left ends and at the right edge (rows 60, 62-63, 65, 67).
    Thumb: hangs from the lower left of the fist (x 419.6-425.4 at row 68) to its point (424.4, 77.6): a lit core (x
    421-423 at rows 68-71, x 423-424 at rows 72-75), a dark left edge, mid red on its right edge."""
    fist = poly((418.4, 58.7), (419.2, 57.8), (422.9, 57.7), (423.2, 56.85), (427.0, 56.85), (427.3, 57.7), (428.5, 57.8),
                (429.2, 58.8), (430.4, 59.6), (430.95, 60.6), (430.95, 67.4), (430.4, 68.15), (425.8, 68.3), (419.7, 68.9),
                (418.7, 67.6), smooth=1)
    thumb = poly((419.6, 67.9), (425.5, 67.9), (425.7, 73.0), (425.9, 75.6), (425.3, 76.9), (424.4, 77.7), (423.5, 76.9),
                 (422.3, 73.3), (420.9, 71.2), (419.8, 69.4), smooth=1)
    P = [
        ("fist", U(fist, thumb), ("sample_in", rect(424, 61, 425, 62))),
        ("back of hand", rect(419.6, 57.9, 423.95, 60.95, 0.5), ("sample_in", rect(420, 59, 423, 60))),
        ("back of hand low", rect(420.3, 62.9, 423.95, 69.9, 0.5), ("as", "back of hand")),
        ("thumb core", limb([(422.4, 69.4, 2.0), (423.4, 72.4, 1.6), (424.3, 75.8, 1.1)]), ("as", "back of hand")),
        ("knuckle light", rect(424.95, 56.95, 425.95, 58.0), ("as", "back of hand")),
    ]
    for k, (y0, x0, x1) in enumerate(((58.6, 424.9, 428.0), (60.95, 425.95, 430.0), (63.95, 425.95, 430.0),
                                      (65.95, 425.95, 429.95))):
        P.append((f"finger {k + 1}", rect(x0, y0, x1, y0 + (1.35 if k == 0 else 1.0), 0.4), ("as", "back of hand")))
    for k, (x0, y0, x1, y1) in enumerate(((423.95, 59.95, 426.95, 60.95), (424.95, 61.95, 425.95, 63.95),
                                          (428.95, 61.95, 430.95, 63.95), (424.95, 64.95, 427.95, 65.95),
                                          (429.95, 64.95, 430.95, 65.95), (424.95, 66.95, 425.95, 67.95))):
        P.append((f"separator {k + 1}", rect(x0, y0, x1, y1, 0.3), ("sample_in", rect(425, 60, 426, 61)) if k == 0
                  else ("as", "separator 1")))
    P += [
        ("thumb edge", limb([(420.5, 69.0, 0.95), (421.4, 70.9, 1.0), (422.4, 72.4, 1.0), (423.3, 74.6, 0.95),
                             (423.7, 76.2, 0.85)]), ("as", "separator 1")),
        ("cuff", rect(416.85, 57.7, 419.1, 65.85, 0.4), "SHADE"),
    ]
    return P


def _fire_cuff(key, dx, dy):
    stops = []
    for yy in (58, 59.5, 61, 62, 63.5, 65):
        stops.append([(yy + 0.5 - 57.7) / (65.85 - 57.7), K.sample(key, rect(417 + dx, int(yy) + dy, 418 + dx, int(yy) + 1 + dy))])
    return {"linear": {"a": [0, 57.7 + dy], "b": [0, 65.85 + dy], "stops": stops}}


el_employee.GLYPHS["employee-fire"] = glyph_fire
SHADE_FN["employee-fire"] = _fire_cuff


def glyph_thumbs_up():
    """A green thumbs-up with a red cuff (E 0,500 idle), read from the hex dump (greens 42ef18 lit, 31bd10 light, 298c08
    mid, 185a08 dark; the cuff's reds as on the thumbs-down).
    Thumb: standing up from the fist, tip (15.6, 504.7); its left edge dark (x 14, rows 505.5-510, then x 15 to the fist),
    its right edge curving out at the base (x 16 at rows 505-509, x 17 at 510-511, x 19 at 512); a lit streak down the
    inside of the right edge.
    Fist: x 9.3-21.6, rows 513-524.3, the lower right corner cut round (rows 522-524 narrow to x 11-18); mid green, light
    patches on the back of the hand (x 16-20, rows 513-517 and 519-522), four fingers on the left (x 10-14): lit bands at
    rows 516, 518 and 523, light at rows 514 and 520-521, a dark separator at row 517, the thumb's crease down x 15
    (rows 513-517); a dark edge along the left and bottom.
    Cuff: red, x 21.8-23.4, rows 513.7-521.9, lit in the middle (rows 515-518), dark at both ends, a dark line on its left."""
    thumb = poly((14.0, 506.2), (14.5, 505.0), (15.6, 504.6), (16.6, 505.5), (16.75, 509.0), (17.7, 510.2), (18.9, 511.2),
                 (19.9, 512.4), (19.9, 513.4), (14.6, 513.4), (14.0, 510.6), smooth=1)
    fist = poly((9.4, 515.8), (10.1, 513.4), (12.0, 512.9), (19.6, 512.9), (21.6, 514.0), (21.7, 521.6), (20.4, 522.4),
                (18.4, 523.6), (14.8, 524.4), (11.2, 524.3), (10.2, 522.6), (9.3, 520.6), smooth=1)
    P = [
        ("hand", U(fist, thumb), ("sample_in", rect(10, 515, 14, 516))),
        ("thumb edge", limb([(14.5, 505.6, 1.0), (14.45, 509.6, 1.0), (15.4, 512.0, 1.0), (15.45, 516.4, 1.0)]),
         ("sample_in", rect(15, 514, 16, 517))),
        ("thumb streak", limb([(15.5, 506.0, 1.0), (15.6, 509.2, 1.0), (16.5, 510.4, 1.0), (17.4, 511.3, 0.95)]),
         ("sample_in", rect(15, 506, 16, 509))),
        ("back light", rect(15.95, 512.95, 20.7, 516.9, 0.7), ("sample_in", rect(17, 513, 20, 516))),
        ("back light low", rect(15.95, 518.9, 20.1, 521.9, 0.7), ("as", "back light")),
        ("finger 1", rect(10.6, 513.8, 13.95, 514.95, 0.4), ("as", "back light")),
        ("finger 1 lit", rect(10.6, 515.6, 13.95, 516.95, 0.4), ("as", "thumb streak")),
        ("finger 2 lit", rect(9.9, 517.95, 13.95, 518.95, 0.4), ("as", "thumb streak")),
        ("finger 3", rect(10.6, 519.9, 13.95, 521.95, 0.4), ("as", "back light")),
        ("finger 4", rect(11.9, 522.6, 14.95, 523.9, 0.5), ("as", "back light")),
        ("finger 4 lit", rect(12.6, 522.7, 13.95, 523.8, 0.3), ("as", "thumb streak")),
        ("separator", rect(10.6, 516.95, 15.95, 517.95, 0.3), ("as", "thumb edge")),
        ("left edge", limb([(10.4, 513.6, 0.9), (9.75, 516.0, 0.9), (9.7, 519.6, 0.9), (10.4, 521.6, 0.9),
                            (11.5, 523.6, 0.9)]), ("as", "thumb edge")),
        ("right edge", limb([(21.4, 514.6, 0.9), (21.4, 521.6, 0.9)], cap="butt"), ("sample_in", rect(21, 519, 22, 521))),
        ("cuff", rect(21.75, 513.7, 23.4, 521.95, 0.4), "SHADE"),
    ]
    return P


def _up_cuff(key, dx, dy):
    stops = []
    for yy in (514, 515.5, 517, 518, 519, 520, 521):
        stops.append([(yy + 0.5 - 513.7) / (521.95 - 513.7), K.sample(key, rect(22 + dx, int(yy) + dy, 23 + dx, int(yy) + 1 + dy))])
    return {"linear": {"a": [0, 513.7 + dy], "b": [0, 521.95 + dy], "stops": stops}}


el_golfer.REF["employee-thumbs-up"] = (glyph_thumbs_up, "employee-thumbs-up", "idle")
SHADE_FN["employee-thumbs-up"] = _up_cuff
EB.BUILD["employee-thumbs-up"] = build_big


# ------------------------------------------------------------------------------------------------------------ rim + shadow
RIM_BIG = 1.5     # the big tan bubble's rim: one and a half pixels of navy round the body (profiles at 0, 45, 90 deg)
_SHADOW_UP = {}


def thumbs_shadow(st):
    """The cast shadow of the thumbs-up bubble for one state, fitted on the lavender square around a rim that is concentric
    with the body (round 1 fitted the rim off-centre and took the shadow's dark inner part for rim)."""
    if st not in _SHADOW_UP:
        import sdf
        _, key, rct = [c for c in C.CELLS["employee-thumbs-up"] if c[0] == st][0]
        g = EB.GEOM["employee-thumbs-up"][st]
        b = g["body"]
        c = [b[0], b[1], b[2] + RIM_BIG]
        sq = rect(rct[0], rct[1], rct[0] + rct[2], rct[1] + rct[3])
        ok = lambda x, y: sdf.sd(sq, x, y) < -0.6  # noqa: E731
        lav = K.sample(key, rect(rct[0] + 1, rct[1] + 1, rct[0] + 6, rct[1] + 6))
        _SHADOW_UP[st] = (c, B.fit_shadow([(key, c, ok)], lav))
    return _SHADOW_UP[st]


def build_thumbs(el):
    scenes = build_big(el)
    for st, key, rct in C.CELLS[el]:
        g = EB.GEOM[el][st]
        c, sh = thumbs_shadow(st)
        it = scenes[st][1]
        glyph = [L for L in it["layers"] if L["shape"].get("t") not in ("circle",) or L.get("name")]
        _, gshape = el_golfer.ground_big(rct, key)
        scenes[st][1] = B.item(c, g["body"], EB.PAINT[g["family"]], glyph=[L for L in it["layers"] if "name" in L],
                               shadow=sh, ground=gshape)
    return scenes


EB.BUILD["employee-thumbs-up"] = build_thumbs


def _resume_paint(name):
    def f(key, dx, dy):
        if name == "clip":
            c0 = K.sample(key, rect(426 + dx, 109 + dy, 428 + dx, 110 + dy))
            c1 = K.sample(key, rect(427 + dx, 110 + dy, 429 + dx, 111 + dy))
            return {"linear": {"a": [425.0 + dx, 109.2 + dy], "b": [428.6 + dx, 110.8 + dy], "stops": [[0.0, c0], [1.0, c1]]}}
        if name == "hat":
            cs = [K.sample(key, rect(xx + dx, 107 + dy, xx + 1 + dx, 108 + dy)) for xx in (415, 417, 419, 420)]
            return {"linear": {"a": [415.0 + dx, 0], "b": [421.0 + dx, 0],
                               "stops": [[0.08, cs[0]], [0.42, cs[1]], [0.75, cs[2]], [0.92, cs[3]]]}}
        cs = [K.sample(key, rect(xx + dx, 112 + dy, xx + 1 + dx, 113 + dy)) for xx in (416, 418, 419)]
        return {"linear": {"a": [416.0 + dx, 0], "b": [420.0 + dx, 0], "stops": [[0.12, cs[0]], [0.6, cs[1]], [0.88, cs[2]]]}}
    return f


def glyph_resume():
    """A purple figure in a hat standing on a line and holding up a clipboard (E 400,100 idle), read from the hex dump.
    Hat: crown x 417-419 (row 106), brim x 415-420.5 (row 107), lit lavender-grey on the left going to dark purple on the
    right. Head x 417-420.5, rows 108-111, dark purple. Shoulders lit along row 112 (x 416-419). Torso x 418-420 down to
    the hips (row 119), its right side against the board's black left edge. Left arm bent with the hand on the hip: down
    x 415 (rows 113-118) and back in along rows 117-118 to the torso, leaving the tan showing through at (417, 115-116);
    the forearm lit. Legs x 417-419 and x 420-422 rows 120-124, the left one lit at its foot (x 418, rows 125-126), the
    right one turning in (x 419-420, row 126); a ground line x 416-423 on row 127.
    Clipboard: white board x 421-433, rows 112-120, a dark olive edge (black down its left side, x 420); three dark text
    lines (rows 113, 116, 118; x 423-430 / 423-432 / 423-432) each with a light grey bullet at x 422; grey lines at rows
    114-115 (x 423-427) and 119 (x 424-430); a cyan clip on its top edge: a dome x 424-429.5, rows 108-111, dark olive
    outline, light cyan at its upper left going to deeper cyan, a dark teal base on row 111."""
    hat = U(ellipse(418.3, 106.75, 1.45, 1.0), rect(414.9, 106.9, 420.6, 108.0, 0.45))
    return [
        ("board edge", rect(420.0, 110.95, 434.0, 121.0, 0.6), ("sample_in", rect(433, 113, 434, 118))),
        ("board", rect(421.0, 112.0, 433.0, 120.0), ("sample_in", rect(425, 117, 430, 118))),
        ("board left", limb([(420.5, 112.0, 1.0), (420.5, 120.4, 1.0), (422.6, 120.5, 1.0)], cap="butt"),
         ("sample_in", rect(420, 114, 421, 118))),
        ("text 1", rect(423.0, 113.0, 430.0, 114.0), ("sample_in", rect(424, 116, 431, 117))),
        ("text 2", rect(423.0, 116.0, 432.0, 117.0), ("as", "text 1")),
        ("text 3", rect(423.0, 118.0, 432.0, 119.0), ("as", "text 1")),
        ("grey 1", U(rect(423.0, 115.0, 427.0, 116.0), rect(423.0, 114.0, 425.0, 115.0)), ("sample_in", rect(424, 115, 427, 116))),
        ("grey 2", rect(424.0, 119.0, 430.0, 120.0), ("as", "grey 1")),
        ("bullets", U(rect(422.0, 113.0, 423.0, 114.0), rect(422.0, 116.0, 423.0, 117.0), rect(422.0, 118.0, 423.0, 119.0)),
         ("sample_in", rect(422, 113, 423, 114))),
        ("clip edge", inter(ellipse(426.55, 111.5, 3.05, 3.55), {"t": "half", "p": [0, 111.6], "n": [0, 1]}),
         ("sample_in", rect(424, 110, 425, 111))),
        ("clip", inter(ellipse(426.55, 111.5, 2.05, 2.55), {"t": "half", "p": [0, 111.0], "n": [0, 1]}),
         ("paint", _resume_paint("clip"))),
        ("clip base", rect(424.9, 111.0, 429.0, 112.0), ("sample_in", rect(425, 111, 428, 112))),
        ("hat", hat, ("paint", _resume_paint("hat"))),
        ("head", ellipse(418.85, 109.35, 1.85, 1.6), ("sample_in", rect(418, 108, 420, 110))),
        ("torso", rect(417.9, 112.0, 420.15, 120.2, 0.3), ("as", "head")),
        ("arm", limb([(416.6, 113.0, 1.5), (415.6, 114.4, 1.45), (415.45, 116.5, 1.4), (416.2, 117.9, 1.5),
                      (418.0, 118.3, 1.6)]), ("as", "head")),
        ("shoulders", rect(415.9, 111.95, 420.0, 113.0, 0.45), ("paint", _resume_paint("shoulders"))),
        ("forearm light", rect(415.95, 116.95, 418.0, 117.95, 0.4), ("sample_in", rect(416, 117, 418, 118))),
        ("hips", rect(417.0, 118.9, 420.1, 120.4, 0.3), ("as", "head")),
        ("left leg", limb([(418.0, 119.8, 2.0), (418.0, 124.4, 1.9)]), ("as", "head")),
        ("left foot", limb([(418.3, 124.6, 1.2), (418.45, 126.6, 1.0)]), ("sample_in", rect(418, 125, 419, 127))),
        ("right leg", limb([(420.95, 119.8, 2.0), (420.85, 124.0, 1.9), (420.3, 125.7, 1.4), (419.8, 126.6, 1.3)]),
         ("as", "head")),
        ("ground line", rect(415.95, 126.95, 423.0, 127.95, 0.3), ("as", "head")),
    ]


el_employee.GLYPHS["employee-resume"] = glyph_resume
