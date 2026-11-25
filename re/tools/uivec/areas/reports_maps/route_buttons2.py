"""P41 fix round (2026-11-24): route-course-button and route-staff-button (both redo), the glyphs re-read from the dark-coverage
dumps of route screens_bottom.pcx (idle cells; the selected ones are the same glyph moved). The bubble and its paints are
round one's (approved on the aura / value buttons, route_buttons.py).

COURSE (bubble centre (303.94, 40.5)):
  - the POLE, a lit cylinder: dark x 300.9..304.6, y 18.4..60, a light core line at x 302 the whole height; a FOOT under it
    (x 298.8..307.3, y 59.8..62.6, rounded ends);
  - the PENNANT from the pole top: top edge curving from (304.4, 18.8) out to the tip (325.4, 27.4), bottom edge back to
    (306.4, 31.4); a 1 px light slit between it and the pole (x 305, rows 22..27);
  - at the left two strokes meeting in a point at (284.6, 41.8): a thin one (2.2..3 px) curving up to a HOOK that turns right
    at (295.8, 25.9), and a tapered one (2 -> 4 px) rising to the pole;
  - that tapered stroke goes on as ONE ARC behind the pole (a 1 px light gap at the pole's right side, rows 34..35) and down
    to the lower right, 4 px wide, ending at (321, 47.5) in a pointed flat end on top of
  - a BLOCK (x 307.5..318.5, y 48..59, its lower right slanting out to 320.6) with a HORN at its top left (x 307.6..310.2
    from y 47.3) and a light V notch between the horn and the arc's end.
STAFF (bubble centre (42.21, 41.77)), a worker in overalls with a spade:
  - CAP: crown x 37.8..46 (its back rounded down to the right), top y 17.3; a bill to the left (x 33.8..38, y 20.3..22.3);
  - HEAD: x 37.2..45.6 under the cap, tapering to the chin at y 28.6; a short neck;
  - OVERALLS: two straps (x 38..40.6 and 44..46.6) from the shoulders to the bib's top (y 34.6), the light shirt between,
    the bib and trousers (x 37.6..47 at the chest, narrower at the waist y 44, wider at the hips y 47..48);
  - LEFT ARM akimbo: upper arm from the shoulder (35.6, 30.2) out to the elbow (29.6, 36.2), forearm back to the hand on the
    hip (34.6, 41), the light loop inside; RIGHT ARM from the shoulder (48.8, 31) down to a HAND gripping the handle
    (x 54.6..59.8, y 36..41.6); a 1 px light line between each arm and the overalls;
  - LEGS with a light gap between (x 40.5..42.5 from y 49 to the ground), knees narrower, BOOTS pointing out (left toe to
    x 31, right toe to x 52.6);
  - GROUND: a slab y 61.6..64.4, x 27.4..57.8, its lower ends slanting in;
  - SPADE: a handle x 56..58.4 from the blade down to y 59.8 (round end), a pointed BLADE (tip (57.8, 21.8), x 53.2..61.8 at
    its widest, bottom y 34) with a light crease near its left edge (from (57.3, 24.2) curving to (55.2, 31.6)).
All strokes are ONE path each with round joins (sdf "stroke" with round caps, corner-cut "smooth") or "qstroke" curves.
"""
import json
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import common as C          # noqa: E402
import deliver2 as D2       # noqa: E402
import fit                  # noqa: E402
import glyphs               # noqa: E402
import route_buttons as RB  # noqa: E402


def poly(*p, smooth=0):
    S = {"t": "poly", "rings": [[list(q) for q in p]]}
    if smooth:
        S["smooth"] = smooth
    return S


def pline(*p, w=3.0, smooth=0):
    S = {"t": "stroke", "pts": [list(q) for q in p], "w": w, "cap": "round"}
    if smooth:
        S["smooth"] = smooth
    return S


def U(*s):
    return {"t": "union", "of": list(s)}


def sub(a, b):
    return {"t": "sub", "a": a, "b": b}


