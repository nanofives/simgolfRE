"""P41 hud_panels: the tan "big" bubbles (golfer stats on the Member list and the Golfer action panel, thumbs-up on the
Employee panel) and the Golfer info panel's eight tab bubbles. Glyphs read from coverage / hex dumps (2026-11-24).

Each glyph is authored once, in the sheet coordinates of one reference cell (named in its docstring); every other cell
that shows it gets the same parts moved by kit.match_offset (whole pixels, from the art's own coverage).
"""
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import common as C  # noqa: E402
import bubble as B  # noqa: E402
import kit as K  # noqa: E402
import el_bubbles as EB  # noqa: E402
import sdf  # noqa: E402
from kit import U, line, curve, poly, rect, circle, ellipse, inter, sub, outline  # noqa: E402,F401


# ------------------------------------------------------------------------------------------------------------ glyphs
def glyph_golfer_list(club=False):
    """A golfer standing beside a list (reference: M 0,300 idle). Golfer: round head (14.0, 308.4) r 2.1 with a cap peak to
    the right at y 307.4; a shoulder bar x 9.6..17.6, y 312..314.6; arms hanging at both ends (x 9.2 and 16.9, down to
    y 318.6, the left one turning out to x 8.6); a torso x 11.6..14.9 to the hips at y 318.8; two legs (x 12 and 14.9)
    closing in to the feet (x 12..14.9, y 324.6..326.8); a ground line x 10..17.4 at y 327.5. List: three rows (y 310.4,
    316.4, 322.4), each a short dash x 19..22.6 and a round dot (25.5, y) r 1.55.
    club: the info tab's copy also holds a light club in the left hand, its shaft running down to a head at the lower left
    (read on G 500,0: shaft x 511.5 -> 509.8 over rows 22..30, head at (509.4, 30.4); written here moved by -503, +297)."""
    P = [
        ("head", circle(14.0, 308.4, 2.1), ("sample_in", circle(13.5, 308.5, 0.6))),
        ("cap peak", line((15.6, 307.4), (17.6, 307.4), w=1.0), ("as", "head")),
        ("shoulders", rect(9.6, 312.0, 17.6, 314.6, 0.8), ("as", "head")),
        ("left arm", line((9.4, 314.0), (9.2, 316.6), (8.6, 318.8), w=1.2), ("as", "head")),
        ("right arm", line((16.9, 314.0), (16.9, 318.8), w=1.2), ("as", "head")),
        ("torso", rect(11.6, 314.0, 14.9, 319.0), ("as", "head")),
        ("hips", rect(11.0, 317.8, 16.0, 319.0), ("as", "head")),
        ("left leg", line((12.0, 318.8), (12.1, 324.8), w=1.9), ("as", "head")),
        ("right leg", line((14.9, 318.8), (14.8, 324.8), w=1.9), ("as", "head")),
        ("feet", rect(11.6, 324.4, 15.4, 327.0, 0.6), ("as", "head")),
        ("ground", line((10.0, 327.5), (17.4, 327.5), w=1.0), ("as", "head")),
    ]
    for k, yy in enumerate((310.4, 316.4, 322.4)):
        P.append((f"dash {k + 1}", line((19.0, yy), (22.6, yy), w=1.3), ("as", "head")))
        P.append((f"dot {k + 1}", circle(25.5, yy + 0.1, 1.55), ("as", "head")))
    if club:
        P.append(("club shaft", line((8.6, 318.4), (7.9, 322.4), (6.8, 327.2), w=1.0), ("sample_in", circle(8.5, 320.5, 0.3))))
        P.append(("club head", ellipse(6.3, 327.4, 1.2, 0.8, 20), ("as", "club shaft")))
    return P


