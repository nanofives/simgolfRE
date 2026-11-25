"""uivec: the title menu's glyphs built from what they ARE (P41).

Each glyph is first READ (glyphdump.py prints it on its pixel grid) and sorted:
  simple   made of a few recognisable parts: a line, an arrow, an outlined square, a door leaf, a folder flap.
           It is written below as those parts; only their numbers are then fitted to the art.
  complex  a free silhouette (a trophy, a golf ball on a tee in grass). It keeps its measured outline: breaking it into
           boxes and circles describes nothing.
Usage: py -3.12 re/tools/uivec/glyphs.py      fits the simple glyphs, writes glyphs_title.json and glyphs_title.png
"""
import copy
import json
import os
import sys

import numpy as np
from PIL import Image

sys.path.insert(0, os.path.dirname(__file__))
import fit      # noqa: E402
import sdf      # noqa: E402
import title    # noqa: E402


def U(*s):
    return {"t": "union", "of": list(s)}


def line(*pts, w=1.0, cap="butt"):
    return {"t": "stroke", "pts": [list(p) for p in pts], "w": w, "cap": cap}


def poly(*pts, smooth=0):
    s = {"t": "poly", "rings": [[list(p) for p in pts]]}
    if smooth:
        s["smooth"] = smooth
    return s


def square(x0, y0, x1, y1, r=2.0):
    return {"t": "rrect", "c": [(x0 + x1) / 2, (y0 + y1) / 2], "half": [(x1 - x0) / 2, (y1 - y0) / 2], "r": r, "rot": 0.0}


def outlined(s, w):
    return {"t": "outline", "of": s, "w": w}


def behind(s, *front, margin=1.1):
    """s where the shapes in front of it (their whole area, a little grown) do not hide it."""
    return {"t": "sub", "a": s, "b": U(*[{"t": "grow", "of": f, "d": margin} for f in front])}


