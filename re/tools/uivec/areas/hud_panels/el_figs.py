"""P41 hud_panels fix round (2026-11-24): the blue tab figures redrawn with volume.

Mariano's verdict on round 1: "too simplified", stick figures. Each figure below was re-read at 16-20x (common.py zoom) and
from luminance dumps, every part listed, then drawn with figs.limb (a limb whose width is set at each point, round joins)
so a torso tapers from the shoulders to the hips, a thigh is thicker than its shin, a foot has a heel and a toe, and the
club runs unbroken from the hands to its head. Coordinates are sheet pixels of the reference cell (pixel i covers
[i, i+1)); the other copies are moved by the art's own whole-pixel offset (kit.match_offset).
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import kit as K  # noqa: E402
import el_blue  # noqa: E402
import figs  # noqa: E402
from kit import circle, ellipse, rect, poly, sub  # noqa: E402,F401

limb = figs.limb


def glyph_golfer():
    """Golfer in the follow-through (M 0,400; same drawing on J 100,400 and E 0,450, mean difference 2.2-2.6 levels).
    Club: its head a short blade hooked down at the left end (pixels (8,409), (8,410), (7,410)); the shaft level along row
    408 from x 9 to the hands at x 16 (the art's one light pixel at x 15 is antialiasing of the hands; drawn unbroken).
    Head: 2 x 3 block x 17-18, rows 407-409, a chin at row 410. Front arm: shoulder (18.5, 411.5) along row 411 to the hand
    up at (21, 410). Torso: x 17-18 leaning from x 18.6 at row 412 to 17.6 at row 419, shoulders wider than the waist.
    Hips: x 15-18, rows 420-421. Back leg: thigh down-left to the knee (11.8, 423.4), shin back down to the toe (12.4,
    425.5). Front leg: x 17-18 straight down rows 421-425, the foot along row 425 to x 20 (toe at x 21)."""
    return [
        ("club head", limb([(9.9, 408.6, 1.15), (9.0, 409.4, 1.3), (8.2, 410.3, 1.35), (7.8, 410.8, 1.2)], cap="butt"), None),
        ("shaft", limb([(9.9, 408.55, 1.0), (16.6, 408.55, 1.1)]), None),
        ("hands", circle(16.7, 408.7, 0.85), None),
        ("head", ellipse(17.85, 408.3, 1.15, 1.45), None),
        ("neck", limb([(18.0, 409.6, 1.5), (18.4, 411.0, 1.7)]), None),
        ("arm", limb([(18.6, 411.5, 1.35), (20.2, 411.4, 1.2), (21.4, 410.4, 1.05)]), None),
        ("torso", limb([(18.8, 411.8, 2.4), (18.7, 414.5, 2.3), (18.3, 417.0, 2.2), (17.6, 419.6, 2.2)]), None),
        ("hips", limb([(15.8, 420.9, 1.6), (17.4, 420.6, 2.1), (18.3, 421.0, 1.5)]), None),
        ("back leg", limb([(16.2, 420.9, 1.65), (13.5, 422.4, 1.45), (11.6, 423.6, 1.3), (12.4, 425.4, 1.2)]), None),
        ("front leg", limb([(17.8, 421.0, 1.75), (17.9, 423.5, 1.55), (17.8, 425.0, 1.4)]), None),
        ("front foot", limb([(17.4, 425.55, 1.15), (19.6, 425.55, 1.05), (20.9, 425.4, 0.8)]), None),
    ]


def glyph_staff():
    """A man beside an upright post (M 0,450; same drawing on J 0,450). Read from the luminance dump, pixel by pixel.
    Head: the dark block at the top, x 13-15: row 455 x 13-15, row 456 x 12-15, row 457 x 13-15, row 458 x 14 (a round
    crown, a chin pointing down), then a light gap (row 459).
    Shirt: rows 460-463 are three dark vertical lines with the lit ground between them (x 13, x 15, x 17; x 14 and x 16
    at 145): the shirt's two edges and its middle seam. The x 17 line turns at row 461 into the right arm, which runs
    down-right ((18, 461), (18-19, 462)) to the post at (20-21, 462-463).
    Left arm: a '<' from the raised hand (10, 460) out to the elbow (9, 463) and back in to the waist (12, 466).
    Lower torso: a solid block rows 464-467 (x 12-16 at 464, x 11-16 at 465, x 12-16 at 466-467).
    Legs: x 12-13 and x 15-16 rows 468-473, the crotch gap x 14 from row 468; feet turned out on rows 474-475 (left x
    10-12, right x 16-18).
    Post: x 21 from row 460 to 472 (about three quarters dark), a round knob on top (x 20-22, rows 457-459, its corners
    lighter)."""
    return [
        ("post", limb([(21.5, 459.5, 1.0), (21.5, 472.95, 1.0)], cap="butt"), None),
        ("knob", circle(21.5, 458.4, 1.5), None),
        ("head", poly((12.9, 456.1), (13.6, 455.05), (15.4, 455.0), (16.2, 455.9), (16.0, 457.3), (15.1, 458.95),
                      (14.4, 458.95), (13.2, 457.4), smooth=2), None),
        ("shirt left", limb([(13.5, 460.0, 1.05), (13.5, 464.2, 1.1)], cap="butt"), None),
        ("shirt seam", limb([(15.5, 460.0, 1.05), (15.5, 464.2, 1.1)], cap="butt"), None),
        ("right arm", limb([(17.5, 460.0, 1.05), (17.6, 460.9, 1.1), (18.6, 461.6, 1.2), (19.8, 462.4, 1.25),
                            (21.0, 462.9, 1.2)], cap="butt"), None),
        ("left arm", limb([(10.6, 460.4, 1.15), (10.3, 461.7, 1.2), (9.5, 463.4, 1.3), (10.5, 464.6, 1.25),
                           (11.6, 465.6, 1.3), (12.4, 466.3, 1.3)]), None),
        ("lower torso", poly((12.1, 463.9), (16.9, 463.9), (16.95, 468.3), (12.2, 468.3), (11.2, 465.6), (11.3, 464.9),
                             smooth=1), None),
        ("left leg", limb([(13.0, 467.8, 2.0), (13.0, 471.0, 1.85), (12.9, 473.9, 1.75)]), None),
        ("right leg", limb([(15.95, 467.8, 2.0), (16.0, 471.0, 1.85), (16.2, 473.9, 1.7)]), None),
        ("left foot", limb([(12.9, 474.8, 1.55), (11.0, 474.9, 1.5), (10.2, 474.7, 1.2)]), None),
        ("right foot", limb([(16.2, 474.8, 1.5), (17.9, 474.9, 1.4), (18.5, 474.8, 1.0)]), None),
    ]


def akimbo(head, hw, sh_y, sh_x0, sh_x1, elbows, hips_y, legs, foot_y, feet_out=0.9, top=None):
    """A standing figure with both hands on its hips: head (cx, cy, rx, ry), shoulders on sh_y from sh_x0 to sh_x1, each
    arm out to its elbow and back in to the waist at hips_y, a torso hw wide, two legs (x, x) down to foot_y, feet turned
    out by feet_out."""
    hx, hy, rx, ry = head
    cx = (sh_x0 + sh_x1) / 2
    (ex0, ey0), (ex1, ey1) = elbows
    return [
        ("head", ellipse(hx, hy, rx, ry)),
        ("neck", limb([(hx, hy + ry * 0.6, 1.2), (cx, sh_y, 1.3)])),
        ("shoulders", limb([(sh_x0, sh_y + 0.45, 1.45), (sh_x1, sh_y + 0.45, 1.45)])),
        ("torso", limb([(cx, sh_y + 0.4, hw + 0.4), (cx, hips_y - 0.6, hw)])),
        ("left arm", limb([(sh_x0, sh_y + 0.3, 1.35), (ex0, ey0, 1.25), (cx - hw / 2 - 0.3, hips_y - 0.5, 1.2)])),
        ("right arm", limb([(sh_x1, sh_y + 0.3, 1.35), (ex1, ey1, 1.25), (cx + hw / 2 + 0.3, hips_y - 0.5, 1.2)])),
        ("hips", limb([(legs[0], hips_y, 1.5), (legs[1], hips_y, 1.5)])),
        ("left leg", limb([(legs[0], hips_y, 1.45), (legs[0], foot_y, 1.3)])),
        ("right leg", limb([(legs[1], hips_y, 1.45), (legs[1], foot_y, 1.3)])),
        ("left foot", limb([(legs[0], foot_y + 0.1, 1.2), (legs[0] - feet_out, foot_y + 0.2, 1.0)])),
        ("right foot", limb([(legs[1], foot_y + 0.1, 1.2), (legs[1] + feet_out, foot_y + 0.2, 1.0)])),
    ]


def glyph_group():
    """Three figures, all standing with their hands on their hips (J 0,400; same drawing on E 0,400). Round 1 read the
    middle one as jumping with its arms up: the dump shows elbows out (12-13, 408) and (19, 408) and hands back in at row
    410, the same pose as the other two, standing higher (further back) between them.
    Middle: head x 15-16, rows 404-406; shoulders row 407 x 13-18; torso x 15-16 to row 411; legs x 15 and x 17 down to
    row 416-417. Left: head (10.5, 410.6); shoulders rows 413-414 x 7-12; elbows (7, 416.5) and (13, 416); torso x 9-11;
    legs x 9 and x 11 rows 419-423, feet on row 423. Right: head (22.6, 410.6); shoulders x 20-25; elbows (18.6, 416.4)
    and (26, 416.2); torso x 22-23; legs x 21 and x 23 to row 423."""
    P = []
    for tag, parts in (
            ("middle", akimbo((15.9, 405.3, 1.05, 1.45), 1.7, 407.2, 13.6, 18.4, ((12.6, 408.6), (19.3, 408.5)), 411.6,
                              (15.5, 17.5), 416.6, feet_out=0.5)),
            ("left", akimbo((10.6, 411.0, 1.1, 1.05), 2.4, 413.9, 7.9, 12.3, ((7.3, 416.3), (13.6, 416.1)), 418.9,
                            (9.5, 11.6), 423.3)),
            ("right", akimbo((22.6, 411.0, 1.1, 1.05), 2.0, 413.9, 20.6, 24.8, ((18.9, 416.4), (26.2, 416.3)), 418.9,
                             (21.5, 23.6), 423.3))):
        for n, s in parts:
            P.append((f"{tag} {n}", s, None))
    return P


el_blue.REF.update({
    "tab-golfer": (glyph_golfer, "tab-golfer"), "employee-tab-golfer": (glyph_golfer, "tab-golfer"),
    "tab-golfer-swing": (glyph_golfer, "tab-golfer"),
    "tab-golfer-staff": (glyph_staff, "tab-golfer-staff"), "joecool-tab-staff": (glyph_staff, "tab-golfer-staff"),
    "tab-golfers-group": (glyph_group, "tab-golfers-group"), "tab-staff-group": (glyph_group, "tab-golfers-group"),
})


# ------------------------------------------------------------------------------------------------------------ lit rim
def rim_paint(key, c, r_in, d=(0.7071, 0.7071), n=5):
    """The rim's colour lit across the bubble: the art's rim is one pixel of navy that is lighter at the upper left
    (luminance 55-95) and nearly black at the lower right (23), measured on rays from the rim centre (2026-11-24). Returns a
    linear paint along d (upper left -> lower right) with n stops, each the median of the ring pixels in its band."""
    import numpy as np
    rgb, m = C.sheet(key)
    cx, cy, r = c[:3]
    x0, y0, x1, y1 = int(cx - r) - 1, int(cy - r) - 1, int(cx + r) + 2, int(cy + r) + 2
    jj, ii = np.mgrid[y0:y1, x0:x1]
    x, y = ii + 0.5, jj + 0.5
    rr = np.hypot(x - cx, y - cy)
    ring = (rr > r_in + 0.15) & (rr < r - 0.15) & m[y0:y1, x0:x1]
    t = ((x - cx) * d[0] + (y - cy) * d[1]) / r          # -1 .. 1
    a = rgb[y0:y1, x0:x1]
    L = a.mean(axis=2)
    stops = []
    for k in range(n):
        lo, hi = -1 + 2 * k / n, -1 + 2 * (k + 1) / n
        sel = ring & (t >= lo) & (t < hi) & (L < 140)
        if sel.sum() >= 2:
            stops.append([round((k + 0.5) / n, 3), [round(float(v), 1) for v in np.median(a[sel], axis=0)]])
    return {"linear": {"a": [cx - d[0] * r, cy - d[1] * r], "b": [cx + d[0] * r, cy + d[1] * r], "stops": stops}}


RIM_W = 1.15      # the blue tab's rim is one pixel wide (profiles at 0, 90, 180 deg: one dark sample pair, r 13.5-14.5)


_BLUE_SHADE = {}


def _geom(el, st, key):
    import el_bubbles as EB
    g = EB.GEOM[el][st]
    c, body0 = el_blue.measure_blue(key, [r for s_, k_, r in C.CELLS[el] if s_ == st][0])
    body_r = c[2] - RIM_W
    body = [c[0] + (body0[0] - c[0]) * 0.5, c[1] + (body0[1] - c[1]) * 0.5, body_r]
    return c, body


def blue_shade():
    """The blue body's lit surface re-measured on the corrected body circle (rim one pixel wide) over every blue tab cell
    at once (bubble.solve_family: bevel least squares, glyph pixels left out)."""
    import bubble as B
    if "coef" not in _BLUE_SHADE:
        rows = []
        for el, (gl, ref) in el_blue.REF.items():
            if el != ref:
                continue
            for st, key, rct in C.CELLS[el]:
                c, body = _geom(el, st, key)
                rows.append(B.body_rows(key, c, body, trim_glyph=K.U(*[s for _, s, _ in gl()])))
        coef = B.solve_family(rows)
        # the deepest stop (the body's centre) has no edge to face: its normal terms are dropped, so the centre does not
        # pinch into a point where the nearest-edge normal turns round
        coef[-1] = [coef[-1][0], [0, 0, 0], [0, 0, 0], [0, 0, 0], [0, 0, 0]]
        _BLUE_SHADE["coef"] = coef
    return _BLUE_SHADE["coef"]


def _blue_build(el, _orig=el_blue.build):
    import bubble as B
    scenes = _orig(el)
    coef = blue_shade()
    for st, key, rct in C.CELLS[el]:
        c, body = _geom(el, st, key)
        it = scenes[st][1]
        rim, bl = it["layers"][0], it["layers"][1]
        rim["fill"] = rim_paint(key, c, body[2])
        bl["shape"] = {"t": "circle", "c": body[:2], "r": body[2]}
        bl["fill"] = {"bevel": {"L": body[2], "stops": B.STOPS, "coef": coef}}
        for L in it["layers"][2:]:
            L["clip"] = bl["shape"]
    return scenes


import common as C  # noqa: E402
import el_bubbles as _EB  # noqa: E402
for _k in el_blue.REF:
    _EB.BUILD[_k] = _blue_build
