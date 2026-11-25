"""P41 hud_panels round 4 (2026-11-25): tab-golfer-staff and joecool-tab-staff, one clean version.

Mariano, round 3: "not centered, this doesn't look like a person holding a shovel". The glass tab under the figure stays
the approved blue tab recipe (round-2 builder, glyph layers stripped); only the figure is new.

Reading of the art (M 0,450, luminance printed per pixel, 20x view):
- head x 12-15, rows 455-458 (one more dark pixel at (12, 456): a cap brim to the viewer's left), chin (14, 458);
- shoulders row 460 from x 10 to 18; the chest is the tab's lit colour with dark vertical lines at x 13 and 15 (overall
  straps) and x 17 (the shirt's right side), rows 460-463; rows 464-467 dark across x 12-16 (the overalls' bib and hips);
- left arm akimbo: shoulder (10-11, 460) out to the elbow (9, 463) and back in to the hip (11-12, 465-466);
- right arm: down from (17-18, 460-461), then across row 462 to x 20-21 where the hand meets the shaft;
- the shaft x 21 from row 457 to row 472, a 3 x 3 grip block on top (x 20-22, rows 457-459);
- legs x 12-13 and 15-16 rows 468-473 with a lit gap at x 14; feet rows 474-475, x 10-13 (left) and x 15-18 (right).
The art shows NO blade at the bottom of the shaft (rows 473-475 at x 19-22 are the tab's rim shading). The brief asks for
"a straight shaft with a visible blade at the ground", so a spade blade is drawn at the foot of the shaft, standing on the
same ground line as the feet, with a D-grip on top where the art's grip block is.

Proportions (brief): head 3.5 of 20.6 total height (1/5.9), shoulders 6.9 wide against a 3.4 wide head. The whole group
(figure + shovel, its exact bounding box x 9.15-22.75) is centred on the tab's visible disc (16.5, 466.5) horizontally,
and stands on the art's ground line (feet and blade tip at the bottom of row 475, as in the art).

Paint: the figure is the art's navy, a darker core 0.3 inside a lighter edge (both sampled on the art); the shirt between
the straps is the art's mid blue so the overalls read; the blade has a lighter face with a highlight on its lit (left)
side. All in sheet coordinates of M; the joecool copy is moved by the art's own offset.
"""
import copy
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import common as C  # noqa: E402,F401
import kit as K  # noqa: E402
import el_bubbles as EB  # noqa: E402
import figs  # noqa: E402
import patches as P  # noqa: E402
from kit import U, circle, ellipse, poly, rect  # noqa: E402,F401

limb = figs.limb
ELS = ("tab-golfer-staff", "joecool-tab-staff")
_ORIG = {el: EB.BUILD[el] for el in ELS}

NAVY_CORE = [33.0, 41.0, 82.0]       # art pixels L < 55 inside the tab (median)
NAVY_EDGE = [49.0, 66.0, 107.0]      # art pixels 55 <= L < 80
SHIRT = [90.0, 107.0, 173.0]         # art pixels 95 < L < 125 (the chest between the straps)
BLADE = [70.0, 86.0, 140.0]
BLADE_HI = [128.0, 142.0, 200.0]


def seg(a, b, w):
    return {"t": "stroke", "pts": [list(a), list(b)], "w": w, "cap": "round"}