# ---------------------------------------------------------------------------------------------------------- the glyphs
# name -> (class, [(what the part is, shape), ...]); a part named "cut: ..." is taken out of the parts before it.
def authored():
    s1, s2, s3 = square(712.9, 377.7, 725.0, 389.3), square(720.6, 384.5, 731.9, 396.3), square(728.1, 391.9, 740.5, 403.4)
    return {
        "exit": ("simple", [
            ("door frame, lower part: floor line and jamb", line((736.3, 567.5), (746.5, 567.5), (746.5, 562.8))),
            ("door frame, upper part: jamb and lintel", line((746.5, 552.2), (746.5, 546.5), (759.5, 546.5))),
            ("floor line right of the door", line((757.0, 567.5), (764.0, 567.5))),
            ("door leaf, open (its foot slants)", poly((751.7, 546.0), (759.8, 546.0), (759.8, 567.8), (757.2, 567.8),
                                                      (751.7, 562.2))),
            ("cut: keyhole", {"t": "circle", "c": [753.4, 553.8], "r": 0.8}),
            ("arrow pointing into the door", {"t": "arrow", "a": [734.8, 557.5], "b": [750.0, 557.5], "w": 3.0, "hl": 6.8,
                                             "hw": 7.6}),
        ]),
        "tutorial": ("simple", [
            ("front square, outlined", outlined(s3, 2.2)),
            ("middle square, outlined, behind the front one", behind(outlined(s2, 2.2), s3)),
            ("back square, outlined, behind the other two", behind(outlined(s1, 2.2), s2, s3)),
        ]),
        "load": ("simple", [
            ("folder back: tab, top bar and the sliver left of the flap",
             poly((285.7, 54.3), (293.6, 54.3), (294.4, 51.1), (301.6, 51.1), (303.2, 54.3), (321.4, 54.3), (321.4, 57.0),
                  (294.8, 57.0), (287.2, 71.6), (287.4, 74.4), (285.7, 74.4))),
            ("folder flap, open toward the right", poly((295.2, 58.7), (325.8, 58.7), (318.4, 74.4), (287.4, 74.4))),
            ("arrow shaft: a curve that widens toward the head",
             {"t": "qstroke", "p": [[297.6, 50.2], [296.8, 39.6], [315.5, 40.5]], "w": [1.4, 5.6]}),
            ("arrow head", poly((315.0, 34.5), (325.4, 40.6), (315.0, 46.8))),
        ]),
        "sandbox": ("simple", [
            ("bucket handle: the upper half of an oval ring",
             {"t": "inter", "of": [outlined({"t": "ellipse", "c": [242.2, 417.2], "r": [7.2, 6.8], "rot": 0.0}, 2.0),
                                   {"t": "half", "p": [242.2, 417.2], "n": [0.0, 1.0]}]}),
            ("bucket: rim and tapered body", poly((234.3, 417.0), (250.2, 417.0), (250.2, 419.0), (249.2, 419.3),
                                                  (246.8, 427.2), (237.3, 427.2), (235.3, 419.3), (234.3, 419.0))),
            ("ground slab", poly((232.4, 427.0), (273.4, 427.0), (273.2, 429.6), (270.6, 432.0), (233.4, 432.0),
                                 (231.3, 430.2), (231.3, 428.0))),
            ("sand heap", poly((249.0, 428.5), (254.0, 424.2), (259.5, 420.6), (264.5, 416.4), (270.6, 416.2), (272.6, 419.5),
                               (272.2, 421.6), (274.2, 423.2), (274.2, 428.5), smooth=2)),
            ("shovel blade, two prongs at the top", poly((257.0, 410.2), (259.6, 411.7), (262.6, 409.7), (264.9, 410.3),
                                                         (265.6, 414.0), (266.2, 418.5), (261.5, 420.2), (258.2, 417.4),
                                                         (256.0, 416.0), (256.0, 414.0), (257.6, 412.6))),
            ("shovel shaft", line((253.2, 403.4), (258.4, 412.2), w=2.1, cap="round")),
            ("shovel grip: an outlined triangle", outlined(poly((252.6, 396.2), (246.9, 401.8), (254.4, 403.6)), 1.7)),
        ]),
        # Drawn from what they SHOW (Mariano, 2026-11-23): the trophy is a turned cup with a golfer and his club on top;
        # the ball is a dimpled sphere lit from the upper right, on a tee, in grass.
        "champ": ("depicted", [
            ("cup: one profile mirrored about its axis (lid, shoulder, bowl, stem, foot)",
             {"t": "lathe", "x": 522.8, "smooth": 1,
              "prof": [[497.8, 3.0], [500.0, 3.8], [501.5, 5.0], [503.0, 7.8], [505.0, 9.5], [506.5, 11.2], [510.0, 11.0],
                       [512.0, 10.0], [514.0, 8.0], [516.0, 5.5], [518.0, 3.2], [520.0, 2.2], [521.5, 2.6], [523.0, 4.2],
                       [525.0, 6.0], [527.0, 8.0], [528.6, 9.6], [529.2, 9.6]]}),
            ("golfer: head", {"t": "circle", "c": [522.2, 484.4], "r": 1.5}),
            ("golfer: body, leaning into the follow-through", line((522.2, 486.4), (524.4, 494.0), w=3.4, cap="round")),
            ("golfer: left leg", line((522.6, 494.2), (519.8, 497.6), w=1.6, cap="round")),
            ("golfer: right leg", line((525.6, 494.2), (525.6, 497.6), w=1.4, cap="round")),
            ("golf club, swung over the shoulder", line((520.6, 486.2), (515.2, 487.4), w=0.7)),
            ("club head", {"t": "circle", "c": [514.8, 487.6], "r": 1.0}),
        ]),
        "play": ("depicted", [
            ("golf ball: a dimpled sphere lit from the upper right",
             # Read from an enlargement (zoom.py, 2026-11-23): the dimples are an orderly staggered grid of light
             # rounded squares, rows about 5.8 px apart and 6.7 px along a row. They are tiny at the lower left and grow
             # toward the upper right, where only thin dark lines part them; the lower left third is solid. The numbers
             # below are that reading (a row through y 57.5 with cells at x 701.5, 708.5, ...; half sizes 0.5 .. 2.9
             # growing along the direction (0.8, -0.6)); the fit only nudges them. Detecting each dimple on the art
             # (ball_from_art) gave a ragged ball and is no longer used here.
             {"t": "dimpled", "cell": "square", "c": [713.5, 57.8], "r": 17.3, "spacing": 6.7, "turn": 0.0,
              # Seen at 8x beside the traced version (ball_in_button.png): the dimples are ROUND, and on the lit side
              # they grow past half a spacing and run together, leaving only the small dark corners between three of
              # them; the ball's outline is missing there too, the ball dissolving into the light.
              "phase": [1.4, -0.3], "light": [0.8, -0.6], "lz": 0.0, "l0": -0.44, "k": 0.56, "cap": 0.56, "round": 1.0,
              "rim": 1.0, "rim_l": 0.35}),
            # Seen at 8x (ball_in_button.png): the tee is a TURNED shape, a wide cup narrowing in one smooth curve to a
            # thin stem, so it is one profile mirrored about its axis like the trophy's cup; the grass is curved blades
            # fanning out of the ground on each side, with one thin blade close against the stem.
            ("tee: one profile mirrored about its axis (cup rim, neck, stem)",
             # a thin lip, then a quick concave sweep in to the stem (an hourglass, not a pedestal)
             {"t": "lathe", "x": 713.3, "smooth": 2,
              "prof": [[75.9, 11.4], [77.4, 11.2], [78.8, 8.8], [80.8, 6.4], [83.6, 4.8], [88.0, 3.9], [93.0, 3.5],
                       [99.5, 3.7]]}),
            ("cut: the light line between the ball and the cup", line((709.6, 77.3), (718.0, 77.3), w=0.9)),
            ("grass blade, left, tall", {"t": "qstroke", "p": [[706.4, 99.5], [706.4, 90.0], [703.8, 83.8]], "w": [6.0, 0.6]}),
            ("grass blade, left, leaning out", {"t": "qstroke", "p": [[703.6, 99.5], [703.0, 93.0], [699.0, 88.8]],
                                               "w": [5.6, 0.6]}),
            ("grass blade, left, short", {"t": "qstroke", "p": [[701.6, 100.0], [699.8, 98.4], [696.8, 95.6]], "w": [4.0, 0.6]}),
            ("grass blade against the stem", {"t": "qstroke", "p": [[719.0, 98.5], [719.6, 90.0], [721.6, 83.4]],
                                             "w": [1.8, 0.5]}),
            ("grass blade, right, tall", {"t": "qstroke", "p": [[723.4, 99.5], [724.0, 90.0], [728.4, 83.8]], "w": [6.4, 0.6]}),
            ("grass blade, right, leaning out", {"t": "qstroke", "p": [[726.4, 99.5], [727.6, 93.4], [731.8, 89.4]],
                                                "w": [5.6, 0.6]}),
            ("grass blade, right, short", {"t": "qstroke", "p": [[728.4, 100.0], [730.4, 98.6], [733.0, 96.0]], "w": [4.0, 0.6]}),
            ("ground: a thin line under the tufts", poly((698.2, 98.8), (731.6, 98.8), (732.0, 101.3), (698.6, 101.3))),
        ]),
    }


