"""P41 hud_panels round 3 (2026-11-24): golfer-trophy-button (rejected twice, no note), two versions.

The tan bubble, its rim, the lavender ground and the cast shadow stay the approved recipe of its two neighbours in the same
column (golfer-club-button and golfer-crossed-button, approved in round 2 with the same bubble), so only the cup changes.

Round 2 beside the art at 14x (fix3.side, hex dump of J 7,571), the five largest differences:
1. the handles: perfect rings 1.3 px thick round a 0.85 px hole, floating beside the walls; the art's handles are
   one-pixel rings round a 2 x 2 lit hole (left x 9-10, right x 20-21, rows 574-575) that close on the cup's walls.
2. the bowl's foot: round 2 ended in a wide flat bottom with round corners; the art's bowl narrows over two rows
   (581: x 12-18, 582: x 13-18 with its dark point at x 15-16) straight into the stem.
3. the shading: a smooth chrome gradient; the art's bowl is bands, one per column (dark wall 423929 at x 12, olive
   766b55 at 13, khaki b5ad84 at 14, pale cream f7dead at 15-17 on rows 575-579 with one near-white pixel ded6ce at
   (16, 577), grey 8c8471 at 18, dark 5a5238 at 19) and its top rows 572-573 are khaki (c6c694), not lit.
4. the foot: 7.3 px wide; the art's foot is six pixels (x 13-18) on two rows: a dark top (423929) and a lit front face
   (9c8452 bda563 b5ad84 ad945a), its two end pixels half tone.
5. the rim: one row x 11-20 that is LIGHT at its left end (b5ad84) and olive (847348) on its right half.

A (faithful): patches.patch_layers on each state's own art (k = 6 tones).
B (clean): the cup re-drawn with those five fixes, every paint sampled per state on that state's art.
"""
import copy
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import common as C  # noqa: E402
import kit as K  # noqa: E402
import el_bubbles as EB  # noqa: E402
import el_tan2 as T2  # noqa: E402
import el_small2  # noqa: E402,F401
import patches as P  # noqa: E402
from kit import U, rect, circle, ellipse, poly, sub, inter  # noqa: E402,F401

EL = "golfer-trophy-button"
_ORIG = EB.BUILD[EL]
AX = 16.0


def band_shade(key, dx, dy, x0=12, x1=20, y0=573, y1=581):
    """The bowl's paint: one colour per column (rows y0..y1), as a linear paint across the bowl."""
    stops = [[(xx + 0.5 - x0) / (x1 - x0), K.sample(key, rect(xx + dx, y0 + dy, xx + 1 + dx, y1 + dy), depth=0.0)]
             for xx in range(x0, x1)]
    return {"linear": {"a": [x0 + dx, 0], "b": [x1 + dx, 0], "stops": stops}}


def rim_shade(key, dx, dy):
    stops = [[(xx + 0.5 - 11) / 10, K.sample(key, rect(xx + dx, 571 + dy, xx + 1 + dx, 572 + dy), depth=0.0)]
             for xx in range(11, 21)]
    return {"linear": {"a": [11 + dx, 0], "b": [21 + dx, 0], "stops": stops}}


def front_shade(key, dx, dy):
    stops = [[(xx + 0.5 - 12.5) / 7.0, K.sample(key, rect(xx + dx, 587 + dy, xx + 1 + dx, 588 + dy), depth=0.0)]
             for xx in range(13, 19)]
    return {"linear": {"a": [12.5 + dx, 0], "b": [19.5 + dx, 0], "stops": stops}}


def cup_b():
    bowl = {"t": "lathe", "x": AX, "prof": [[572.0, 4.0], [580.7, 4.0], [581.55, 3.55], [582.45, 2.55], [583.05, 1.15]]}
    P_ = []
    for side, cx in (("left", AX - 5.5), ("right", AX + 5.5)):
        ring = sub(ellipse(cx, 575.0, 2.15, 2.15), ellipse(cx, 575.0, 1.05, 1.05))
        how = ("sample_in", rect(8, 574, 9, 576)) if side == "left" else ("as", "handle left")
        P_.append((f"handle {side}", ring, how))
    P_ += [
        ("bowl", bowl, ("paint", band_shade)),
        ("bottom", inter(sub(bowl, {"t": "grow", "of": bowl, "d": -1.0}), {"t": "half", "p": [0, 580.5], "n": [0, -1]}),
         ("sample_in", rect(15, 582, 17, 583))),
        ("highlight", rect(15.0, 574.9, 18.0, 580.0, 1.0), ("sample_in", rect(15, 575, 18, 577))),
        ("specular", ellipse(16.5, 577.5, 0.6, 0.75), ("sample_in", rect(16, 577, 17, 578))),
        ("rim", rect(11.0, 571.0, 21.0, 572.05, 0.45), ("paint", rim_shade)),
        ("stem", rect(15.0, 582.8, 17.0, 586.1, 0.2), ("sample_in", rect(15, 583, 17, 584))),
        ("knot", ellipse(AX, 584.5, 2.0, 0.62), ("sample_in", rect(14, 584, 18, 585))),
        ("foot top", rect(12.55, 586.0, 19.45, 587.0, 0.45), ("sample_in", rect(14, 586, 17, 587))),
        ("foot front", rect(12.55, 587.0, 19.45, 588.0, 0.45), ("paint", front_shade)),
    ]
    return P_


def build(el):
    base = _ORIG(el)
    outA, outB = {}, {}
    for st, key, rct in C.CELLS[el]:
        bg = P.strip_glyph(base[st])
        it = bg[1]
        g = EB.GEOM[el][st]
        bx, by, br = g["body"]
        body = {"t": "circle", "c": [bx, by], "r": br}
        reg = {"t": "circle", "c": [bx, by], "r": br - 1.2}
        sa = copy.deepcopy(bg)
        lay = P.patch_layers(key, rct, bg, reg, thr=20.0, k=6)
        for L in lay:
            L["clip"] = body
        sa[1]["layers"] += lay
        outA[st] = sa
        dx, dy = EB.offset(el, st)
        lb = T2.glyph_layers(cup_b(), key, dx, dy)
        for L in lb:
            L["clip"] = body
            L["name"] = "B " + L["name"]
        sb = copy.deepcopy(bg)
        sb[1]["layers"] += lb
        outB[st] = sb
    return {"a": outA, "b": outB}


EB.BUILD[EL] = build
