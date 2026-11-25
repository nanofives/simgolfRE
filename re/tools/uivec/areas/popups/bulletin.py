"""P41 popups area: Bulletin board and Top 10 pieces that are redrawn: tacs-note, tacs-polaroid, tacs-pins, top10-charms.

Usage: py -3.12 bulletin.py [id ...]
"""
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import auto             # noqa: E402
import bubbles          # noqa: E402
import common as c      # noqa: E402
import pages            # noqa: E402
import paint            # noqa: E402
from auto import poly   # noqa: E402

TT = "tacs&tees.pcx"
CH = "Top10_Charms.pcx"


def polaroid():
    box = (58, 246, 284, 452)
    g = np.array(c.sheet(TT)[0][5, 5], float)
    # the frame, read on column 160 and row 330: outer box x 63-276, y 251-444; window x 70-270, y 258-418; the top and left
    # edges darken toward the window (the frame's inner wall in shadow), the bottom and right edges darken toward the outside;
    # a bright row second from the top of the top and bottom bars
    def px(x, y):
        return [float(v) for v in c.sheet(TT)[0][y, x]]
    top = [px(160, y) for y in range(251, 258)]
    lef = [px(x, 330) for x in range(63, 70)]
    rig = [px(x, 330) for x in range(275, 269, -1)]
    bot = [px(160, y) for y in range(443, 417, -1)]
    frame = c.rr(63.0, 251.0, 276.0, 444.0, 0.6)
    P = paint.bevel_from_bands(top, bot, lef, rig, px(160, 430))
    window = c.rr(70.0, 258.0, 270.0, 418.0, 0.0)
    it = auto.measured_item(TT, box, [(frame, {"paint": P}), (window, "flat")], ground=g, name="tacs-polaroid")
    return auto.deliver("tacs-polaroid", TT, [("idle", box, it)], ground=g, how="an instant photo frame: a white box (x 63-276, "
                        "y 251-444) whose edge rows were read side by side (top and left edges darken toward the window, bottom "
                        "and right edges toward the outside, a bright second row in the top and bottom bars), and its black "
                        "window (x 70-270, y 258-418)", z=2, unsure="the frame's white has a faint dither texture in the art; drawn flat")


def note():
    box = (50, 40, 262, 222)
    g = np.array(c.sheet(TT)[0][5, 5], float)
    # the paper: a quadrilateral turned about 3.5 degrees, corners read where its rows / columns of yellow end:
    # (58.0, 61.0), (242.6, 49.4), (252.2, 202.4), (68.2, 213.8); its light falls off toward the lower right (measured)
    paper = poly((58.0, 61.0), (242.6, 49.4), (252.2, 202.4), (68.2, 213.8))
    # the printed title (CLUBHOUSE, the logo, NOTES, 'things to do: (after a quick 18)') is lettering: kept as the art's pixels
    # inside the turned band that holds it; its pixels are kept out of the paper's measurement (an invisible layer over them)
    title = poly((59.0, 70.0), (241.0, 58.0), (243.5, 93.5), (61.5, 106.5))
    it = auto.measured_item(TT, box, [(paper, {"linear": {"n": 4}}), ({"t": "grow", "of": title, "d": 2.0}, [0, 0, 0], {"alpha": 0.0})],
                            ground=g, name="tacs-note")
    it["layers"] = it["layers"][:1]
    tex = [(TT, title)]
    mad = pages.score_tex([it], tex, [], TT, box, g, s=3)
    p = pages.compare_tex([([it], tex, [], TT, box, g)], "tacs-note", 2)
    c.save_scene("tacs-note", {"id": "tacs-note", "sheet": "Interface/" + TT, "states": {"idle": {"box": list(box), "scene": [it],
                 "textures": [{"texture": {"sheet": "Interface/" + TT, "rect": [58, 49, 194, 165]}, "clip": title,
                               "element": "the printed title and logo (kept art)"}]}}})
    c.record("tacs-note", "done", "the sticky note's paper: a quadrilateral turned about 3.5 degrees (corners read where its "
             "yellow rows and columns end), its yellow measured as a linear gradient (lighter toward the upper left); the printed "
             "title and logo are lettering and stay the art's pixels inside the turned band that holds them", mad, None,
             "the title lettering 'CLUBHOUSE NOTES / things to do: (after a quick 18)' and its logo are kept as art; say if they "
             "should be drawn (they would need the game's serif font)")
    return p