def curve(p0, p1, p2, w0, w1=None):
    return {"t": "qstroke", "p": [list(p0), list(p1), list(p2)], "w": [w0, w0 if w1 is None else w1]}


def oval(cx, cy, rx, ry, rot=0.0):
    return {"t": "ellipse", "c": [cx, cy], "r": [rx, ry], "rot": rot}


def authored_diff():
    """The difficulty screen's four faces (read with the dump in the session of 2026-11-23): every one is eyes, a short nose
    line and a mouth; the mouth is a smile drawn as one curve (a parabola: its sides run straighter than a circle's) with a
    cap or a bar at its ends."""
    return {
        "easy": ("simple", [        # eyes closed in a smile
            ("left eye: a closed lid, one curved line", curve((220.5, 73.2), (223.0, 65.2), (237.5, 58.3), 3.6, 5.0)),
            ("right eye: the same, mirrored", curve((266.9, 73.2), (264.4, 65.2), (249.9, 58.3), 3.6, 5.0)),
            ("nose: a short line", line((238.5, 77.3), (246.5, 77.3), w=1.6)),
            # Read again at 18x (Mariano: "the line of the mouth is not ending properly on the edges"): the line is THIN
            # (under 3 px; I had drawn 4), its sides run straight down to a rounded bottom, and each end stops INSIDE the
            # inner end of a short flat bar that points outward: the line and its bar make one corner, with no gap and no
            # overhang.
            # a parabola came out pointed again once fitted; the art's smile is a bowl: sides sloping in, a wide round
            # bottom. Drawn as a line through six points, rounded.
            # Third reading, against the button at 8x: the sides do not slope in, they drop from the bars and round under,
            # which is half a circle (my first guess was right in shape; what was wrong was its thickness, 4.2 for a line
            # of about 3, and bars that did not sit on its ends). The parts are kept as read ("fixed").
            ("smile: half a circle, thin", {"t": "arc", "c": [241.6, 90.4], "r": 22.6, "a0": 0.0, "sweep": 180.0, "w": 3.1,
                                           "fixed": True}),
            ("smile: left end bar, pointing out from the line's end",
             dict(line((211.8, 89.4), (220.5, 89.4), w=2.2), fixed=True)),
            ("smile: right end bar, pointing out from the line's end",
             dict(line((262.7, 89.4), (271.4, 89.4), w=2.2), fixed=True)),
        ]),
        "medium": ("simple", [      # the plain smiley
            ("left eye: a tall oval", oval(189.6, 205.8, 6.6, 12.0)),
            ("right eye: a tall oval", oval(216.0, 205.6, 6.8, 12.0)),
            ("nose: a short line", line((196.2, 229.2), (207.0, 229.2), w=1.7)),
            # at 16x: a thin line again; each end runs into the MIDDLE of a short bar set across it (a T at each end)
            ("smile: one thin curve", curve((172.8, 223.2), (201.2, 262.8), (229.6, 223.6), 2.9)),
            ("smile: left end cap, set across the curve", line((177.3, 219.0), (168.0, 224.6), w=3.2)),
            ("smile: right end cap", line((226.6, 219.0), (236.2, 224.8), w=3.2)),
        ]),
        "hard": ("simple", [        # frowning brows over a smirk
            ("left eye: an oval", oval(198.6, 365.5, 5.8, 8.2)),
            ("left brow: a thick slanted line into the eye", line((189.5, 350.0), (203.5, 358.5), w=5.0)),
            ("right eye: an oval", oval(217.6, 365.5, 5.8, 8.2)),
            ("right brow", line((228.5, 349.5), (214.0, 358.5), w=5.0)),
            ("nose: a short line", line((198.2, 382.3), (204.2, 382.3), w=1.6)),
            # at 18x: a thin line from the middle of a slanted cap at the left, round a low bottom, up into the under
            # side of a flat bar at the right (the bar sits on the line's end like the top of a T)
            ("smirk: one thin curve, its right side running high", curve((186.8, 393.0), (216.0, 415.0), (240.2, 375.8), 3.0)),
            ("smirk: left end cap, set across the curve", line((190.6, 388.0), (182.6, 394.8), w=3.2)),
            ("smirk: a flat bar on its right end", line((234.0, 374.0), (246.0, 374.0), w=4.0)),
        ]),
        "expert": ("simple", [      # eyes wide, mouth open
            ("left eye: an oval", oval(237.5, 489.2, 5.6, 8.2)),
            ("right eye: an oval", oval(271.5, 489.2, 5.6, 8.2)),
            ("nose: a short line", line((252.4, 504.3), (260.2, 504.3), w=1.8)),
            # at 22x (Mariano: "the tongue on the extreme difficulty needs tweak"): the mouth's wall is thin at the left
            # and thick at the right; inside, a SHORT THICK tongue hangs from the top, left of the middle, a stub that
            # swells into a round end about half way down. I had drawn a long thin line with a small dot.
            ("open mouth: a leaning oval", oval(253.2, 527.0, 15.2, 16.6, -10.0)),
            # seen in the button at 8x: the inside is small in a thick wall, and the tongue is a round dot in its middle
            # on a short thin stem to the top (a comma, upside down); the fit shrank and bent my thicker one, so both
            # stay as read ("fixed")
            ("cut: the inside of the mouth", dict(oval(251.9, 528.8, 7.8, 7.3), fixed=True)),
            ("tongue: a short thin stem from the top", dict(line((252.6, 520.2), (251.9, 526.6), w=1.8), fixed=True)),
            ("tongue: its round end", {"t": "circle", "c": [251.6, 528.8], "r": 3.0, "fixed": True}),
        ]),
    }


