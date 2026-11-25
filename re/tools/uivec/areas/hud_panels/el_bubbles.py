"""P41 hud_panels: the bubble button elements (glyphs authored from their pixel grids, 2026-11-24).

Every glyph below was READ off a coverage / RGB dump of the idle cell (common.py dump / covdump) and written as its parts.
Coordinates are sheet pixels of the idle cell; another state's glyph is the same parts moved by the whole-pixel offset of
that state's bubble body (the art moves the glyph with the button when it is pressed). Part colours are sampled per state
on that state's art (kit.sample), never typed in.

Usage: py -3.12 re/tools/uivec/areas/hud_panels/el_bubbles.py [element id ...]
"""
import json
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import common as C  # noqa: E402
import bubble as B  # noqa: E402
import kit as K  # noqa: E402
from kit import U, line, curve, poly, rect, circle, ellipse, sub, inter, outline  # noqa: E402,F401

GEOM = json.load(open(os.path.join(HERE, "bubble_geom.json")))
PAINT = B.load_paint()
SHADOWS_FILE = os.path.join(HERE, "bubble_shadow.json")
SHADOWS = json.load(open(SHADOWS_FILE)) if os.path.exists(SHADOWS_FILE) else {}


def offset(el, st, ref="idle"):
    a, b = GEOM[el][ref]["body"], GEOM[el][st]["body"]
    ra, rb = GEOM[el][ref]["rect"], GEOM[el][st]["rect"]
    return (rb[0] - ra[0]) + round(b[0] - a[0] - (rb[0] - ra[0])), (rb[1] - ra[1]) + round(b[1] - a[1] - (rb[1] - ra[1]))


def glyph_layers(parts, key, dx, dy, shade=None):
    """parts: [(name, shape, paint)] with paint 'sample' (measure it on this state's art) or a colour from another part's
    sample: ('as', name). Returns layers with the measured colours."""
    out, cols = [], {}
    for name, shape, how in parts:
        s = K.shift(shape, dx, dy)
        if how == "SHADE":
            col = shade(key, dx, dy)
        elif how == "sample":
            col = K.sample(key, s)
        elif isinstance(how, tuple) and how[0] == "as":
            col = cols[how[1]]
        elif how == "dark":       # a thin dark stroke: the darkest third of the pixels under it
            col = K.sample_dark(key, s)
        elif isinstance(how, tuple) and how[0] == "sample_in":   # sample inside another shape (the part's core)
            col = K.sample(key, K.shift(how[1], dx, dy))
        else:
            col = how
        cols[name] = col
        out.append({"name": name, "shape": s, "fill": col})
    return out


# ------------------------------------------------------------------------------------------------------------ grounds
def ground_small(c, key):
    """The Golfer action panel's club / crossed / trophy column: a lavender disc behind the bubble and a stub to the right
    that runs into the panel's lilac strip (read on J 0,500: disc r 16 round (15.7, 515.9); stub x 29..34, strip x 34..38
    with a lighter top edge). Placed from the bubble's rim centre c (idle club rim at (15.93, 516.24))."""
    dx, dy = c[0] - 15.93, c[1] - 516.24
    disc = circle(15.7 + dx, 515.9 + dy, 16.1)
    stub = rect(27.0 + dx, 509.0 + dy, 34.0 + dx, 526.0 + dy)
    strip = rect(34.0 + dx, 509.0 + dy, 38.0 + dx, 526.0 + dy)
    lav = K.sample(key, rect(30 + dx, 513 + dy, 33 + dx, 522 + dy))
    cols = [K.sample(key, rect(34 + i + dx, 515 + dy, 35 + i + dx, 525 + dy)) for i in range(4)]
    top = K.sample(key, rect(34 + dx, 509 + dy, 36 + dx, 510 + dy))
    L = [K.lay(U(disc, stub), lav),
         K.lay(strip, {"linear": {"a": [34.0 + dx, 0], "b": [38.0 + dx, 0],
                                  "stops": [[0.125, cols[0]], [0.375, cols[1]], [0.625, cols[2]], [0.875, cols[3]]]}}),
         K.lay(line((34 + dx, 509.6 + dy), (38 + dx, 509.6 + dy), w=1.2), top, alpha=0.9)]
    return L, U(disc, stub)