def glyph_thumbs_up():
    """A green thumbs-up (reference: E 0,500 idle): the thumb standing up from the fist's top, x 14.2..18.6 from y 504.4
    to 513 (dark outline, lighter inside), the fist x 9.6..22, y 512.8..524 with four finger lines across its left part
    (y 514.5, 516.5, 518.5, 520.5, x 9.6..15), and the cuff at its right (x 21.4..23.6, y 513.6..522)."""
    thumb = poly((13.8, 513.2), (13.8, 507.0), (14.6, 504.6), (16.4, 504.2), (17.6, 505.6), (18.0, 509.4), (19.0, 512.8),
                 smooth=2)
    fist = poly((9.4, 513.6), (18.0, 512.6), (21.6, 513.2), (21.8, 521.8), (17.4, 524.6), (12.6, 524.8), (9.6, 522.8),
                smooth=1)
    P = [
        ("cuff", rect(21.2, 513.4, 23.8, 522.0), ("sample_in", rect(22, 515, 23, 520))),
        ("fist", fist, ("sample_in", rect(16, 517, 18, 520))),
        ("thumb", thumb, ("sample_in", circle(14.5, 509.5, 0.3))),
        ("thumb inside", poly((14.8, 512.6), (14.8, 507.4), (15.4, 505.6), (16.4, 505.4), (16.9, 506.8), (17.2, 509.6),
                              (18.0, 512.6), smooth=2),
         ("sample_in", circle(16.0, 508.5, 0.3))),
    ]
    for k, yy in enumerate((514.5, 516.5, 518.5, 520.5)):
        P.append((f"finger line {k + 1}", line((9.6, yy), (15.0, yy), w=0.9), ("sample_in", circle(11.5, yy, 0.3))))
    return P


def glyph_smiley():
    """A smiling face (reference: G 500,50 idle): two upright oval eyes round (516.6, 65.7) and (524.7, 65.7), r 1.7 x 3.9;
    a smile 2 px thick from (508.8, 71.6) down through (520.4, 80.0) up to (532.2, 71.6), its two ends a little thicker."""
    return [
        ("left eye", ellipse(516.6, 65.7, 1.75, 3.9), ("sample_in", circle(516.5, 65.5, 0.6))),
        ("right eye", ellipse(524.7, 65.7, 1.75, 3.9), ("as", "left eye")),
        ("smile", curve((508.6, 71.4), (510.6, 74.6), (513.2, 77.6), (516.6, 79.4), (520.4, 80.0), (524.2, 79.4),
                        (527.6, 77.6), (530.2, 74.6), (532.2, 71.4), w=2.0, smooth=2), ("as", "left eye")),
        ("smile end left", circle(509.2, 72.4, 1.25), ("as", "left eye")),
        ("smile end right", circle(531.6, 72.4, 1.25), ("as", "left eye")),
    ]


def glyph_kick():
    """A big boot kicking a running figure (reference: G 500,100 idle; the roster calls it "a golfer mid swing").
    Boot (lower left), tipped back: its cuff up at the left (x 507..514, y 119.4..122.4), the leg running down to the heel
    (514, 131.6), the foot along the bottom to the right and the toe turned up at (523.6, 124.8). Figure: head round (522.6, 110.6) r 1.8, a sweat drop up-left of it (tip
    (518.6, 106.4), bulb round (517, 109.8)); torso from (523.4, 113.6) to (524.8, 119.4); the left arm down-left to a hand
    at (518, 117.8), the right arm right to (530.6, 115.6); the back leg right to the knee (531, 119.6) then down to the
    foot (533.6, 123.6); the front leg from the hip to (528.6, 121.4) then down to the foot (527.6, 126)."""
    boot = poly((507.0, 122.4), (508.4, 120.8), (510.4, 119.6), (513.8, 119.4), (514.8, 121.4), (515.2, 123.6),
                (516.6, 124.4), (518.4, 124.0), (520.4, 123.4), (522.4, 123.6), (523.6, 124.8), (523.2, 125.8),
                (521.8, 126.6), (519.6, 127.6), (517.4, 128.2), (516.8, 129.8), (515.6, 131.0), (513.6, 131.6),
                (512.0, 130.4), (511.0, 128.0), (510.0, 125.8), (508.4, 124.2))
    drop = poly((518.8, 106.2), (518.8, 108.6), (518.2, 110.4), (516.8, 110.8), (516.0, 109.8), (516.8, 108.2), smooth=1)
    return [
        ("boot", boot, ("sample_in", rect(510, 122, 513, 127))),
        ("drop", drop, ("sample_in", circle(517.6, 108.6, 0.4))),
        ("head", circle(522.7, 110.6, 1.85), ("as", "boot")),
        ("torso", line((523.4, 113.4), (524.6, 119.4), w=3.0, cap="round"), ("as", "boot")),
        ("shoulders", line((521.4, 114.4), (527.0, 114.6), w=1.6, cap="round"), ("as", "boot")),
        ("left arm", line((521.4, 114.6), (519.4, 116.0), (518.0, 117.8), w=1.8, cap="round"), ("as", "boot")),
        ("right arm", line((526.6, 114.8), (529.4, 115.2), (530.8, 116.2), w=1.8, cap="round"), ("as", "boot")),
        ("back leg", line((525.0, 119.4), (528.4, 119.4), (531.2, 119.8), (532.0, 122.4), (534.4, 123.6), w=1.9,
                          cap="round"), ("as", "boot")),
        ("front leg", line((524.2, 120.0), (526.6, 120.8), (528.4, 121.8), (528.2, 125.2), (527.4, 126.4), w=1.9,
                           cap="round"), ("as", "boot")),
    ]


