"""P41 hud_panels round 3 (2026-11-24): tab-golfer-staff and joecool-tab-staff (rejected twice, no note), two versions.

The blue glass tab under the figure stays the approved recipe of its neighbours (tab-golfer, tab-golfer-swing,
tab-golfers-group, tab-staff-group were approved in round 2 on the same disc, rim and halo), so only the figure changes.

Round 2 beside the art at 14x (fix3.side), the five largest differences:
1. the head: an inverted drop 3.4 x 4 px reaching the shoulders; the art's head is a 3 x 3 block (x 13-15, rows 455-457,
   one more pixel at (12, 456)) with a one-pixel chin (14, 458) and a LIGHT row 459 between head and shoulders.
2. the left arm: a thick limb with a knob at its top that read as a raised fist; the art's arm is a thin '<' (one pixel,
   58-95) from the shoulder (10, 460) out to the elbow (9, 463) and in to the waist (12, 467), with a light gap between it
   and the shirt.
3. the shirt: three equal dark bars; in the art the shirt is dark only at its two sides and seam (x 13, 15, 17) with the
   lit tab colour (145) between them, so the chest reads light.
4. the lower body: a block wider than the shirt and legs 2 px with long feet; the art's hips are x 12-16 (rows 464-467),
   its legs x 12-13 and 15-16 with a light gap at x 14, its feet two rows (474-475) and four pixels long (x 10-13,
   x 15-18).
5. the tone: one flat navy; the art's figure has a dark core (43-51) inside a lighter edge (58-78).

A (faithful): patches.patch_layers on the art (dark pixels only, k = 4 tones): every tone patch its own shape and colour.
B (clean): the parts above as shapes, the figure painted as a dark core over a lighter edge (the edge tone is the figure
drawn whole, the core the figure shrunk 0.28 px, both colours sampled on the art; the chest and leg gaps
in the tab body's own lit paint).
"""
import copy
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import common as C  # noqa: E402
import kit as K  # noqa: E402
import el_bubbles as EB  # noqa: E402
import figs  # noqa: E402
import fit  # noqa: E402
import patches as P  # noqa: E402
from kit import U, rect, circle, ellipse, poly, sub  # noqa: E402,F401

limb = figs.limb
ELS = ("tab-golfer-staff", "joecool-tab-staff")
_ORIG = {el: EB.BUILD[el] for el in ELS}


def staff_b():
    """The man and his post (sheet M 0,450 coordinates; pixel i covers [i, i+1))."""
    head = U(ellipse(14.4, 456.5, 1.6, 1.45),
             poly((13.6, 457.3), (15.25, 457.3), (14.85, 458.5), (14.5, 458.95), (14.15, 458.5), smooth=1))
    torso = poly((12.85, 459.95), (17.95, 459.95), (17.5, 461.0), (16.95, 463.3), (16.95, 464.2),
                 (16.95, 468.1), (12.05, 468.1), (11.45, 465.6), (12.25, 464.1), (12.85, 461.2), smooth=1)
    left_arm = limb([(10.6, 460.35, 0.95), (10.15, 461.9, 1.0), (9.6, 463.35, 1.05), (10.35, 464.6, 1.05),
                     (11.3, 465.6, 1.05), (12.3, 466.6, 1.0)])
    right_arm = limb([(17.45, 460.45, 1.0), (18.15, 461.3, 1.0), (19.25, 462.05, 1.05), (20.6, 462.55, 1.1)])
    hand = circle(20.95, 462.7, 0.75)
    legs = [limb([(12.95, 467.8, 1.8), (12.9, 470.8, 1.7), (12.8, 473.9, 1.65)]),
            limb([(15.95, 467.8, 1.8), (15.95, 470.8, 1.7), (16.05, 473.9, 1.65)])]
    feet = [limb([(13.15, 474.85, 1.45), (11.6, 474.95, 1.5), (10.65, 474.9, 1.3)]),
            limb([(15.65, 474.85, 1.4), (17.1, 474.95, 1.45), (18.25, 474.9, 1.2)])]
    post = limb([(21.5, 459.5, 0.95), (21.5, 472.9, 0.9)], cap="butt")
    knob = circle(21.5, 458.4, 1.3)
    fig = [("head", head), ("torso", torso), ("left arm", left_arm), ("right arm", right_arm),
           ("hand", hand), ("left leg", legs[0]), ("right leg", legs[1]), ("left foot", feet[0]),
           ("right foot", feet[1]), ("post", post), ("knob", knob)]
    # the lit chest: the tab's own colour shows between the shirt's sides and seam (x 14 and x 16, rows 460-463)
    slits = [("chest left", limb([(14.5, 460.25, 0.9), (14.5, 463.5, 0.85)], cap="butt")),
             ("chest right", limb([(16.45, 460.5, 0.8), (16.4, 463.25, 0.75)], cap="butt")),
             ("leg gap", limb([(14.45, 468.2, 0.95), (14.45, 474.3, 1.0)], cap="butt"))]
    return fig, slits


def _body(scene):
    return [L for L in scene[1]["layers"] if L["shape"]["t"] == "circle"][-1]["shape"]


def build(el):
    base = _ORIG[el](el)
    ref = "tab-golfer-staff"
    outA, outB = {}, {}
    for st, key, rct in C.CELLS[el]:
        bg = P.strip_glyph(base[st])
        body = _body(bg)
        reg = copy.deepcopy(body)
        reg["r"] -= 1.6
        # A
        sa = copy.deepcopy(bg)
        lay = P.patch_layers(key, rct, bg, reg, thr=18.0, k=4, sign=-1)
        for L in lay:
            L["clip"] = body
        sa[1]["layers"] += lay
        outA[st] = sa
        # B
        g = EB.GEOM[el][st]
        rg = EB.GEOM[ref]["idle"]
        dx, dy = (0, 0) if el == ref else K.match_offset((rg["key"], rg["body"]), (key, g["body"]))
        fig, slits = staff_b()
        fu = U(*[K.shift(s, dx, dy) for _, s in fig])
        mask, art = P.glyph_mask(key, rct, bg, reg, 18.0, -1)
        X = art[mask]
        Lm = X.mean(axis=1)
        core = [round(float(v), 1) for v in np.median(X[Lm <= np.quantile(Lm, 0.4)], axis=0)]
        edge = [round(float(v), 1) for v in np.median(X[(Lm > np.quantile(Lm, 0.4)) & (Lm <= np.quantile(Lm, 0.85))], axis=0)]
        sb = copy.deepcopy(bg)
        gl = [{"name": "B edge", "shape": fu, "fill": edge, "clip": body},
              {"name": "B core", "shape": {"t": "grow", "of": fu, "d": -0.28}, "fill": core, "clip": body}]
        # the chest and leg gaps take the tab body's own lit colour at that spot (the body paint, clipped to the slit)
        bl = [L for L in bg[1]["layers"] if L["shape"] == body][0]
        lit = {"bevel": dict(bl["fill"]["bevel"], of=body)}
        for n, s in slits:
            gl.append({"name": f"B {n}", "shape": K.shift(s, dx, dy), "fill": lit, "clip": body})
        sb[1]["layers"] += gl
        outB[st] = sb
    return {"a": outA, "b": outB}


for _el in ELS:
    EB.BUILD[_el] = build
