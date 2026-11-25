"""P41 hud_panels round 4 (2026-11-25): member-mood-faces and member-mood-faces-extra, ONE clean version drawn from
log/roadmap/P41/work4/mood_recipe.md (the recipe the marks area also follows for gb-mood).

Mariano, round 3: "i want a clean version but this still needs something in the middle of capturing the actual emotions of
the faces + look believable". So the features are no longer copied from the art's pixel blocks: each face is drawn from its
emotion (recipe table), with brows / eyes / mouth shaped to carry it, thick round-ended features, a baked lighter centre and
darker rim, cheek light on the smiles, soft shadows under brows and mouth, and a glint per eye. Colours per face come from
the art (recipe colour table).

Everything below is in face-relative units of a radius-8 face (the recipe's coordinates); `F` maps them into sheet pixels.
"""
import copy
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import common as C  # noqa: E402
import kit as K  # noqa: E402
import el_bubbles as EB  # noqa: E402
import el_marks as EM  # noqa: E402
import el_faces2  # noqa: E402,F401   sets C.CELLS for the two elements (ten 16 px cells; the extra rows' faces 5-8)
import figs  # noqa: E402
from kit import rect  # noqa: E402

ELS = ("member-mood-faces", "member-mood-faces-extra")


def hx(s):
    s = s.lstrip("#")
    return [int(s[i:i + 2], 16) for i in (0, 2, 4)]


def mix(a, b, t):
    return [round(a[i] * (1 - t) + b[i] * t, 1) for i in range(3)]


def scale(a, k):
    return [round(min(255.0, v * k), 1) for v in a]


# recipe colour table: light, base, rim, outer rim, ink (eyes), line (mouth / brows), brow (if different)
COL = {
    1: ("5aef00", "31de00", "217b10", "184a10", "181810", "394231"),
    2: ("5aef00", "31de00", "217b10", "184a10", "181810", "394231"),
    3: ("8cff20", "5aef00", "217b10", "184a10", "181810", "394231"),
    4: ("8cff20", "5aef00", "217b10", "184a10", "15152b", "184a10"),
    5: ("d8ff30", "b5ef00", "5a9c00", "5a5310", "181810", "5a5310"),
    6: ("fffa60", "fdef02", "c6ad00", "94ad00", "181810", "181810"),
    7: ("ffb830", "ef9c00", "b55a00", "7b5208", "181818", "4a3910"),
    8: ("e86020", "ce4a00", "8c1000", "522939", "181818", "4a3910"),
    9: ("f70000", "ce0000", "8c1000", "5a1010", "181818", "521010"),
    10: ("f70000", "ce0000", "8c1000", "5a1010", "181818", "521010"),
}
AXIS = {n: 8.0 for n in range(1, 11)}      # drawn centred in the cell (the art's half-pixel offsets on 4, 5, 7 dropped)
TEETH, TOOTH_HI, TOOTH_LINE = hx("d6d6d6"), hx("fdfdf0"), hx("a0a0a0")


class F:
    """face-relative (radius 8, y down) -> sheet coordinates of one cell."""

    def __init__(self, x0, y0, n):
        self.ox, self.oy = x0 + AXIS[n], y0 + 8.0 + (0.3 if n == 10 else 0.0)   # face 10 sits 0.3 lower (horn room)

    def p(self, dx, dy):
        return [round(self.ox + dx, 3), round(self.oy + dy, 3)]

    def circle(self, dx, dy, r):
        return {"t": "circle", "c": self.p(dx, dy), "r": r}

    def ellipse(self, dx, dy, rx, ry, rot=0.0):
        return {"t": "ellipse", "c": self.p(dx, dy), "r": [rx, ry], "rot": rot}

    def stroke(self, pts, w, smooth=0):
        S = {"t": "stroke", "pts": [self.p(*q) for q in pts], "w": w, "cap": "round"}
        if smooth:
            S["smooth"] = smooth
        return S

    def poly(self, pts, smooth=0):
        S = {"t": "poly", "rings": [[self.p(*q) for q in pts]]}
        if smooth:
            S["smooth"] = smooth
        return S

    def rrect(self, dx, dy, hx_, hy, r):
        return {"t": "rrect", "c": self.p(dx, dy), "half": [hx_, hy], "r": r}

    def below(self, dy):          # half plane y > dy
        return {"t": "half", "p": self.p(0, dy), "n": [0.0, -1.0]}

    def above(self, dy):          # half plane y < dy
        return {"t": "half", "p": self.p(0, dy), "n": [0.0, 1.0]}