def pins():
    rgb, m, _ = c.sheet(TT)
    g = np.array(rgb[5, 5], float)
    states, n = [], 0
    for i, col in enumerate(("pink", "crimson", "orange", "yellow", "green", "blue")):
        x0 = 295 + 50 * i
        box = (x0, 2, x0 + 30, 26)
        a = c.art(TT, box, g)
        sat = a.max(axis=2) - a.min(axis=2)
        jj, ii = np.mgrid[box[1]:box[3], box[0]:box[2]]
        head = sat > 60
        pts = c.fit.edge_points(head) + np.array(box[:2], float)
        cx, cy, r, _ = c.fit.fit_circle(pts, trim=1.0)
        ball = c.circ(cx, cy, r)
        # each head casts a short grey shadow to its lower right (2-3 px wide, ending about 7 px from the centre)
        shadow = {"t": "stroke", "pts": [[cx + 2.0, cy + 2.5], [cx + 6.0, cy + 7.0]], "w": 2.4, "cap": "round"}
        parts = [(shadow, "flat", {"soft": 0.9}),
                 (ball, {"bevel": {"stops": [0.06, 0.15, 0.3, 0.5, 0.75, 1.0], "L": r, "radial": (cx, cy)}})]
        it = auto.measured_item(TT, box, parts, ground=g, name=f"tacs-pins {col}")
        sp = bubbles.spot(TT, box, (cx, cy, r), it["layers"][1]["fill"], (), g, min_res=18.0)
        if sp:
            it["layers"].append(sp)
        states.append((col, box, it))
    return auto.deliver("tacs-pins", TT, states, ground=g, how="six push pins: each a glossy round head (circle fitted on the "
                        "edge of its coloured pixels, its lit surface and specular measured) casting a short grey shadow to its "
                        "lower right", z=8)


def charms():
    box = (15, 28, 55, 150)
    g = np.array(c.sheet(CH)[0][5, 5], float)
    # four gold charms on black, read row by row (# = white light, + = gold, . = dark gold edge):
    # a cup (bowl rows 33-41 about x 32.5, handles x 26-28 and 35-39 at rows 33-37, stem rows 42-44, foot row 45 x 30-34),
    # an exclamation mark (bar rows 64-74, x 30-36 narrowing, dot rows 76-78), a heart (lobes from row 99, widest x 26-39 at
    # rows 101-102, point (32.5, 110.5)), a five-pointed star (top (32, 130.6), arms to x 25 and 39 at row 135, feet at
    # (27.8, 143.3) and (36.2, 143.3))
    cup = {"t": "union", "of": [
        {"t": "lathe", "x": 32.5, "prof": [[33.3, 3.4], [37.5, 3.6], [38.5, 3.4], [40.0, 3.0], [41.4, 2.2], [42.2, 1.0], [42.6, 0.8], [45.0, 0.8]]},
        c.rr(30.0, 44.8, 35.2, 46.0, 0.5),
        {"t": "outline", "of": {"t": "ellipse", "c": [27.6, 35.3], "r": [1.5, 1.9]}, "w": 1.1},
        {"t": "outline", "of": {"t": "ellipse", "c": [37.6, 35.3], "r": [1.5, 1.9]}, "w": 1.1}]}
    bang = {"t": "union", "of": [poly((30.0, 63.6), (36.2, 63.6), (35.9, 72.5), (35.0, 75.0), (31.0, 75.0), (30.1, 72.5)),
                                 c.rr(31.0, 75.8, 36.0, 78.6, 1.0)]}
    heart = {"t": "union", "of": [c.circ(29.4, 101.9, 3.4), c.circ(35.6, 101.9, 3.4),
                                  poly((26.1, 102.6), (38.9, 102.6), (32.5, 110.6))]}
    pts = []
    for k in range(10):
        ang = np.deg2rad(-90 + 36 * k)
        rr = 7.2 if k % 2 == 0 else 3.0
        pts.append((32.0 + rr * np.cos(ang), 137.5 + rr * np.sin(ang)))
    star = poly(*pts)
    parts = [(S, {"linear": {"n": 3}}) for S in (cup, bang, heart, star)]
    it = auto.measured_item(CH, box, parts, ground=g, inset=0.2, gap=0.3, name="top10-charms")
    return auto.deliver("top10-charms", CH, [("idle", box, it)], ground=g, how="four tiny gold charms read row by row: a cup "
                        "(a mirrored profile with two ring handles, stem and foot), an exclamation mark (a tapering bar and a "
                        "dot), a heart (two round lobes on a point) and a five-pointed star; each painted with a gold gradient "
                        "measured on its pixels (light upper left)", z=6,
                        unsure="the roster crop starts at y 40 and cuts the cup; the comparison shows y 28-150 so all four charms are whole")


FNS = {"tacs-polaroid": polaroid, "tacs-note": note, "tacs-pins": pins, "top10-charms": charms}

if __name__ == "__main__":
    todo = sys.argv[1:] or list(FNS)
    for k, eid in enumerate(todo):
        print(f"[{k + 1}/{len(todo)}] {eid} {FNS[eid]()}", flush=True)
