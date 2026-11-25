"""P41 frontend area: Pick a Pro (Title_Pickapro.pcx). Reuses Load Game's pieces for the right half (page, plate, arrows,
track, info bar, bubbles); new here: the portrait ring, the pro list (tray, little balls, name pills, connectors) and the
bottom bar. Every number below was read off dumps and radial profiles of the sheet (2026-11-24), the ring's circles fitted
to its navy pixels, the paints measured (accept.solve_bevel / least squares) on the art's own pixels.

Usage: py -3.12 re/tools/uivec/areas/frontend/pro_screen.py
"""
import json
import os

import numpy as np

import common as c
import parts
from common import fit, sdf

SHEET = "Interface/Title_Pickapro.pcx"
NAVY = [33, 43, 79]


def circle(cx, cy, r):
    return {"t": "circle", "c": [cx, cy], "r": r}


def ring_line_bevel(art, shape, width, x0, y0, x1, y1):
    """A 2 px line round `shape` whose light half faces up-left: the shape grown by `width`, painted with a lit surface
    measured on the line's pixels (depth 0..width)."""
    g = {"t": "grow", "of": shape, "d": width}
    X, Y = parts.grid(x0, y0, x1, y1)
    d = sdf.sd(shape, X, Y)
    m = (d > 0.15) & (d < width - 0.15)
    return {"shape": g, "fill": parts.bevel_fit(art[y0:y1, x0:x1], g, m, x0, y0, width, [0.0, 0.5, 1.0])}


def portrait_ring(art):
    Lm = fit.lum(art)
    x0, y0, x1, y1 = 20, 14, 260, 250
    sub = Lm[y0:y1, x0:x1]
    j, i = np.nonzero(sub < 60)
    pts = np.c_[i + x0 + 0.5, j + y0 + 0.5]
    cx, cy, _, _ = fit.fit_circle(pts, trim=1.0)
    d = np.hypot(pts[:, 0] - cx, pts[:, 1] - cy)
    ball = fit.fit_circle(pts[(d > 55) & (d < 66)], trim=0.8)
    line = fit.fit_circle(pts[(d > 75) & (d < 86)], trim=0.8)
    cx, cy = (ball[0] + line[0]) / 2, (ball[1] + line[1]) / 2
    rb, rl = ball[2], line[2]
    # the disc's outer edge: on rays, where the luminance crosses half way to the cloud (median of the rays)
    outs = []
    for deg in range(0, 360, 4):
        t = np.deg2rad(deg)
        ks = np.arange(rl + 12, rl + 40, 0.25)
        v = np.array([Lm[int(cy + k * np.sin(t)), int(cx + k * np.cos(t))] for k in ks])
        hit = np.nonzero(v > 150)[0]
        if len(hit) and hit[0] > 0 and not (170 < deg < 190):
            outs.append(ks[hit[0]])
    R = round(float(np.median(outs)), 2)
    X, Y = parts.grid(x0, y0, x1, y1)
    rr_ = np.hypot(X - cx, Y - cy)
    crop = art[y0:y1, x0:x1]
    disc = circle(cx, cy, R)
    ballc = circle(cx, cy, rb + 1.1)
    plate = {"t": "sub", "a": disc, "b": ballc}           # the ball's own pixels stay the picture
    keep = (Lm[y0:y1, x0:x1] > 62) & ~((X < 62) & (np.abs(Y - 128.5) < 4)) & ~((X < 100) & (Y > 190))
    outer = (rr_ < R - 0.6) & (rr_ > rl + 4.5) & keep
    paint_o = parts.bevel_fit(crop, disc, outer, x0, y0, R - rl, [0.0, 0.08, 0.18, 0.3, 0.5, 1.0])
    inner = (rr_ < rl - 4.0) & (rr_ > rb + 3.0) & keep
    paint_i = [round(float(v), 1) for v in np.median(crop[inner], axis=0)]
    clip_in = circle(cx, cy, rl)
    L = [
        {"shape": plate, "fill": paint_o},
        {"shape": {"t": "sub", "a": circle(cx, cy, rl - 0.8), "b": ballc}, "fill": paint_i},
        # the groove of the navy line: its soft shadow falls 2 px below it (inside at the top, outside at the bottom)
        {"shape": {"t": "move", "of": {"t": "outline", "of": circle(cx, cy, rl), "w": 2.6}, "d": [0.0, 2.0]},
         "fill": [70, 72, 110], "soft": 1.4, "alpha": 0.55, "clip": {"t": "sub", "a": disc, "b": ballc}},
        {"shape": {"t": "outline", "of": circle(cx, cy, rl), "w": 1.7}, "fill": NAVY},
        # the ball's navy rim with a light lip outside it on the lit (upper left) side
        {"shape": {"t": "arc", "c": [cx, cy], "r": rb + 2.1, "a0": 188.0, "sweep": 92.0, "w": 1.3}, "fill": [150, 152, 196],
         "alpha": 0.8},
        {"shape": {"t": "outline", "of": circle(cx, cy, rb), "w": 2.2}, "fill": NAVY},
    ]
    # the two wires (read off a dump): a horizontal one into the ring line from the left, turning down at the screen's
    # left wire, and a diagonal one from the ring line down to the list's header pill, 2 px then 1 px, each with its soft
    # shadow to the lower right
    w1 = {"t": "stroke", "pts": [[19.6, 134.0], [21.6, 130.4], [25.6, 128.5], [cx - rl, 128.5]], "w": 1.2, "cap": "butt"}
    w2 = {"t": "stroke", "pts": [[89.8, 198.8], [78.4, 219.6]], "w": 2.0, "cap": "butt"}
    w3 = {"t": "stroke", "pts": [[78.6, 220.6], [66.2, 246.0]], "w": 1.2, "cap": "butt"}
    for w in (w1, w2, w3):
        L.insert(4, {"shape": {"t": "move", "of": {"t": "grow", "of": w, "d": 1.0}, "d": [1.6, 1.6]}, "fill": [70, 72, 110],
                     "soft": 1.2, "alpha": 0.45})
    L += [{"shape": w, "fill": NAVY} for w in (w1, w2, w3)]
    info = {"c": [round(cx, 2), round(cy, 2)], "ball_r": round(rb, 2), "line_r": round(rl, 2), "outer_r": R}
    return {"name": "portrait ring", "box": [x0, y0, x1, y1], "layers": L}, info