def sym(half):
    """a left-to-right polyline from its left half (the points from the left end to the centre, centre excluded)."""
    return half + [(-x, y) for x, y in reversed(half)]


def U(*s):
    return {"t": "union", "of": list(s)}


def inter(*s):
    return {"t": "inter", "of": list(s)}


def grow(s, d):
    return {"t": "grow", "of": s, "d": d}


def move(s, dy):
    return {"t": "move", "of": s, "d": [0.0, dy]}


def features(f, n):
    """(brows, eyes [(shape, glint centre, glint r)], mouth layers builder) per the recipe."""
    brows, eyes, mouth = [], [], []
    G = 0.45

    def ell_eyes(cx, cy, rx, ry, cut=None):
        out = []
        for s in (-1, 1):
            e = f.ellipse(s * cx, cy, rx, ry)
            if cut is not None:
                e = inter(e, f.below(cut))
            gy = cy - 0.7 if cut is None else max(cy - 0.7, cut + 0.5)
            out.append((e, (s * (cx - 0.35), gy), G))
        return out

    def cap_eyes(a, b, w, g=0.35):
        out = []
        for s in (-1, 1):
            e = f.stroke([(s * a[0], a[1]), (s * b[0], b[1])], w)
            mx, my = (a[0] + b[0]) / 2, (a[1] + b[1]) / 2
            out.append((e, (s * (mx - 0.3), my - 0.3), g))
        return out

    def pair(a, b, w):
        return [f.stroke([(-a[0], a[1]), (-b[0], b[1])], w), f.stroke([(a[0], a[1]), (b[0], b[1])], w)]

    if n in (1, 2):
        eyes = ell_eyes(3.0, -3.4, 1.05, 2.3)
    if n == 1:
        m = f.poly([(0, 0.1), (-3.2, 0.2), (-5.6, 0.9), (-4.0, 3.2), (-2.6, 4.6), (0, 4.9),
                    (2.6, 4.6), (4.0, 3.2), (5.6, 0.9), (3.2, 0.2)], smooth=2)
        teeth = inter(grow(m, -0.7), f.above(3.5))
        mouth = [("mouth", m, "mouthfill"), ("teeth", teeth, TOOTH_HI),
                 ("teeth shade", inter(teeth, f.below(2.4)), TEETH),
                 ("teeth line", inter(f.stroke([(-5, 2.4), (5, 2.4)], 0.35), teeth), TOOTH_LINE)]
    if n == 2:
        opening = f.poly([(-4.6, 2.0), (-2.5, 2.6), (2.5, 2.6), (4.6, 2.0), (3.4, 3.9), (2.0, 4.8), (-2.0, 4.8),
                          (-3.4, 3.9)], smooth=2)
        low_teeth = f.rrect(0, 3.3, 2.1, 0.55, 0.35)
        mouth = [("mouth", opening, "mouthfill"), ("teeth", low_teeth, TEETH),
                 ("teeth top", inter(low_teeth, f.above(3.3)), TOOTH_HI),
                 ("smile", f.stroke(sym([(-5.6, 0.9), (-4.6, 2.0), (-2.5, 2.6)]), 1.3, smooth=2), "line")]
    if n == 3:
        eyes = ell_eyes(3.0, -3.1, 1.0, 2.0)
        mouth = [("smile", f.stroke(sym([(-5.6, 1.4), (-4.4, 2.6), (-3.0, 3.5)]), 1.3, smooth=2), "line")]
    if n == 4:
        eyes = ell_eyes(2.5, -3.0, 1.15, 1.35)
        mouth = [("smile", f.stroke(sym([(-5.0, 1.3), (-3.6, 2.5), (-1.8, 3.4)]), 1.25, smooth=2), "line")]
    if n == 5:
        eyes = ell_eyes(3.0, -2.2, 1.45, 1.75)
        mouth = [("smile", f.stroke(sym([(-4.6, 2.2), (-2.8, 3.0)]), 1.15, smooth=2), "line")]
    if n == 6:
        eyes = ell_eyes(3.0, -2.6, 1.45, 1.45)
        mouth = [("mouth", f.stroke([(-4.6, 2.5), (4.6, 2.5)], 1.25), "line")]
    if n == 7:
        brows = pair((4.0, -5.5), (1.4, -6.0), 1.0)
        eyes = ell_eyes(2.5, -3.6, 1.3, 1.3, cut=-4.3)
        mouth = [("frown", f.stroke(sym([(-4.6, 2.6), (-3.0, 1.7)]), 1.25, smooth=2), "line")]
    if n == 8:
        brows = pair((4.0, -5.3), (1.5, -5.9), 1.15)
        eyes = ell_eyes(2.5, -3.6, 1.4, 1.3, cut=-4.4)
        # a rounded arch through the art's apex (row 8) and corners (rows 10-11), tapering to its corners, and the pout as
        # a lit bulge with a shadow under it (the flat-topped arch + a dark lip line read as a moustache over a second
        # mouth at 12x; the marks area found it first, both areas now draw this form)
        frown = figs.limb([(f.p(x, y)[0], f.p(x, y)[1], w) for x, y, w in
                           ((-5.0, 4.1, 1.0), (-4.2, 2.7, 1.25), (-2.7, 1.45, 1.3), (0.0, 0.95, 1.35), (2.7, 1.45, 1.3),
                            (4.2, 2.7, 1.25), (5.0, 4.1, 1.0))], smooth=2)
        lip = f.stroke([(-1.3, 2.4), (1.3, 2.4)], 0.9)
        pout = f.ellipse(0, 2.15, 1.5, 0.65)
        mouth = [("pout shadow", f.ellipse(0, 3.0, 1.6, 0.5), "pout_shadow"), ("pout", pout, "pout"),
                 ("pout edge", inter(lip, move(pout, 0.25)), "pout_edge"), ("frown", frown, "line")]
    if n == 9:
        brows = pair((4.3, -5.1), (1.4, -6.4), 1.15)
        eyes = [(f.ellipse(s * 2.8, -3.5, 1.45, 1.05, rot=s * 30.0), (s * 2.5, -4.0), 0.4) for s in (-1, 1)]
        mouth = [("frown", f.stroke(sym([(-5.6, 3.7), (-4.6, 2.0), (-2.8, 0.6)]), 1.4, smooth=2), "line")]
    if n == 10:
        brows = pair((4.8, -5.5), (1.3, -3.6), 1.3)
        eyes = cap_eyes((4.5, -3.1), (1.6, -2.0), 1.2)
        box = f.poly([(-4.4, 0.8), (-3.0, 0.3), (0, 0.1), (3.0, 0.3), (4.4, 0.8), (4.4, 3.8), (3.0, 3.5), (0, 3.4),
                      (-3.0, 3.5), (-4.4, 3.8)], smooth=2)
        teeth = grow(box, -0.65)
        gaps = U(*[f.stroke([(x, -1), (x, 5)], 0.35) for x in (-3.0, -1.0, 1.0, 3.0)], f.stroke([(-5, 2.0), (5, 2.0)], 0.4))
        mouth = [("lips", box, "line"), ("teeth", teeth, TEETH),
                 ("teeth top", inter(teeth, f.above(1.2)), TOOTH_HI),
                 ("tooth lines", inter(gaps, teeth), hx("848484"))]
    return brows, eyes, mouth


