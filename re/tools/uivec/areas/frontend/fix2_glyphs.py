"""P41 fix round: the editor's tan glyph bubbles (CGButtons.pcx idle column x 300, selected x 350, one cell per 50 rows).

Mariano's notes: body "more detail", hair "hair needs work", outfit "center it, make it look much more like clothes", pants
"needs to be centered", shirt "centered". Each glyph re-read from a five-level dump of its cell (gdump.py) and drawn from its
parts in cell-local coordinates; symmetric ones are one half mirrored about the axis read off the art. Every glyph is centred
on its bubble: the bounding box of the drawn glyph is moved onto the centre of the rim circle (the pants bubble's rim is fitted
with a hint now: its first fit merged the rim with the dark backdrop below and came out 1.7 px too big and 2.6 px low, which
is what pushed the trousers off centre). The glyph's paint is fitted to its own pixels per state (a linear paint: lighter blue
at the upper left, navy at the lower right), the way the art lights the cloth.
"""
import numpy as np

import common as c
import editor_screen as E
import fix2 as F
import parts
from common import fit, sdf

CB = "Interface/CGButtons.pcx"
BG_F = "Interface/CustGolfBckgrnd.pcx"


def mirror(pts, axis):
    """A closed outline from its left half (top centre ... bottom centre, going down the left side)."""
    right = [[2 * axis - x, y] for x, y in reversed(pts)]
    return pts + right


def P(pts, smooth=0):
    s = {"t": "poly", "rings": [[[float(x), float(y)] for x, y in pts]]}
    if smooth:
        s["smooth"] = smooth
    return s


def g_outfit():
    """A jumpsuit (one-piece work suit), symmetric about x 19.0: a V neckline between two collar points, the shoulders
    sloping into short sleeves that flare to a slanted cuff, a light seam where each sleeve meets the body (the art's light
    pixels at x 14 and 23, rows 12-13), the body cinched at the waist, then two trouser legs flaring to flat hems with rounded
    corners, the crotch an upside-down V."""
    half = [(19.0, 9.6), (17.7, 7.0), (13.7, 7.0), (12.3, 8.0), (10.8, 9.7), (9.9, 11.6), (10.0, 13.1), (10.9, 14.3),
            (13.3, 13.8), (14.4, 13.3), (15.25, 14.6), (15.25, 17.3), (14.3, 19.6), (13.3, 22.0), (10.45, 29.9),
            (10.9, 30.8), (12.0, 31.05), (14.1, 31.05), (15.1, 30.4), (18.25, 23.0), (19.0, 22.5)]
    body = P(mirror(half, 19.0), smooth=1)
    seams = F.U({"t": "stroke", "pts": [[14.45, 11.2], [14.05, 14.2]], "w": 0.85, "cap": "butt"},
                {"t": "stroke", "pts": [[23.55, 11.2], [23.95, 14.2]], "w": 0.85, "cap": "butt"})
    return F.sub(body, seams)


def g_shirt():
    """A T-shirt, symmetric about x 18.75: flat shoulders running straight out into short square sleeves (rows 11-15), a V
    neck notch 3 px wide at the top, the body hanging from under the sleeves and narrowing a little to a flat hem."""
    half = [(18.75, 14.3), (17.15, 11.0), (9.0, 11.0), (9.0, 16.0), (12.05, 16.0), (14.2, 28.0), (18.75, 28.0)]
    return P(mirror(half, 18.75))


def g_pants():
    """Trousers, symmetric about x 19.4: a straight waist on top, the hips falling straight for 5 rows, then two legs splaying
    out to flat hems along one gentle outer curve, the crotch an upside-down V opening at row 15."""
    half = [(19.4, 8.0), (14.4, 8.0), (14.25, 11.0), (13.8, 15.0), (12.95, 20.0), (11.05, 29.0), (16.25, 29.0), (19.4, 15.1)]
    return P(mirror(half, 19.4))