def pro_list(art, boxes):
    """The tray, the header pill (boxes.py), and ten rows of a little cream ball, a lilac connector and a cream name pill."""
    tray, tinfo = parts.tray(art, (30.0, 259.6, 247.0, 522.0), r=9.0,
                             exclude=c.shape_mask([parts_item(b) for b in boxes], 0, 0, 800, 600, 3.0))
    items = [{"name": "list tray", "box": [26, 255, 251, 526], "layers": tray}]
    hdr = [b for b in boxes if b["box"][1] == 256]
    pills = [b for b in boxes if b["box"][0] == 91]
    items += [parts_item(b) for b in hdr]
    Lm = fit.lum(art)
    for b in pills:
        x0, y0, x1, y1 = b["box"]
        cy = (y0 + y1) / 2 + 0.25
        # the little ball: an ellipse read off rows 273-296 / columns 37-82 of the first row (rim included); the next rows
        # are the same at 24 px steps
        ball = {"t": "ellipse", "c": [59.75, cy], "r": [20.6, 9.8], "rot": 0.0}
        pill = parts.rr(x0, y0, x1, y1, b["r"])
        L = []
        bx0, by0, bx1, by1 = 34, int(cy) - 14, 94, int(cy) + 15
        L.append(ring_line_bevel(art, ball, 2.0, bx0, by0, bx1, by1))
        X, Y = parts.grid(bx0, by0, bx1, by1)
        m = sdf.sd(ball, X, Y) < -0.6
        L.append({"shape": ball, "fill": parts.bevel_fit(art[by0:by1, bx0:bx1], ball, m, bx0, by0, 9.8,
                                                         [0.0, 0.1, 0.25, 0.45, 0.7, 1.0])})
        # the connector: a 2 px light lilac bar with a dark line under it (rows y0+6..y0+9)
        L.append({"shape": parts.rr(82.0, y0 + 6.0, 89.0, y0 + 8.0, 0.0), "fill": [165, 167, 203]})
        L.append({"shape": parts.rr(82.0, y0 + 8.0, 89.0, y0 + 9.0, 0.0), "fill": [76, 76, 112]})
        px0, py0, px1, py1 = x0 - 3, y0 - 3, x1 + 3, y1 + 3
        L.append(ring_line_bevel(art, pill, 2.0, px0, py0, px1, py1))
        L.append({"shape": pill, "fill": b["fill"]})
        items.append({"name": f"row {y0}", "box": [34, y0 - 5, x1 + 4, y1 + 5], "layers": L})
    return items, tinfo


