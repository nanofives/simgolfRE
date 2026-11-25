"""P41 hud_panels fix round (2026-11-24): the Golfer action panel's club / crossed clubs / trophy bubbles.

Mariano: club and crossed "a little bit sharper"; trophy redo. Re-read from hex dumps of the idle cells (J 0,500 / 0,535 /
0,566):
- The bubble rim is 1.4 px of navy, concentric with the body (profiles at 0-315 deg: two dark samples, then the soft cast
  shadow fading 99 -> 127 to the lower right). Round 1 fitted the rim 0.6 px off the body's centre, which drew a heavy
  crescent; here the rim is body + RIM_SMALL and the cast shadow is refitted on the disc around it.
- Club and crossed clubs: each shaft is two-tone, a one-pixel dark grey-brown core with a lit line on its outer side;
  crisp black grips with a dark brown ferrule; club heads are small blades with a light spot. All drawn hard-edged.
- Trophy: a straight-walled cup (not round 1's vase): rim bar on row 571, walls x 12 and 19 down to row 580, the bottom
  rounding in on rows 581-582, a narrow stem with a knot (rows 583-585), a flat foot of two rows (dark top, lit bottom);
  shaded across (dark left wall, mid, light, a white streak x 15-17 rows 575-580, mid, dark right wall); two ring handles
  mirrored about the cup's axis x 16.
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import common as C  # noqa: E402
import bubble as B  # noqa: E402
import kit as K  # noqa: E402
import el_bubbles as EB  # noqa: E402
import el_tan2 as T2  # noqa: E402
import figs  # noqa: E402
import sdf  # noqa: E402
from kit import U, rect, circle, ellipse, poly, sub, inter, outline  # noqa: E402,F401

limb = figs.limb
RIM_SMALL = 1.4


def glyph_club():
    """Club standing upright with a ball beside its head (J 0,500 idle): grip black x 13 rows 506.3-510 (its top a lighter
    5a5238); shaft x 13 rows 510-521 dark grey-brown with a lit line down its right side (x 14); head a blade at the foot
    pointing left, x 10.2-14, rows 521-523; ball 2 px at x 17-19: white top (row 520), dark lower half (row 521)."""
    return [
        ("shaft light", limb([(14.45, 510.0, 0.95), (14.45, 520.9, 0.95)], cap="butt"), ("sample_in", rect(14, 514, 15, 519))),
        ("shaft", limb([(13.5, 509.8, 1.05), (13.5, 521.2, 1.05)], cap="butt"), "dark"),
        ("grip", limb([(13.5, 506.4, 1.1), (13.5, 510.0, 1.1)], cap="butt"), ("sample_in", rect(13, 507, 14, 510))),
        ("grip top", rect(12.95, 506.0, 14.05, 507.0, 0.2), ("sample_in", rect(13, 506, 14, 507))),
        ("head", poly((10.15, 521.05), (14.05, 520.9), (14.05, 522.1), (13.6, 523.0), (10.5, 523.0), (10.05, 522.3)),
         ("sample_in", rect(11, 521, 13, 523))),
        ("ball shadow", rect(16.95, 520.95, 19.05, 522.0, 0.5), ("sample_in", rect(17, 521, 19, 522))),
        ("ball", rect(16.95, 519.95, 19.05, 521.0, 0.5), ("sample_in", rect(17, 520, 19, 521))),
    ]


def glyph_crossed():
    """Two clubs crossed in an X, mirror images about x 15.5 (J 0,535 idle): each shaft a one-pixel dark grey-brown line
    from its ferrule (dark brown, (12, 553)) up to its head on the far side ((21.4, 542.4)) (the lighter pixels beside it
    alternate sides along the diagonal: antialiasing, not a lit edge, so none is drawn); the head a small blade x 20-23, rows 541-544, mid brown with a light spot at (21.5, 542.5); the grip a black
    diagonal from (11.6, 553.7) down to (10.3, 556.4)."""
    ax = 15.5

    def m(p):
        return (2 * ax - p[0], p[1]) + tuple(p[2:])
    sh = [(12.4, 552.9, 1.15), (20.3, 542.6, 1.1)]
    grip = [(11.9, 553.3, 1.45), (10.45, 556.3, 1.35)]
    head = ellipse(21.45, 542.45, 1.55, 1.15, 20)
    P = []
    for side, f in (("1", lambda p: p), ("2", m)):
        P += [
            (f"shaft {side}", limb([f(p) for p in sh], cap="butt"), "dark" if side == "1" else ("as", "shaft 1")),
            (f"head {side}", ellipse(2 * ax - 21.45, 542.45, 1.55, 1.15, -20) if side == "2" else head,
             ("sample_in", rect(21, 543, 23, 544))),
            (f"head light {side}", circle(f((21.5, 542.4))[0], 542.4, 0.5), ("sample_in", rect(21, 542, 22, 543))),
            (f"ferrule {side}", circle(f((12.1, 553.2))[0], 553.2, 0.75), ("sample_in", rect(11, 553, 13, 554))),
            (f"grip {side}", limb([f(p) for p in grip], cap="butt"), ("sample_in", rect(11, 554, 12, 555))),
        ]
    return P


def glyph_trophy():
    """The cup (J 0,566 idle), axis x 16: see the module note. Profile [y, half width] read row by row: rim 571 (x 11.3-
    20.7), walls 572-580.6 (x 12-20), bottom 581.6 (12.4-19.6) and 582.4 (13.4-18.6), stem 583-585 (x 15-17, knot x 14-18
    at row 584), foot rows 586-588 (x 12.4-19.6). Handles: rings round (10.1, 575.0) and (21.9, 575.0), outer 2.1 x 2.4,
    hole 0.85 x 0.95."""
    axis = 16.0
    prof = [[571.0, 4.75], [572.0, 4.5], [580.6, 4.05], [581.6, 3.65], [582.4, 2.7], [583.0, 1.25], [583.7, 1.1],
            [584.0, 1.95], [584.9, 1.95], [585.1, 1.05], [585.95, 1.05], [586.0, 3.65], [588.0, 3.65]]
    cup = {"t": "lathe", "x": axis, "prof": prof}
    bowl = {"t": "lathe", "x": axis, "prof": [[571.0, 4.75], [572.0, 4.5], [580.6, 4.05], [581.6, 3.65], [582.6, 2.6]]}
    P = []
    for side, cx in (("left", 10.1), ("right", 21.9)):
        ring = sub(ellipse(cx, 575.0, 2.1, 2.4), ellipse(cx + (0.15 if side == "left" else -0.15), 574.9, 0.85, 0.95))
        P.append((f"handle {side}", ring, ("sample_in", rect(8, 573, 9, 576)) if side == "left" else ("as", "handle left")))
    P += [
        ("cup", cup, ("sample_in", rect(15, 584, 17, 586))),
        ("bowl", bowl, ("paint", trophy_shade)),
        ("streak", rect(14.9, 574.6, 17.6, 580.2, 0.9), ("sample_in", rect(15, 575, 17, 580))),
        ("rim", rect(11.25, 570.95, 20.75, 572.0, 0.45), ("sample_in", rect(13, 571, 19, 572))),
        ("bottom", inter(sub(bowl, {"t": "grow", "of": bowl, "d": -1.0}), {"t": "half", "p": [0, 580.4], "n": [0, -1]}),
         ("sample_in", rect(14, 582, 17, 583))),
        ("left wall", rect(11.95, 572.0, 13.0, 581.0), ("sample_in", rect(12, 576, 13, 581))),
        ("foot top", rect(12.35, 586.0, 19.65, 587.0, 0.4), ("sample_in", rect(14, 586, 17, 587))),
        ("foot", rect(12.35, 587.0, 19.65, 588.0, 0.4), "FOOT"),
    ]
    return P


def trophy_shade(key, dx, dy):
    """The bowl's paint across its width, sampled column by column on the art (rows 573-580)."""
    stops = []
    for xx in range(12, 20):
        stops.append([(xx + 0.5 - 11.25) / (20.75 - 11.25), K.sample(key, rect(xx + dx, 573 + dy, xx + 1 + dx, 581 + dy))])
    return {"linear": {"a": [11.25 + dx, 0], "b": [20.75 + dx, 0], "stops": stops}}