SMALL_ROWS = {"golfer-club-button": 500, "golfer-crossed-button": 535, "golfer-trophy-button": 566}


def glyph_club():
    """A golf club standing upright with a ball beside its head: black grip (x 13, y 506-510), a mid-brown shaft with a
    lighter right side down to y 521, a dark head turned left at its foot (x 10-13, y 521-522), and a ball 2 px wide at
    (17-18, 520): white top, dark lower half (its shadow)."""
    return [
        ("shaft", line((13.9, 510.0), (13.9, 521.2), w=1.7), "sample"),
        ("grip", line((13.5, 505.6), (13.5, 510.4), w=1.1), "sample"),
        ("head", poly((10.0, 521.0), (14.0, 520.6), (14.0, 522.0), (12.6, 523.0), (10.8, 522.6), smooth=1), "sample"),
        ("ball shadow", ellipse(18.0, 521.6, 1.1, 0.6), "sample"),
        ("ball", ellipse(18.0, 520.5, 1.1, 0.6), "sample"),
    ]


def glyph_crossed():
    """Two clubs crossed in an X at (15.5, 548): each a light-brown shaft from a black grip at the bottom (left grip
    (11, 553) -> (10, 556), right grip (19, 553) -> (20, 556)) up to a small oval head at the top on the other side
    (left head round (10, 542.3), right head round (20.8, 542.3)): a small mid-brown blade with one light pixel."""
    return [
        ("shaft 1", line((11.8, 552.6), (19.4, 543.4), w=1.1), "sample"),
        ("shaft 2", line((19.2, 552.6), (11.6, 543.4), w=1.1), ("as", "shaft 1")),
        ("grip 1", line((11.7, 553.0), (10.1, 556.0), w=1.4), ("sample_in", line((11.6, 554.5), (10.6, 555.5), w=0.3))),
        ("grip 2", line((19.3, 553.0), (20.9, 556.0), w=1.4), ("as", "grip 1")),
        ("head 1", ellipse(10.0, 542.3, 1.9, 1.3, -15), ("sample_in", circle(9.0, 543.0, 0.5))),
        ("head 1 light", circle(9.5, 542.2, 0.6), ("sample_in", circle(9.5, 542.5, 0.4))),
        ("head 2", ellipse(20.8, 542.3, 1.9, 1.3, 15), ("as", "head 1")),
        ("head 2 light", circle(21.4, 542.2, 0.6), ("as", "head 1 light")),
    ]


def glyph_trophy():
    """A two-handled cup: one profile mirrored about x 16 (rim y 571, bowl walls x 12..20 down to y 581, narrowing to the
    stem x 15..17 at 583-585, foot x 12..19 at 586-588), shaded across: dark left wall, light middle, mid-brown right; a
    ring handle each side (left round (9.8, 574.6), right round (21.8, 574.6)); a dark rim line along the top."""
    axis = 16.0
    prof = [[571.0, 4.6], [574.0, 4.2], [578.0, 4.0], [580.5, 3.7], [582.2, 2.4], [583.0, 1.2], [585.4, 1.1],
            [586.0, 3.2], [587.0, 3.6], [588.0, 3.3]]
    cup = {"t": "lathe", "x": axis, "prof": prof, "smooth": 1}
    return [
        ("handle left", outline(ellipse(9.9, 574.6, 1.8, 2.3), 1.3), ("sample_in", circle(8.4, 574.8, 0.4))),
        ("handle right", outline(ellipse(22.0, 574.6, 1.8, 2.3), 1.3), ("as", "handle left")),
        ("cup", cup, "SHADE"),
        ("rim", line((11.4, 571.6), (20.6, 571.6), w=1.1), ("sample_in", circle(14.5, 571.5, 0.4))),
        ("foot", line((12.4, 586.5), (19.0, 586.5), w=1.1), ("sample_in", circle(15.5, 586.5, 0.4))),
    ]