def glyph_camera():
    """A camera (reference: G 500,150 idle): body x 508.6..529.6, y 163.8..177.8; on top a prism (trapezoid x 514.2..523.8
    at its foot, 516..521.6 at y 158.6) and a small button x 527..529.6, y 162.8..164; a light line down each end of the
    body (x 510.5 and 528.5, y 166.4..176.6); the lens round (519.5, 170.0): a light ring r 4.3 round a dark core r 2.4."""
    return [
        ("body", rect(508.6, 163.8, 529.6, 177.8, 1.0), ("sample_in", rect(512, 175, 526, 177))),
        ("prism", poly((514.2, 164.4), (516.0, 158.6), (521.6, 158.6), (523.8, 164.4)), ("as", "body")),
        ("button", rect(527.0, 162.6, 529.6, 164.4, 0.4), ("as", "body")),
        ("left light", line((510.5, 166.4), (510.5, 176.6), w=0.9), ("sample_in", rect(510, 168, 511, 175))),
        ("right light", line((528.5, 166.4), (528.5, 176.6), w=0.9), ("as", "left light")),
        ("lens ring", circle(519.5, 170.1, 4.3), ("sample_in", rect(516, 166.4, 523, 167.4))),
        ("lens", circle(519.5, 170.2, 2.4), ("sample_in", rect(518.5, 169, 520.5, 171))),
    ]


def book(dy=0.0):
    """An open book drawn in outline (reference: G 500,200 idle; dy moves it). Each page: its outer edge upright (x 507.6
    and 532.4, y 214.4..222.2), its top edge curving up over the page and down into the spine (left: 507.6,214.4 ->
    512.4,213.4 -> 516.6,214.6 -> 519.2,217.4), its bottom made of two stacked curves running down into the spine foot
    (519.4, 228.6); a second, inner upright line on each page (x 509.8 / 530.2); the spine x 519.4 from 218 down to 228.6.
    The right page is the left one mirrored about x 520."""
    ax = 520.0

    def mirror(pts):
        return [(2 * ax - x, y) for x, y in pts]
    top = [(507.6, 214.4), (510.0, 213.4), (512.6, 213.2), (515.4, 214.0), (517.6, 215.6), (519.4, 217.8)]
    outer = [(507.6, 214.4), (507.6, 222.2)]
    bottom1 = [(507.6, 222.2), (510.8, 222.0), (513.8, 222.8), (516.6, 224.6), (518.6, 226.8), (519.6, 228.6)]
    bottom2 = [(509.8, 220.4), (512.6, 220.8), (515.4, 222.2), (517.8, 224.0), (519.4, 226.0)]
    inner = [(509.8, 214.4), (509.8, 220.6)]
    parts = []
    for side, f in (("left", lambda p: p), ("right", mirror)):
        for nm, pts, w in (("top", top, 1.3), ("outer edge", outer, 1.3), ("bottom", bottom1, 1.4), ("inner bottom", bottom2, 1.1),
                           ("inner edge", inner, 1.1)):
            parts.append((f"{side} page {nm}", curve(*[(x, y + dy) for x, y in f(pts)], w=w, cap="round", smooth=1)))
    parts.append(("spine", line((519.6, 218.0 + dy), (519.6, 228.6 + dy), w=1.2, cap="round")))
    return parts


def glyph_book():
    P = book()
    return [(n, s, "dark" if i == 0 else ("as", P[0][0])) for i, (n, s) in enumerate(P)]