def authored_load():
    """The Load Game screen's three glyphs, read from their pixel grids (glyphdump.py, 2026-11-24)."""
    return {
        "ok": ("simple", [      # a check mark: a short thick stroke and a long one that leans right and widens upward
            # one outline: flat top on the long stroke, a V notch between the strokes, flat top on the short stroke,
            # one flat foot (my first reading as two overlapping bars gave a pointed foot the art does not have)
            ("check mark, one outline", dict(poly((637.4, 543.0), (647.0, 543.0), (642.0, 552.0), (638.6, 560.0), (635.8, 568.0),
                                                  (625.8, 568.0), (622.6, 560.4), (617.0, 554.6), (625.4, 554.6),
                                                  (629.8, 561.0), (633.2, 552.0)), fixed=True)),
        ]),
        "delete": ("simple", [  # an open folder with an X above it
            # Mariano: "the X looks off". Read again on the grid: it is a bold block letter X, 11 wide and 10 tall,
            # with FLAT tops and feet and a waist 6 wide, not two thin crossed strokes with slanted ends.
            ("X: a bold block letter, one outline",
             dict(poly((701.8, 540.4), (706.7, 540.4), (707.6, 542.5), (708.5, 540.4), (713.3, 540.4), (710.5, 545.4),
                       (713.5, 550.4), (708.5, 550.4), (707.6, 548.4), (706.7, 550.4), (701.6, 550.4), (704.7, 545.4)),
                  fixed=True)),
            ("folder back: tab, top bar and the sliver left of the flap",
             poly((692.2, 555.2), (696.6, 555.2), (697.4, 552.4), (703.2, 552.4), (704.6, 554.6), (718.6, 554.6), (718.6, 556.6),
                  (699.0, 556.6), (693.4, 566.2), (693.4, 568.0), (692.2, 568.0))),
            ("folder flap, open toward the right", poly((699.4, 557.7), (720.4, 557.7), (715.0, 568.0), (693.6, 568.0))),
        ]),
        "back": ("simple", [    # a block arrow pointing left
            ("arrow: a thick shaft and a wide head", {"t": "arrow", "a": [782.4, 558.0], "b": [756.2, 558.0], "w": 9.2,
                                                      "hl": 11.0, "hw": 19.0}),
        ]),
    }