def trophy_shade(key, dx, dy):
    """The cup's paint across its width, sampled column by column on the art (rows 574-579)."""
    stops = []
    for i, xx in enumerate(range(12, 20)):
        c = K.sample(key, rect(xx + dx, 574 + dy, xx + 1 + dx, 580 + dy))
        stops.append([(xx + 0.5 - 11.4) / (20.6 - 11.4), c])
    return {"linear": {"a": [11.4 + dx, 0], "b": [20.6 + dx, 0], "stops": stops}}


GLYPHS = {"golfer-club-button": glyph_club, "golfer-crossed-button": glyph_crossed, "golfer-trophy-button": glyph_trophy}


def build_small(el):
    scenes = {}
    for st, key, rct in C.CELLS[el]:
        g = GEOM[el][st]
        dx, dy = offset(el, st)
        gdx, gdy = dx, dy
        y_cell = SMALL_ROWS[el]
        ground, gshape = ground_small(g["c"], key)
        parts = GLYPHS[el]()
        lay = glyph_layers(parts, key, gdx, gdy, shade=trophy_shade)
        GU[(el, st)] = U(*[L["shape"] for L in lay])
        fam = g["family"]
        sh = SHADOWS.get(fam)
        it = B.item(g["c"], g["body"], PAINT[fam], glyph=lay, shadow=sh, ground=gshape)
        scenes[st] = [K.item(ground, box=[rct[0], rct[1], rct[0] + rct[2], rct[1] + rct[3]]), it]
    return scenes


def fit_shadows_small():
    out = {}
    els = list(SMALL_ROWS)
    for st in ("idle", "selected", "disabled", "hover"):
        insts = []
        for el in els:
            _, key, rct = [c for c in C.CELLS[el] if c[0] == st][0]
            g = GEOM[el][st]
            _, gshape = ground_small(g["c"], key)
            import sdf
            ok = (lambda gs: (lambda x, y: sdf.sd(gs, x, y) < -0.6))(gshape)
            insts.append((key, g["c"], ok))
        lav = K.sample("J", rect(30, 513, 33, 522))
        out["J:small:" + st] = B.fit_shadow(insts, lav)
        print("shadow J:small:" + st, out["J:small:" + st])
    return out


GU = {}     # (element, state) -> the union of its glyph's parts, for leaving them out of the paint measurement


def measure_paint(ids):
    """Each paint family's body coefficients over every built cell of it, the glyph's parts (grown 1.3 px) left out."""
    for el in ids:
        BUILD[el](el)
    fams = {}
    for (el, st), gu in GU.items():
        g = GEOM[el][st]
        fams.setdefault(g["family"], []).append(B.body_rows(g["key"], g["c"], g["body"], trim_glyph=gu))
    for fam, rows in fams.items():
        PAINT[fam]["shade"] = B.solve_family(rows)
        print("paint", fam, len(rows))
    B.save_paint(PAINT)


def run(ids):
    for el in ids:
        scenes = BUILD[el](el)
        mad, edge = C.compare(el, scenes)
        print(f"{el}: interior_mad {mad:.2f} edge_px {edge:.2f}")


BUILD = {k: build_small for k in SMALL_ROWS}
SHADOW_FITS = [fit_shadows_small]

def main(argv):
    if argv[:1] == ["--shadows"]:
        for f in SHADOW_FITS:
            SHADOWS.update(f())
        json.dump(SHADOWS, open(SHADOWS_FILE, "w"))
        return
    if argv[:1] == ["--paint"]:
        measure_paint(argv[1:] or list(BUILD))
        return
    run(argv or list(BUILD))


if __name__ == "__main__":
    main(sys.argv[1:])