def flag():
    pole = RB.R_(300.9, 18.4, 304.6, 60.2, 0.8)
    foot = RB.R_(298.8, 59.8, 307.3, 62.6, 1.4)
    pennant = poly((304.4, 18.6), (306.8, 19.2), (310.6, 20.4), (315.4, 21.2), (319.4, 22.0), (322.4, 23.6), (324.4, 25.6),
                   (325.4, 27.4), (321.0, 27.8), (317.0, 28.0), (313.4, 29.2), (309.8, 30.4), (306.4, 31.4), (304.4, 31.4), smooth=1)
    hook = {"t": "qstroke", "p": [[284.5, 42.4], [283.4, 27.2], [295.4, 26.0]], "w": [1.0, 2.3]}
    hook_end = {"t": "circle", "c": [295.6, 25.9], "r": 1.35}
    # the arc: a tapered stroke from the same sharp point up to the pole, then (behind the pole) on down to the block; two
    # quadratics meeting under the pole, read from the stroke's middle on rows 34..48
    arc = U({"t": "qstroke", "p": [[284.5, 42.4], [291.45, 34.55], [300.8, 34.3]], "w": [1.0, 3.6]},
            {"t": "qstroke", "p": [[300.5, 34.3], [316.25, 37.95], [320.6, 49.8]], "w": [3.6, 5.0]})
    arc_end = {"t": "circle", "c": [300.6, 34.3], "r": 1.8}
    block = poly((307.5, 48.2), (314.0, 48.2), (321.8, 48.6), (323.6, 50.4), (322.6, 51.8), (318.6, 52.6), (319.2, 55.0),
                 (320.8, 58.9), (307.5, 58.9))
    horn = RB.R_(307.6, 47.3, 310.2, 50.0, 0.8)
    gap = RB.R_(299.9, 31.0, 305.5, 38.6, 0.6)            # the light gap round the pole where the arc passes behind it
    core = RB.R_(301.7, 19.6, 302.6, 60.2, 0.4)           # the pole's light core line
    slit = RB.R_(304.75, 21.8, 305.6, 27.8, 0.3)          # the light slit between the pennant and the pole
    notch = poly((310.6, 47.0), (313.6, 47.0), (312.2, 49.6))
    arc_cut = sub(U(arc, arc_end), sub(gap, RB.R_(299.0, 30.0, 300.4, 40.0)))   # behind the pole, the left side joins it
    return sub(U(pole, foot, pennant, hook, hook_end, arc_cut, block, horn), U(core, slit, notch))