def parts_item(b):
    import boxes as bx
    return bx.item(b)


def bottom_bar(art, boxes):
    """A long rounded bar whose inside is a 50 % black veil over the backdrop (least squares of the bar's pixels against the
    backdrop just above and below: alpha 0.51, colour about (5, 6, 8) on every channel) in a navy line, under a lilac pill."""
    bar = parts.rr(29.6, 550.2, 555.6, 579.6, 9.5)
    inner = parts.rr(31.2, 551.4, 554.0, 578.0, 8.0)
    L = [{"shape": {"t": "sub", "a": bar, "b": inner}, "fill": NAVY},
         {"shape": inner, "fill": [5.0, 6.0, 8.0], "alpha": 0.51}]
    items = [{"name": "bottom bar", "box": [25, 546, 560, 584], "layers": L}]
    items += [parts_item(b) for b in boxes if b["box"][1] == 546]
    return items


def main():
    art, _ = c.sheet(SHEET)
    idx, pal = fit.load_sheet(SHEET)
    import boxes as bx
    boxes, _ = bx.measure(pal[idx])
    ring, rinfo = portrait_ring(art)
    print(rinfo)
    sc = c.element("pro-ball-ring", [{"name": "idle", "art": art, "scene": [ring], "rect": (18, 12, 244, 240)}],
                   "a lavender disc lit from the upper left (measured lit surface over its outer band), a 1.7 px navy circle "
                   "line with its soft shadow 2 px below, the ball's 2.2 px navy rim with a light lip on the lit side (all "
                   "circles fitted to the navy pixels, one centre), and the two navy wires read off a dump; the ball stays "
                   "the picture (its pixels are neither wiped nor painted)", z=2, extra={"measured": rinfo})
    print("pro-ball-ring", sc)
    lst, tinfo = pro_list(art, boxes)
    sc = c.element("pro-list", [{"name": "idle", "art": art, "scene": lst, "rect": (26, 246, 226, 280)}],
                   "a dark rounded tray (measured outline, linear fill) under a lilac header pill (boxes.py); ten rows of "
                   "a cream ellipse ball (measured shading, a 2 px lilac rim lit from the upper left), a two-tone lilac "
                   "connector bar and a flat cream name pill in the same 2 px rim", z=2)
    print("pro-list", sc)
    bb = bottom_bar(art, boxes)
    sc = c.element("pro-bottom-bar", [{"name": "idle", "art": art, "scene": bb, "rect": (24, 534, 538, 50)}],
                   "a long rounded bar: a navy line round a 51 % black veil over the backdrop (alpha and colour fitted by "
                   "least squares), under a lilac stadium pill (boxes.py)", z=1,
                   unsure="the veil darkens whatever the backdrop is under it, so the frame's diagonal edge shows through it "
                          "as in the art")
    print("pro-bottom-bar", sc)
    c.kept("pro-ball", "the cream dimpled golf ball inside the portrait ring stays the picture", art, (78, 68, 126, 126))
    # the backdrop: this screen's shapes and the Load Game pieces it shares (same cells), over the art
    load = json.load(open(os.path.join(c.OUT, "_load_screen_scene.json")))["scene"]
    shared = [it for it in load if it.get("name") in ("scroll up, first arrow", "scroll up, second arrow", "scroll down, first arrow",
                                                      "scroll down, second arrow", "scroll track", "info bar")]
    shared += [it for it in load if it["box"][1] > 500 and it["box"][0] > 580]       # OK, Delete, Back
    page = [parts_item(b) for b in boxes if b["box"][1] in (39, 111)]
    allsc = [ring] + lst + bb + page + shared
    sc = c.element("pro-backdrop", [{"name": "idle", "art": art, "scene": allsc, "rect": (0, 0, 800, 600)}],
                   "the whole screen: cloud and arched frame (with its faint ball) kept as texture, the portrait ring, the "
                   "list, the bottom bar, the page and title plate (boxes.py) and Load Game's arrows, track, info bar and "
                   "buttons drawn over it", z=1, unsure="the arched frame's lines are still texture")
    print("pro-backdrop", sc)


if __name__ == "__main__":
    main()