def staff_parts():
    """[(name, shape, paint key)] in the art's coordinates (M 0,450; pixel i covers [i, i+1)); paint keys: 'navy', 'shirt',
    'blade', 'blade_hi'. Drawn in this order."""
    head = ellipse(14.05, 456.85, 1.7, 1.75)
    cap = U(inter_top(ellipse(14.05, 456.6, 1.85, 1.6), 456.55),
            {"t": "rrect", "c": [12.15, 456.5], "half": [0.95, 0.38], "r": 0.35})       # brim to the viewer's left
    neck = rect(13.45, 458.2, 14.65, 459.9, 0.3)
    shirt = poly((11.2, 460.3), (12.6, 459.6), (15.5, 459.6), (16.9, 460.3), (17.5, 461.0), (16.9, 466.2), (12.2, 466.2),
                 (11.6, 461.0), smooth=1)
    bib = U(rect(12.75, 462.0, 15.45, 466.6, 0.35),
            seg((13.05, 462.3), (12.7, 460.1), 0.85), seg((15.15, 462.3), (15.6, 460.1), 0.85))
    hips = poly((12.15, 465.9), (16.85, 465.9), (17.05, 468.6), (11.95, 468.6), smooth=1)
    legs = U(limb([(12.95, 468.0, 1.95), (12.85, 471.0, 1.85), (12.75, 474.2, 1.75)]),
             limb([(15.95, 468.0, 1.95), (16.05, 471.0, 1.85), (16.15, 474.2, 1.75)]))
    feet = U(limb([(13.3, 475.15, 1.35), (11.9, 475.25, 1.4), (10.7, 475.3, 1.25)]),
             limb([(15.65, 475.15, 1.35), (17.05, 475.25, 1.4), (18.25, 475.3, 1.25)]))
    arm_l = limb([(11.55, 460.55, 1.45), (10.55, 461.9, 1.35), (9.75, 463.35, 1.3), (10.6, 464.7, 1.25),
                  (11.9, 465.85, 1.2)])
    arm_r = limb([(16.95, 460.55, 1.45), (17.8, 461.6, 1.35), (18.45, 462.75, 1.3), (19.7, 462.85, 1.2),
                  (20.55, 462.8, 1.15)])
    hand = ellipse(21.15, 462.8, 0.95, 0.8)
    shaft = seg((21.25, 458.9), (21.25, 471.2), 0.9)
    grip = U({"t": "outline", "of": {"t": "rrect", "c": [21.25, 457.85], "half": [1.05, 0.75], "r": 0.6}, "w": 0.7},
             rect(20.95, 458.4, 21.55, 459.3))
    socket = poly((20.75, 470.0), (21.75, 470.0), (22.25, 470.95), (20.25, 470.95))
    blade = poly((19.75, 470.75), (22.75, 470.75), (22.8, 473.8), (21.25, 475.65), (19.7, 473.8), smooth=1)
    blade_hi = poly((20.25, 471.25), (20.85, 471.25), (20.85, 474.5), (20.25, 473.75), smooth=1)
    return [("shaft", shaft, "navy"), ("grip", grip, "navy"), ("socket", socket, "navy"), ("blade", blade, "blade"),
            ("blade light", blade_hi, "blade_hi"),
            ("legs", legs, "navy"), ("feet", feet, "navy"), ("hips", hips, "navy"), ("shirt", shirt, "shirt"),
            ("bib", bib, "navy"), ("left arm", arm_l, "navy"), ("right arm", arm_r, "navy"), ("neck", neck, "navy"),
            ("head", head, "navy"), ("cap", cap, "navy"), ("hand", hand, "navy")]


def inter_top(s, y):
    """the part of s above y"""
    return {"t": "inter", "of": [s, {"t": "half", "p": [0.0, y], "n": [0.0, 1.0]}]}


def bbox(parts):
    """the exact box of the drawn group: the union's distance field sampled every 0.05 px over the cell"""
    import numpy as np
    import sdf
    yy, xx = np.mgrid[450:483:0.05, 0:33:0.05]
    d = sdf.sd(U(*[s for _, s, _ in parts]), xx, yy)
    ins = d < 0
    return float(xx[ins].min()), float(yy[ins].min()), float(xx[ins].max()), float(yy[ins].max())


def _body(scene):
    return [L for L in scene[1]["layers"] if L["shape"]["t"] == "circle"][-1]["shape"]


DISC = (16.5, 466.5)      # the tab's visible disc centre in its 33 x 33 cell (cell 0,450): rows 450-482, cols 0-32


def build(el):
    base = _ORIG[el](el)
    ref = "tab-golfer-staff"
    out = {}
    parts = staff_parts()
    x0, y0, x1, y1 = bbox(parts)
    gx = DISC[0] - (x0 + x1) / 2
    gy = 476.0 - y1              # feet / blade tip on the art's ground line (the bottom of row 475)
    print(f"{el}: group box x {x0:.2f}-{x1:.2f} y {y0:.2f}-{y1:.2f}, moved by ({gx:.2f}, {gy:.2f})")
    for st, key, rct in C.CELLS[el]:
        bg = P.strip_glyph(base[st])
        body = _body(bg)
        g = EB.GEOM[el][st]
        rg = EB.GEOM[ref]["idle"]
        dx, dy = (0, 0) if el == ref else K.match_offset((rg["key"], rg["body"]), (key, g["body"]))
        sc = copy.deepcopy(bg)
        lay = []
        for name, s, pk in parts:
            s = K.shift(s, gx + dx, gy + dy)
            if pk == "navy":
                lay.append({"name": name, "shape": s, "fill": NAVY_EDGE, "clip": body})
                lay.append({"name": name + " core", "shape": {"t": "grow", "of": s, "d": -0.3}, "fill": NAVY_CORE,
                            "clip": body})
            elif pk == "shirt":
                lay.append({"name": name, "shape": s, "fill": NAVY_EDGE, "clip": body})
                lay.append({"name": name + " cloth", "shape": {"t": "grow", "of": s, "d": -0.35}, "fill": SHIRT,
                            "clip": body})
            elif pk == "blade":
                lay.append({"name": name, "shape": s, "fill": NAVY_EDGE, "clip": body})
                lay.append({"name": name + " face", "shape": {"t": "grow", "of": s, "d": -0.35}, "fill": BLADE,
                            "clip": body})
            else:
                lay.append({"name": name, "shape": s, "fill": BLADE_HI, "clip": body, "soft": 0.3})
        sc[1]["layers"] += lay
        out[st] = sc
    return out


for _el in ELS:
    EB.BUILD[_el] = build