def staff():
    crown = poly((37.8, 21.6), (37.8, 18.4), (38.6, 17.3), (42.6, 17.3), (44.4, 17.9), (45.6, 19.0), (46.1, 20.4),
                 (46.1, 21.6), smooth=1)
    bill = poly((33.8, 20.4), (38.4, 20.2), (38.4, 22.3), (34.6, 22.3), (33.8, 21.6), smooth=1)
    head = poly((37.2, 20.8), (45.8, 20.8), (45.6, 25.4), (44.8, 27.2), (43.6, 28.6), (40.0, 28.6), (38.4, 27.4), (37.2, 25.2),
                smooth=1)
    neck = RB.R_(40.3, 28.0, 43.0, 29.8, 0.4)
    straps = U(RB.R_(37.9, 29.3, 40.6, 35.0, 0.5), RB.R_(44.0, 28.6, 46.6, 35.0, 0.5))
    body = poly((37.6, 34.5), (46.9, 34.5), (47.0, 38.8), (47.6, 40.0), (47.6, 43.4), (47.3, 44.3), (48.0, 45.2), (48.8, 46.2),
                (49.8, 47.4), (49.8, 48.6), (33.2, 48.6), (33.4, 47.2), (34.3, 46.2), (35.2, 45.0), (35.2, 44.0), (34.3, 42.8),
                (34.6, 40.6), (36.4, 39.4), (37.6, 38.6))
    l_arm = pline((35.8, 30.3), (29.9, 36.2), (34.8, 41.4), w=3.5)
    r_arm = pline((49.0, 31.0), (51.6, 35.8), (55.0, 38.6), w=3.4)
    hand = RB.R_(54.8, 36.0, 60.2, 41.6, 1.6)          # the fist round the handle (rows 36..41, to x 60)
    l_leg = poly((33.4, 48.2), (40.4, 48.2), (40.0, 52.0), (39.6, 55.0), (40.4, 56.4), (40.6, 59.4), (41.0, 61.8), (31.0, 61.8),
                 (33.2, 59.0), (35.0, 57.2), (35.2, 54.4), (34.6, 52.0), smooth=1)
    r_leg = poly((42.6, 48.2), (48.8, 48.2), (48.4, 52.6), (47.6, 55.0), (47.6, 58.6), (49.4, 59.6), (51.2, 60.6), (52.8, 61.8),
                 (41.8, 61.8), (42.4, 58.0), (42.8, 54.6), smooth=1)
    ground = poly((27.4, 61.7), (57.8, 61.7), (57.8, 62.8), (55.2, 64.4), (31.4, 64.4), (27.6, 62.6), smooth=1)
    handle = RB.R_(56.0, 33.0, 58.4, 59.8, 1.2)
    blade = poly((57.6, 21.8), (58.4, 22.2), (60.0, 24.2), (61.2, 26.4), (61.8, 28.2), (61.8, 32.6), (60.6, 33.6), (58.6, 34.0),
                 (56.0, 34.0), (54.4, 33.4), (53.2, 32.4), (53.2, 28.2), (54.0, 25.6), (55.6, 23.2), (56.8, 22.0), smooth=1)
    crease = {"t": "qstroke", "p": [[57.4, 24.0], [55.4, 26.4], [55.2, 31.6]], "w": [0.9, 0.8]}
    gaps = U(RB.R_(40.6, 29.0, 44.0, 34.5, 0.3),                         # the shirt between the straps
             RB.R_(40.5, 49.0, 42.5, 61.6, 0.4),                         # between the legs
             poly((46.8, 30.0), (47.8, 30.0), (47.8, 38.4), (47.0, 38.4)),   # between the right arm and the overalls
             pline((36.9, 30.2), (36.9, 33.0), w=0.9))                   # between the left arm and the strap
    return sub(U(crown, bill, head, neck, straps, body, l_arm, r_arm, hand, l_leg, r_leg, ground, handle, blade),
               U(gaps, crease))


G2 = {"course": flag, "staff": staff}
WHAT = {"course": "the course glyph from its parts: a lit pole (dark, a light core line the whole height) on a rounded foot; a pennant "
                  "from its top curving out to a pointed tip, with the light slit by the pole; at the left a thin stroke curving up "
                  "to a hook that turns right and a tapered stroke meeting it in a point; that stroke goes on as ONE arc (2 -> 4 px) "
                  "behind the pole (light gap at the pole) down to the lower right, ending in a pointed flat end on a block with a "
                  "horn at its top left (a light V notch between them)",
        "staff": "the worker from his parts: cap (crown rounded at the back, bill to the left), head tapering to the chin, neck; "
                 "overalls (two straps, the light shirt between, bib and trousers narrowing at the waist); the left arm akimbo as "
                 "one path shoulder-elbow-hip with the open loop inside, the right arm down to a hand gripping the spade; legs with "
                 "the light gap, knees, boots pointing out; the ground slab; the spade's handle and pointed blade with its light "
                 "crease; 1 px light lines between each arm and the overalls"}
CHANGED = {"course": "redone from a new reading: the left mark is a hooked thin stroke plus a tapered stroke meeting in a point "
                     "(was one arc and a separate straight bar), the tapered stroke continues as ONE smooth arc behind the pole to "
                     "the lower right (was a straight bar from the pole, disconnected from the left), the block now has the art's "
                     "horn, notch and slanted foot, the pennant curves to a drawn-out tip with the slit by the pole, the pole is lit "
                     "with its core line the whole height on a rounded foot",
           "staff": "redone from a new reading, no stick parts: the cap has a rounded crown and a bill, the face tapers to a chin, "
                    "the overalls have straps with the shirt between and a waist, the left arm is one bent path (shoulder, elbow, "
                    "hand on hip) with its open loop, the right arm ends in a hand around the handle, the legs have knees and boots "
                    "pointing out, the blade is the art's pointed spade with its crease, the ground's ends slant like the art's"}