def glyph_check():
    """A bold check mark (reference: G 500,250 idle, on a blue bubble): the short arm's top end cut level at y 270.2
    (x 508.8..515.6), the long arm's top end cut level at y 260.2 (x 526..534.2), the inner corner at (518.4, 274.6), the
    foot cut level at y 281.6 (x 515.6..525.2)."""
    return [("check", poly((508.8, 270.2), (515.6, 270.2), (518.4, 274.6), (526.0, 260.2), (534.2, 260.2), (526.0, 274.6),
                          (525.2, 281.6), (515.6, 281.6), (514.2, 277.4)), ("sample_in", rect(519, 276, 523, 279)))]


def glyph_history():
    """An open book under a green arch (reference: G 500,300 idle): the story tab's outline book moved down 101 px, its
    middle hidden behind a green arch (a half ring round (519.6, 316.2), mid radius 5.2, 2.6 thick, with a dark edge and
    a lighter streak along its outer top), whose right foot runs a little lower (to y 319.6) than its left (317.8)."""
    P = book(101.0)
    out = [(n, s, "dark" if i == 0 else ("as", P[0][0])) for i, (n, s) in enumerate(P)]
    arch = {"t": "arc", "c": [519.6, 316.4], "r": 5.2, "a0": 180.0, "sweep": 195.0, "w": 2.6}
    edge = {"t": "arc", "c": [519.6, 316.4], "r": 5.2, "a0": 178.0, "sweep": 199.0, "w": 4.4}
    out += [("arch edge", edge, ("sample_in", circle(513.5, 317.5, 0.4))),
            ("arch", arch, ("sample_in", rect(515.5, 309.5, 517.5, 310.5))),
            ("arch light", {"t": "arc", "c": [519.6, 316.4], "r": 6.0, "a0": 235.0, "sweep": 70.0, "w": 0.9},
             ("sample_in", rect(518, 309, 521, 310)))]
    return out


def glyph_thought():
    """A navy speech bubble with a light exclamation mark cut out of it (reference: G 500,350 idle): the bubble an ellipse
    round (519.6, 368.4), 12.8 x 7.6; the mark's bar x 518.6..520.6 from y 362.8 down to 369.4 and its dot round
    (519.2, 372.4) r 0.95; the tail from the bubble's foot (521, 375) curving down-left to a point at (517.4, 382.2)."""
    bubble = ellipse(519.6, 368.4, 12.8, 7.6)
    tail = poly((519.0, 374.6), (523.0, 374.8), (522.4, 378.6), (520.0, 381.4), (517.2, 382.6), (519.4, 379.4), (520.0, 376.6),
                smooth=1)
    mark = U(poly((518.4, 362.8), (520.8, 362.8), (520.2, 369.6), (519.0, 369.6)), circle(519.4, 372.4, 1.0))
    return [("speech bubble", sub(U(bubble, tail), mark), ("sample_in", rect(510, 366, 515, 370)))]


# ------------------------------------------------------------------------------------------------------------ grounds
def ground_big(rct, key):
    """The big bubbles sit on a flat lavender square (the cell); the Golfer action panel's copy has its top-right corner cut
    off along the line (37.6, 300) -> (42, 318.2)."""
    x0, y0, w, h = rct
    lav = K.sample(key, rect(x0 + 1, y0 + h - 6, x0 + 6, y0 + h - 1))
    sq = rect(x0, y0, x0 + w, y0 + h)
    if key == "J":
        n = np.array([18.2, -4.4])
        n = n / np.hypot(*n)
        sq = inter(sq, {"t": "half", "p": [x0 + 37.6, y0 + 0.0], "n": [float(n[0]), float(n[1])]})
    return [K.lay(sq, lav)], sq