def combine(parts):
    """The parts in order: each is added to what is there; a part named "cut: ..." is taken out of what is there, and a
    part after it is drawn over the cut (the uvula inside the open mouth)."""
    shape, adds = None, []

    def flush():
        nonlocal shape, adds
        if adds:
            shape = U(*([shape] if shape else []), *adds)
            adds = []

    for what, s_ in parts:
        if what.startswith("cut:"):
            flush()
            shape = {"t": "sub", "a": shape, "b": s_}
        else:
            adds.append(s_)
    flush()
    return shape


# ---------------------------------------------------------------------------------------------------------- fitting
def art_coverage(arts, meas, name, state="idle"):
    m = meas[name]
    cx, cy, r = m[state]["c"]
    k = 0.76 if m["kind"] == "disc" else min(0.90, title.GLYPH_K)
    x0, y0, x1, y1 = int(cx - r), int(cy - r), int(np.ceil(cx + r)), int(np.ceil(cy + r))
    jj, ii = np.mgrid[y0:y1, x0:x1]
    inside = np.hypot(ii + 0.5 - cx, jj + 0.5 - cy) < r * k
    cov, lo, hi = fit.dark_coverage(arts[state][y0:y1, x0:x1], inside)
    return cov, inside, (x0, y0, x1, y1)


def shape_coverage(shape, box, inside, s=4):
    x0, y0, x1, y1 = box
    ys, xs = np.mgrid[0:(y1 - y0) * s, 0:(x1 - x0) * s]
    d = sdf.sd(shape, (xs + 0.5) / s + x0, (ys + 0.5) / s + y0)
    c = np.clip(0.5 - d * s, 0, 1).reshape(y1 - y0, s, x1 - x0, s).mean(axis=(1, 3))
    return c * inside


FINE = {"light": 0.05, "lz": 0.05, "l0": 0.04, "k": 0.05, "turn": 2.0, "cap": 0.02, "rim_l": 0.05, "round": 9.0}     # first step of the numbers that are not pixels