def horns(f):
    out = []
    for s in (-1, 1):
        # a curved cone standing on the crown, leaning outward: base inside the disc, tip up and out
        out.append(f.poly([(s * 4.4, -4.4), (s * 5.7, -5.1), (s * 6.4, -6.4), (s * 6.55, -8.0), (s * 5.2, -7.6),
                           (s * 3.4, -6.8), (s * 1.9, -6.0)], smooth=1))
    return out


def face(x0, y0, n):
    f = F(x0, y0, n)
    light, base, rim, outer, ink, line = (hx(c) for c in COL[n])
    out = []
    # face 10: the head sits lower and a little smaller (the art's red head spans rows 1-15, its horns row 0), so the
    # horns stand clear of the cell top
    cy, R = (0.0, 7.4) if n == 10 else (-0.1, 7.7)
    disc = f.circle(0, cy, R)
    skin = f.circle(0, cy, R - 0.8)
    if n == 10:
        hs = horns(f)
        for h, sd_ in zip(hs, (-1, 1)):      # lit on the inner side (toward the light centre), rim colour outside
            out.append(K.lay(h, scale(rim, 0.65)))
            out.append(K.lay(grow(h, -0.5), {"linear": {"a": f.p(sd_ * 5.6, -6.2), "b": f.p(sd_ * 3.2, -7.4),
                                                      "stops": [[0.0, rim], [1.0, light]]}}))
    out.append(K.lay(disc, outer))
    out.append(K.lay(grow(disc, -0.4), rim, soft=0.5))
    out.append(K.lay(skin, {"radial": {"c": f.p(-1.0, -2.4), "r": 9.0,
                                       "stops": [[0.0, light], [0.5, base], [0.8, mix(base, rim, 0.55)], [1.0, rim]]}},
                     soft=0.8))
    if n <= 4:
        for s in (-1, 1):
            out.append(K.lay(f.ellipse(s * 4.4, -0.4, 1.5, 1.0), light, alpha=0.55, soft=0.8, clip=skin))
    if n == 5:
        for s in (-1, 1):
            out.append(K.lay(f.ellipse(s * 4.3, 1.3, 1.3, 0.7), hx("d4ca07"), alpha=0.8, soft=0.6, clip=skin))
    brows, eyes, mouth = features(f, n)
    shade = scale(base, 0.75)
    for b in brows:
        out.append(K.lay(move(b, 0.7), shade, alpha=0.5, soft=0.6, clip=skin))
    mshape = U(*[s for _, s, pk in mouth if not str(pk).startswith("pout")]) if n != 8 else         f.stroke([(-1.3, 2.4), (1.3, 2.4)], 0.9)        # face 8: the shadow falls under the lower lip only
    out.append(K.lay(move(mshape, 0.9), scale(base, 0.8), alpha=0.35, soft=0.7, clip=skin))
    bcol = hx("7b5208") if n == 7 else line
    for b in brows:
        out.append(K.lay(b, bcol, name="brow"))
        out.append(K.lay(grow(b, -0.3), scale(bcol, 0.75), name="brow core"))
    edge_ink = mix(ink, rim, 0.35)
    for e, g, gr in eyes:
        out.append(K.lay(e, edge_ink, name="eye"))
        out.append(K.lay(grow(e, -0.3), ink, name="eye core"))
        if n == 7 or n == 8:     # the lowered lid along the flat top
            out.append(K.lay(inter(grow(e, 0.15), f.above(-4.3 if n == 7 else -4.4 + 0.0)), rim, name="lid"))
    for name, s, pk in mouth:
        if pk == "mouthfill":
            out.append(K.lay(s, mix(line, ink, 0.25), name=name))
            out.append(K.lay(grow(s, -0.35), mix(line, ink, 0.6), name=name + " core"))
        elif pk == "pout_shadow":
            out.append(K.lay(s, scale(base, 0.72), alpha=0.6, soft=0.5, clip=skin, name=name))
        elif pk == "pout":
            out.append(K.lay(s, light, alpha=0.6, soft=0.55, clip=skin, name=name))
        elif pk == "pout_edge":
            out.append(K.lay(s, scale(base, 0.8), alpha=0.5, name=name))
        elif pk == "line":
            out.append(K.lay(s, line, name=name))
            out.append(K.lay(grow(s, -0.3), scale(line, 0.75), name=name + " core"))
        else:
            out.append(K.lay(s, pk, name=name))
    gl = hx("848484") if n == 10 else hx("c8c8c8")
    for e, g, gr in eyes:
        out.append(K.lay(f.circle(g[0], g[1], gr), gl, name="glint", clip=e))
    return out


def build(el):
    out = {}
    for st, key, rct in C.CELLS[el]:
        x0, y0, w, h = rct
        n = int(st.split()[-1])
        bgl = [K.lay(rect(x0, y0, x0 + w, y0 + h), EM.cell_bg(key, rct))]
        out[st] = [K.item(copy.deepcopy(bgl) + face(x0, y0, n), box=[x0, y0, x0 + w, y0 + h])]
    return out


for _el in ELS:
    EB.BUILD[_el] = build