def tune(name, g):
    rgb, m = C.sheet(RB.RB)
    cx, cy, R = RB.CENTRES[(name, "idle")]
    box = (int(cx - 27), int(cy - 27), int(cx + 28), int(cy + 28))
    jj, ii = np.mgrid[box[1]:box[3], box[0]:box[2]]
    inside = np.hypot(ii + 0.5 - cx, jj + 0.5 - cy) < 25.5
    cov, lo, hi = fit.dark_coverage(rgb[box[1]:box[3], box[0]:box[2]], inside)
    s0 = glyphs.shape_coverage(g, box, inside, s=3)
    i0 = float(np.minimum(cov, s0).sum() / max(np.maximum(cov, s0).sum(), 1e-9))
    if "--tune" in sys.argv:
        glyphs.fit_shape(g, cov, inside, box, passes=3, reach=0.8)
    s1 = glyphs.shape_coverage(g, box, inside, s=3)
    i1 = float(np.minimum(cov, s1).sum() / max(np.maximum(cov, s1).sum(), 1e-9))
    return g, i0, i1


def glyph_tile(name, g, z=12):
    """The glyph alone at z x beside the art's: navy on the body colour, for reading the parts."""
    import deliver as D
    rgb, m = C.sheet(RB.RB)
    cx, cy, R = RB.CENTRES[(name, "idle")]
    x, y = int(cx - 27), int(cy - 27)
    art = rgb[y:y + 55, x:x + 55]
    st = json.load(open(RB.STYLE_FILE))
    base = C.wipe(art, RB.circ((cx, cy), R + 1.5), origin=(x, y))
    return D.tile(art, base, [{"name": "bubble", "box": [cx - 37, cy - 37, cx + 37, cy + 37],
                               "layers": RB.layers((cx, cy, R), st, "blue", g)}], (x, y), z)


def run(name):
    st = json.load(open(RB.STYLE_FILE))
    g, i0, i1 = tune(name, G2[name]())
    print(name, "iou", round(i0, 3), "->", round(i1, 3), flush=True)
    RB.GLYPHS[name] = lambda g=g: g
    rgb, m = C.sheet(RB.RB)
    states = []
    for state, colour in (("idle", "blue"), ("selected", "yellow")):
        x, y, w, h = RB.CELLS[name]
        if state == "selected":
            y += RB.SEL_DY[name]
        art = rgb[y:y + h, x:x + w].copy()
        mk = m[y:y + h, x:x + w]
        art[~mk] = [70, 110, 90]
        c = RB.CENTRES[(name, state)]
        base = C.wipe(art, RB.circ(c[:2], c[2] + 1.5), origin=(x, y))
        base[~mk] = [70, 110, 90]
        states.append((state, art, base, [{"name": "bubble", "box": [c[0] - 37, c[1] - 37, c[0] + 37, c[1] + 37],
                                            "layers": RB.layers(c, st, colour, RB.glyph(name, state))}], (x, y)))
    D2.deliver(f"route-{name}-button", states,
               "the ringed glass bubble as approved on the aura / value buttons (navy rim r 35.4, lit glass band, thin navy inner "
               "ring r 27.6, lit body; paints solved per colour over the four buttons) with " + WHAT[name]
               + ". Idle and selected at 3x, then the glyph at 12x (shapes as read, no numeric tuning)",
               CHANGED[name],
               unsure=("what the hooked stroke, the arc and the block depict is not certain (a ball's flight to a green, or a route "
                       "round the pin); drawn as read" if name == "course" else ""),
               z=3, extra_tiles=[glyph_tile(name, g)])


if __name__ == "__main__":
    for n in [a for a in sys.argv[1:] if not a.startswith("--")] or ["course", "staff"]:
        run(n)