def numbers(node, out, key=None):
    """Every fitted number of a shape tree as [container, index, step, min]."""
    if isinstance(node, dict):
        if node.get("fixed"):       # a part written as read and kept so
            return out
        for k, v in node.items():
            if k in ("t", "cap", "closed", "smooth", "rot", "n", "a0", "sweep"):
                continue
            if isinstance(v, (int, float)):
                out.append([node, k, FINE.get(k, 0.25), 0.4 if k in ("w", "r", "hl", "hw", "spacing", "rim") else None])
            else:
                numbers(v, out, k)
    elif isinstance(node, list):
        for i, v in enumerate(node):
            if isinstance(v, (int, float)):
                out.append([node, i, FINE.get(key, 0.25), 0.4 if key in ("w", "half", "r") else None])
            else:
                numbers(v, out, key)
    return out


def fit_shape(shape, cov, inside, box, passes=7, reach=2.5):
    params = numbers(shape, [])
    start = [p[0][p[1]] for p in params]
    # a number moves at most eight of its own first steps from where it was written (and never past `reach`): the fit
    # tunes a reading, it does not replace it
    limit = [min(reach, 8 * p[2]) for p in params]

    def loss():
        d = shape_coverage(shape, box, inside) - cov
        return float((d * d).mean())

    best = first = loss()
    for _ in range(passes):
        moved = 0
        for p, v_start, lim in zip(params, start, limit):
            v0 = p[0][p[1]]
            for sign in (1, -1):
                v = round(v0 + sign * p[2], 3)
                if abs(v - v_start) > lim or (p[3] is not None and v < p[3]):
                    continue
                p[0][p[1]] = v
                e = loss()
                if e < best - 1e-10:
                    best, moved, v0 = e, moved + 1, v
                    break
                p[0][p[1]] = v0
        if moved == 0:
            for p in params:
                p[2] *= 0.5
    return first, best, len(params)


def place_lattice(ball, shape, cov, inside, box):
    """The dimple lattice's turn, phase and lighting threshold by exhaustive search (the best overlap with the art)."""
    best = None
    sp = ball["spacing"]
    for l0 in (-0.3, -0.1, 0.1):
        for k in (0.3, 0.45, 0.6):
            for turn in range(0, 60, 10):
                for pu in np.arange(0, sp, 1.5):
                    for pv in np.arange(0, sp * 0.866, 1.5):
                        ball.update(l0=l0, k=k, turn=float(turn), phase=[float(pu), float(pv)])
                        d = shape_coverage(shape, box, inside, s=2) - cov
                        e = float((d * d).mean())
                        if best is None or e < best[0]:
                            best = (e, l0, k, float(turn), [float(pu), float(pv)])
    ball.update(l0=best[1], k=best[2], turn=best[3], phase=best[4])