def ground_tab(rct, key):
    """The Golfer info panel's tab cells: black, a lilac strip across the cell behind the bubble (rows 7..29: a light top
    edge fading into the flat strip, a darker bottom edge) and the strip's dark shadow under it (rows 29..33)."""
    x0, y0, w, h = rct
    col = x0 + w - 4
    S = lambda r: K.sample(key, rect(col, y0 + r, col + 2, y0 + r + 1))  # noqa: E731
    top = y0 + 7
    stops = [[(r - 7) / 25.0, S(r)] for r in range(7, 33)]
    strip = rect(x0, top, x0 + w, y0 + 33)
    L = [K.lay(rect(x0, y0, x0 + w, y0 + h), [0, 0, 0]),
         K.lay(strip, {"linear": {"a": [0, top + 0.5], "b": [0, top + 25.5], "stops": stops}})]
    above = [S(r) for r in range(0, 7)]
    if max(max(c) for c in above) > 8:     # the history and thought cells start under a cream band (the panel above)
        L.insert(1, K.lay(rect(x0, y0, x0 + w, top), {"linear": {"a": [0, y0 + 0.5], "b": [0, y0 + 6.5],
                                                                 "stops": [[r / 6.0, above[r]] for r in range(7)]}}))
    return L, rect(x0, top, x0 + w, y0 + 29)


# ------------------------------------------------------------------------------------------------------------ elements
REF = {   # element -> (glyph function, reference element, reference state)
    "golfer-stats-button": (glyph_golfer_list, "golfer-stats-button", "idle"),
    "joecool-stats-button": (glyph_golfer_list, "golfer-stats-button", "idle"),
    "employee-thumbs-up": (glyph_thumbs_up, "employee-thumbs-up", "idle"),
    "golfer-tab-info": (lambda: glyph_golfer_list(club=True), "golfer-stats-button", "idle"),
    "golfer-tab-mood": (glyph_smiley, "golfer-tab-mood", "idle"),
    "golfer-tab-swing": (glyph_kick, "golfer-tab-swing", "idle"),
    "golfer-tab-photo": (glyph_camera, "golfer-tab-photo", "idle"),
    "golfer-tab-story": (glyph_book, "golfer-tab-story", "idle"),
    "golfer-tab-ok": (glyph_check, "golfer-tab-ok", "idle"),
    "golfer-tab-history": (glyph_history, "golfer-tab-history", "idle"),
    "golfer-tab-thought": (glyph_thought, "golfer-tab-thought", "idle"),
}


def build(el):
    gl, ref_el, ref_st = REF[el]
    rg = EB.GEOM[ref_el][ref_st]
    scenes = {}
    for st, key, rct in C.CELLS[el]:
        g = EB.GEOM[el][st]
        dx, dy = (0, 0) if (el, st) == (ref_el, ref_st) else K.match_offset((rg["key"], rg["body"]), (key, g["body"]))
        if key == "G":
            ground, gshape = ground_tab(rct, key)
        else:
            ground, gshape = ground_big(rct, key)
        lay = EB.glyph_layers(gl(), key, dx, dy)
        EB.GU[(el, st)] = U(*[L["shape"] for L in lay])
        fam = g["family"]
        it = B.item(g["c"], g["body"], EB.PAINT[fam], glyph=lay, shadow=EB.SHADOWS.get(fam), ground=gshape)
        scenes[st] = [K.item(ground, box=[rct[0], rct[1], rct[0] + rct[2], rct[1] + rct[3]]), it]
    return scenes


def fit_shadows():
    out = {}
    fams = {}
    for el in REF:
        for st, key, rct in C.CELLS[el]:
            g = EB.GEOM[el][st]
            if key == "G":
                _, gs = ground_tab(rct, key)
                prof = (lambda r: (lambda x, y: C.sheet("G")[0][np.floor(y).astype(int), r[0] + r[2] - 3]))(rct)
            else:
                _, gs = ground_big(rct, key)
                prof = K.sample(key, rect(rct[0] + 1, rct[1] + rct[3] - 6, rct[0] + 6, rct[1] + rct[3] - 1))
            ok = (lambda s: (lambda x, y: sdf.sd(s, x, y) < -0.6))(gs)
            fams.setdefault(g["family"], []).append(((key, g["c"], ok), prof))
    for fam, items in fams.items():
        if callable(items[0][1]):
            groups = items

            def gfn(x, y, _items=groups):
                return np.concatenate([p(x, y) for _, p in _items])
            # one profile per instance: fit each family on its first instance's profile (the strip is the same in each)
            out[fam] = B.fit_shadow([items[0][0]], items[0][1])
        else:
            out[fam] = B.fit_shadow([i for i, _ in items], items[0][1])
        print("shadow", fam, out[fam])
    return out


EB.BUILD.update({k: build for k in REF})
EB.SHADOW_FITS.append(fit_shadows)