def g_hair():
    """A head of shoulder-length hair round a face: the crown as two rounded lobes parted in the middle (the valley between
    them is the parting), the hair falling straight down both sides to strands that end in rounded tips at shoulder height
    (row 26), a fringe across the forehead, a thin light gap between the hair and the face from the temples round the jaw;
    the face an oval, a short neck, the shoulders a flat oval."""
    ax = 19.1
    lobes = F.U(F.circ(15.4, 14.2, 4.4), F.circ(2 * ax - 15.4, 14.2, 4.4))
    left = [(11.0, 14.2), (16.2, 14.2), (16.2, 21.0), (13.85, 22.5), (13.7, 24.6), (13.1, 25.9), (12.1, 26.05),
            (11.35, 25.0), (11.0, 22.6)]
    sides = F.U(P(left, smooth=1), P([(2 * ax - x, y) for x, y in left], smooth=1))
    outer = F.U(lobes, sides)
    face = F.ell(19.4, 18.9, 4.5, 5.4)
    gap = {"t": "inter", "of": [{"t": "grow", "of": face, "d": 1.0}, {"t": "half", "p": [0, 14.1], "n": [0, -1]}]}
    hair = F.sub(outer, gap)
    neck = F.box(16.9, 25.0, 21.5, 27.4, 0.5)
    shoulders = F.ell(19.3, 28.7, 8.5, 2.25)
    return F.U(hair, face, neck, shoulders)


def g_body():
    """A figure for the body-type choice, half solid and half in outline as the art draws it: the head a thin ring with its
    left half filled; the torso a thin outline from the shoulders to the waist with its right half filled; the left arm solid,
    bending a little at the elbow, ending in a round hand; the right arm a thin tube in outline with a solid hand; the left leg
    solid, the right leg one thin line; the feet two bars on a faint floor line."""
    head = F.U({"t": "outline", "of": F.circ(19.0, 7.5, 2.85), "w": 1.1},
               {"t": "inter", "of": [F.circ(19.0, 7.5, 3.4), {"t": "half", "p": [19.15, 0], "n": [1, 0]}]})
    torso_line = {"t": "stroke", "pts": [[13.9, 12.45], [25.0, 12.45], [24.55, 20.75], [15.1, 20.75]], "w": 1.0,
                  "cap": "round", "closed": True}
    torso_fill = P([(19.0, 12.0), (25.45, 12.0), (24.45, 20.6), (19.0, 20.6)])
    arm_l = F.path([[[13.4, 12.9], [12.1, 17.0], [10.75, 20.9], [10.85, 22.3]]], 2.0)
    hand_l = F.circ(10.9, 22.45, 1.2)
    arm_r = {"t": "outline", "of": {"t": "stroke", "pts": [[25.7, 13.7], [27.75, 21.4]], "w": 2.5, "cap": "round"}, "w": 0.8}
    hand_r = F.circ(27.85, 22.35, 1.1)
    leg_l = P([(15.2, 20.6), (19.8, 20.6), (19.8, 31.2), (16.0, 31.2)])
    leg_r = F.path([[[23.35, 20.8], [23.2, 26.0], [22.75, 30.9]]], 1.1)
    feet = F.U(F.box(13.9, 30.9, 20.1, 32.4, 0.6), F.box(22.2, 30.9, 25.3, 32.4, 0.6))
    floor = {"t": "seg", "a": [14.2, 32.2], "b": [25.0, 32.2], "w": 0.7}
    return F.U(head, torso_line, torso_fill, arm_l, hand_l, arm_r, hand_r, leg_l, leg_r, feet, floor)


GLYPHS = {"outfit": (0, g_outfit), "shirt": (3, g_shirt), "pants": (4, g_pants), "hair": (5, g_hair), "body": (6, g_body)}
HINT = {"pants": (319.3, 219.6, 19.4)}           # (cx, cy, rmax) in the idle cell: keep the rim off the dark band below
WINDOWS = {"outfit": (420, 0, 500, 70), "shirt": (420, 150, 500, 220), "pants": (420, 200, 500, 270),
           "hair": (295, 20, 370, 100), "body": (295, 70, 370, 150)}


def centred(glyph, cx, cy, ox, oy):
    """glyph (cell-local) moved to the cell at (ox, oy) with its bounding box centred on (cx, cy)."""
    g = F.mv(glyph, ox, oy)
    b = F.bbox_of(g, ox, oy, ox + 42, oy + 42, s=8)
    dx, dy = cx - (b[0] + b[2]) / 2, cy - (b[1] + b[3]) / 2
    return F.mv(glyph, ox + dx, oy + dy), (round(dx, 2), round(dy, 2))