def ball_from_art(spec, cov, box, z=4):
    """A lit dimpled ball: the disc, minus the ovals that cover the light spots the art shows inside it, plus its outline."""
    import basic
    x0, y0, x1, y1 = box
    up = np.asarray(Image.fromarray((cov * 255).astype(np.uint8)).resize(((x1 - x0) * z, (y1 - y0) * z), Image.BILINEAR),
                    float) / 255
    ys, xs = np.mgrid[0:(y1 - y0) * z, 0:(x1 - x0) * z]
    x, y = (xs + 0.5) / z + x0, (ys + 0.5) / z + y0
    # the ball's own circle, measured: the outermost dark pixels round the read centre, above where the tee covers it
    dark = cov > 0.5
    dark[int(spec["c"][1] + 0.65 * spec["r"]) - y0:] = False
    pts = fit.outermost(dark, (spec["c"][0] - x0, spec["c"][1] - y0))
    bx, by, br, _ = fit.fit_circle(pts, trim=0.7)
    spec = dict(spec, c=[round(bx + x0, 2), round(by + y0, 2)], r=round(br + 0.3, 2))
    rr = np.hypot(x - spec["c"][0], y - spec["c"][1])
    disc = rr < spec["r"] - 1.2
    # Read from an enlargement (zoom.py), the ball's dimples are a GRID OF LIGHT CELLS, rounded squares between dark
    # lines: tiny in the shadow at the lower left, larger toward the light at the upper right, where only thin dark lines
    # part them. So: one rounded rectangle per cell. A cell is a connected patch of light pixels inside the ball; its box
    # is the patch's extent, each side moved out by the light the next row or column of pixels holds. A patch that is not
    # box-like (two cells joined through a faint line) is split by asking for more light.
    H, W = cov.shape
    jj, ii = np.mgrid[0:H, 0:W]
    px, py = ii + 0.5 + x0, jj + 0.5 + y0
    inball = np.hypot(px - spec["c"][0], py - spec["c"][1]) < spec["r"] - 0.9
    lt = np.where(inball, 1.0 - cov, 0.0)

    def patches(mask):
        seen = np.zeros(mask.shape, bool)
        out = []
        for j0, i0 in zip(*np.nonzero(mask)):
            if seen[j0, i0]:
                continue
            seen[j0, i0] = True
            stack, comp = [(j0, i0)], []
            while stack:
                j, i = stack.pop()
                comp.append((j, i))
                for dj, di in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                    a, b = j + dj, i + di
                    if 0 <= a < H and 0 <= b < W and mask[a, b] and not seen[a, b]:
                        seen[a, b] = True
                        stack.append((a, b))
            out.append(comp)
        return out

    def cells(mask, thr):
        out = []
        for comp in patches(mask & (lt > thr)):
            js, is_ = np.array(comp).T
            hgt, wid = js.max() - js.min() + 1, is_.max() - is_.min() + 1
            if thr < 0.9 and (len(comp) < 0.72 * hgt * wid or max(hgt, wid) > 8):
                sub = np.zeros(mask.shape, bool)
                sub[js, is_] = True
                finer = cells(sub, thr + 0.15)
                if finer:
                    out += finer
                    continue
            out.append(comp)
        return out

    ovals = []
    for comp in cells(inball, 0.5):
        js, is_ = np.array(comp).T
        j0, j1, i0, i1 = js.min(), js.max(), is_.min(), is_.max()
        edge = lambda a: float(np.clip(a, 0, 1).mean()) if a.size else 0.0   # noqa: E731
        left = i0 - (edge(lt[j0:j1 + 1, i0 - 1]) if i0 > 0 else 0)
        right = i1 + 1 + (edge(lt[j0:j1 + 1, i1 + 1]) if i1 + 1 < W else 0)
        top = j0 - (edge(lt[j0 - 1, i0:i1 + 1]) if j0 > 0 else 0)
        bottom = j1 + 1 + (edge(lt[j1 + 1, i0:i1 + 1]) if j1 + 1 < H else 0)
        hx, hy = max((right - left) / 2, 0.55), max((bottom - top) / 2, 0.55)
        ovals.append({"t": "rrect", "c": [round(float(x0 + (left + right) / 2), 2), round(float(y0 + (top + bottom) / 2), 2)],
                      "half": [round(float(hx), 2), round(float(hy), 2)], "r": round(float(0.45 * min(hx, hy)), 2), "rot": 0.0})
    c = {"t": "circle", "c": list(spec["c"]), "r": spec["r"]}
    inner = {"t": "circle", "c": list(spec["c"]), "r": spec["r"] - spec["rim"]}
    return U({"t": "sub", "a": c, "b": U(*ovals)}, {"t": "sub", "a": copy.deepcopy(c), "b": inner})


def iou(a, b):
    a, b = a > 0.5, b > 0.5
    return float((a & b).sum() / max((a | b).sum(), 1))