def foot_shade(key, dx, dy):
    stops = []
    for xx in range(13, 19):
        stops.append([(xx + 0.5 - 12.35) / (19.65 - 12.35), K.sample(key, rect(xx + dx, 587 + dy, xx + 1 + dx, 588 + dy))])
    return {"linear": {"a": [12.35 + dx, 0], "b": [19.65 + dx, 0], "stops": stops}}


GLYPHS = {"golfer-club-button": glyph_club, "golfer-crossed-button": glyph_crossed, "golfer-trophy-button": glyph_trophy}
_SH = {}


def rim_of(el, st):
    b = EB.GEOM[el][st]["body"]
    return [b[0], b[1], b[2] + RIM_SMALL]


def shadow(st):
    """One cast shadow per state over the three small bubbles, fitted on the lavender disc + stub around the rims."""
    if st not in _SH:
        insts = []
        for el in GLYPHS:
            _, key, rct = [c for c in C.CELLS[el] if c[0] == st][0]
            c = rim_of(el, st)
            _, gshape = EB.ground_small(c, key)
            insts.append((key, c, (lambda gs: (lambda x, y: sdf.sd(gs, x, y) < -0.6))(gshape)))
        lav = K.sample("J", rect(30, 513, 33, 522))
        _SH[st] = B.fit_shadow(insts, lav)
    return _SH[st]


def build(el):
    scenes = {}
    for st, key, rct in C.CELLS[el]:
        g = EB.GEOM[el][st]
        dx, dy = EB.offset(el, st)
        c = rim_of(el, st)
        ground, gshape = EB.ground_small(EB.GEOM[el][st]["c"], key)
        parts = [(n, s, ("paint", foot_shade) if p == "FOOT" else p) for n, s, p in GLYPHS[el]()]
        lay = T2.glyph_layers(parts, key, dx, dy)
        EB.GU[(el, st)] = U(*[L["shape"] for L in lay])
        it = B.item(c, g["body"], EB.PAINT[g["family"]], glyph=lay, shadow=shadow(st), ground=gshape)
        scenes[st] = [K.item(ground, box=[rct[0], rct[1], rct[0] + rct[2], rct[1] + rct[3]]), it]
    return scenes


for _k in GLYPHS:
    EB.BUILD[_k] = build