def bubble(art, cx, cy, r, glyph, rim_rgb=None):
    it = E.bubble(art, cx, cy, r, glyph)
    rim_i = [k for k, L in enumerate(it["layers"]) if L["shape"].get("t") == "circle" and L["shape"]["r"] == r][0]
    if rim_rgb is not None:
        it["layers"][rim_i]["fill"] = rim_rgb
    gl = it["layers"][-1]
    gl["fill"] = F.glyph_paint(art, glyph, it["box"], fallback=gl["fill"])
    return it


def main():
    cg, _ = c.sheet(CB)
    art, _ = c.sheet(BG_F)
    for name, (k, gf) in GLYPHS.items():
        if not F._only(f"editor-{name}"):
            continue
        y = 50 * k
        g_local = gf()
        st, info = [], {}
        rim_rgb = None
        for state, x0 in (("idle", 300), ("selected", 350)):
            hint = HINT.get(name)
            if hint:
                hint = (hint[0] + x0 - 300, hint[1], hint[2])
            cx, cy, r, res = E.rim_circle(cg, (x0, y, 42, 42), hint=hint)
            gl, d = centred(g_local, cx, cy, x0, y)
            it = bubble(cg, cx, cy, r, gl, rim_rgb)
            if rim_rgb is None:       # the idle rim's navy; the selected cell's ring median came out brown (yellow mixed in)
                rim_rgb = [L for L in it["layers"] if L["shape"].get("t") == "circle" and L["shape"]["r"] == r][0]["fill"]
            st.append({"name": state, "art": cg, "scene": [it], "rect": (x0, y, 50, 46)})
            info[state] = {"rim": [round(cx, 2), round(cy, 2), round(r, 2)], "glyph_shift": d}
        v, dx, dy = E.locate(cg, (300, y, 42, 42), art, WINDOWS[name])
        cx, cy, r = info["idle"]["rim"]
        gl, d = centred(g_local, cx + dx, cy + dy, 300 + dx, y + dy)
        bit = bubble(art, cx + dx, cy + dy, r, gl, rim_rgb)
        st.append({"name": "on the backdrop", "art": art, "scene": [bit], "rect": (300 + dx, y + dy, 46, 46)})
        info["backdrop"] = {"at": [300 + dx, y + dy], "overlap": round(v, 3)}
        how = (f"tan glass bubble (rim circle fitted, paint measured per state) with {gf.__doc__.split('.')[0][0].lower()}"
               f"{gf.__doc__.split('.')[0][1:]}; the glyph's bounding box centred on the rim's centre; glyph paint a linear "
               f"fit to its own pixels per state; third row: where the backdrop draws it ({300 + dx}, {y + dy})")
        F.deliver(f"editor-{name}", st, how, CHANGED[name], z=8, extra={"info": info})
        BACKDROP_ITEMS[name] = bit


CHANGED = {
    "outfit": "centred on the bubble; redrawn as a garment: collar points round a V neck, sloping shoulders into flared "
              "short sleeves set off by seams, a cinched waist, trouser legs with rounded hems, soft (corner-cut) outline, "
              "cloth shading lighter at the upper left",
    "shirt": "glyph bounding box centred on the rim centre (and the selected rim back to navy); shirt redrawn mirrored about "
             "its axis",
    "pants": "rim refitted with a hint (the old fit merged with the dark band below and drew a bigger, lower bubble), "
             "trousers mirrored about their axis and centred on it",
    "hair": "hair redrawn: two crown lobes with the parting between them, a fringe joined to the face, the light gap only "
            "from the temples round the jaw, side strands down to rounded tips at shoulder height (they stopped at the "
            "chin before), lit lighter at the upper left; centred",
    "body": "more detail: head as a ring with its left half filled, a thin torso outline with the right half filled, the "
            "left arm bending at the elbow with a round hand, the right arm a thin outlined tube with its hand, the left leg "
            "solid and the right a thin line, two feet on the faint floor line; shading fitted on its pixels",
}
BACKDROP_ITEMS = {}


if __name__ == "__main__":
    main()