def main():
    out_dir = os.path.join(fit.ROOT, "log", "roadmap", "P41", title.OUT)
    base_idx, base_pal, un_idx, un_pal, mo_idx, mo_pal = title.load_art()
    arts = {"idle": title.composite(base_idx, base_pal, un_idx, un_pal).astype(float),
            "hover": title.composite(base_idx, base_pal, mo_idx, mo_pal).astype(float)}
    meas = title._measure(verbose=False)
    result, rows = {}, []
    z = 10
    tint = [[70, 90, 200], [200, 90, 70], [60, 160, 90], [170, 80, 180], [200, 150, 40], [40, 160, 180], [120, 120, 120]]
    todo = {"diff": authored_diff, "load": authored_load, "loaddis": authored_load}.get(title.OUT, authored)()
    todo = {n: v for n, v in todo.items() if n in title.CELLS}
    only = [a for a in sys.argv[1:] if a in todo]
    if only:        # fit just these glyphs; the others keep what the last full run saved
        todo = {n: todo[n] for n in only}
        if os.path.exists(title.GLYPHS):
            with open(title.GLYPHS) as f:
                result = json.load(f)
    print(f"{'glyph':9s} {'class':8s} {'parts':>5s} {'numbers':>7s} {'overlap with the art':>21s} {'(traced outline)':>17s}")
    for k, (name, (cls, parts)) in enumerate(todo.items()):
        cov, inside, box = art_coverage(arts, meas, name)
        traced = {"t": "poly", "rings": meas[name]["idle"]["glyph"]}
        t_iou = iou(shape_coverage(traced, box, inside), cov)
        entry = {"class": cls}
        if parts:
            parts = copy.deepcopy(parts)
            shape = combine(parts)
            for i, (w, s) in enumerate(parts):      # the ball's dimples: ovals found on this art
                if s.get("t") == "ball_from_art":
                    parts[i] = (w, ball_from_art(s, cov, box))
            shape = combine(parts)
            first, best, n = fit_shape(shape, cov, inside, box, reach=0.8 if cls == "depicted" else 1.2)
            a_iou = iou(shape_coverage(shape, box, inside), cov)
            entry.update(shape=shape, parts=[w for w, _ in parts], overlap=round(a_iou, 3))
            print(f"{name:9s} {cls:8s} {len(parts):5d} {n:7d} {a_iou:21.3f} {t_iou:17.3f}", flush=True)
            # the hover art: the same parts, moved with the bubble, then fitted to that art on their own
            hc, ic = meas[name]["hover"]["c"], meas[name]["idle"]["c"]
            hshape = {"t": "move", "of": copy.deepcopy(shape), "d": [round(hc[0] - ic[0], 2), round(hc[1] - ic[1], 2)]}
            hcov, hinside, hbox = art_coverage(arts, meas, name, "hover")
            if meas[name]["kind"] == "disc":     # its hover glyph is yellow: the dark coverage does not see it
                entry.update(shape_hover=hshape)
            else:
                moved = iou(shape_coverage(hshape, hbox, hinside), hcov)
                fit_shape(hshape, hcov, hinside, hbox, reach=0.8 if cls == "depicted" else 1.2)
                h_iou = iou(shape_coverage(hshape, hbox, hinside), hcov)
                entry.update(shape_hover=hshape, overlap_hover=round(h_iou, 3))
                print(f"{'':9s} {'hover':8s} {'':5s} {'':7s} {h_iou:21.3f}   (moved only: {moved:.3f})", flush=True)
        else:
            shape = None
            entry.update(overlap=round(t_iou, 3))
            print(f"{name:9s} {cls:8s} {'-':>5s} {'-':>7s} {'(keeps its outline)':>21s} {t_iou:17.3f}", flush=True)
        result[name] = entry
        # sheet: the art | its coverage | the traced outline | the parts, tinted | the parts, solid
        x0, y0, x1, y1 = box
        up = lambda q: np.kron(q, np.ones((z, z, 1)))   # noqa: E731
        tiles = [up(arts["idle"][y0:y1, x0:x1]), up(np.repeat((255 * (1 - cov))[..., None], 3, axis=2))]
        for layers in ([{"shape": traced, "fill": [36, 42, 90]}],
                       [{"shape": s, "fill": [255, 255, 255] if w.startswith("cut:") else tint[i % len(tint)],
                         "alpha": 0.9 if w.startswith("cut:") else 0.75} for i, (w, s) in enumerate(parts or [])],
                       [{"shape": shape, "fill": [36, 42, 90]}] if shape else []):
            img = np.full(((y1 - y0) * z, (x1 - x0) * z, 3), 235.0)
            if layers:
                sdf.render([{"box": list(box), "layers": layers}], img, z, origin=(x0, y0))
            tiles.append(img)
        rows.append(np.concatenate(tiles, axis=1))
        print(f"[{k + 1}/{len(todo)}] {name} PASS", flush=True)
    with open(title.GLYPHS, "w") as f:
        json.dump(result, f)
    W = max(r.shape[1] for r in rows)
    rows = [np.pad(r, ((0, 0), (0, W - r.shape[1]), (0, 0)), constant_values=20) for r in rows]
    path = os.path.join(out_dir, f"glyphs_{title.OUT}.png" if not only else "glyphs_" + "_".join(only) + ".png")
    Image.fromarray(np.clip(np.concatenate(rows) + 0.5, 0, 255).astype(np.uint8)).save(path)
    print(path)


if __name__ == "__main__":
    main()
