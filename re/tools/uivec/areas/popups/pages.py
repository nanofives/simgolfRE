"""P41 popups area: the Pair select page (pair-base, pair-title-pill, pair-ok-button, pair-capsule).

Kept pictures (the book medallion, the capsule's golf ball) stay textures: in the comparison image their art pixels are pasted
inside a read shape (a circle, a box) between the shapes drawn under them and the chrome drawn over them; in the scene json
they are {"texture": sheet rectangle, "clip": shape} entries, never pixels.

Usage: py -3.12 pages.py [id ...]
"""
import os
import sys

import numpy as np
from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import auto             # noqa: E402
import bubbles          # noqa: E402
import common as c      # noqa: E402
import paint            # noqa: E402
from auto import poly   # noqa: E402

PB, PBt = "PairBase.pcx", "PairButtons.pcx"
BG = [103, 103, 159]
NAVY = [38, 46, 82]


def render_tex(under, textures, over, box, s, ground):
    """under / over: scenes; textures: list of (rel, clip shape). Returns the image at scale s."""
    img = c.render_box(under, box, s, ground)
    x0, y0, x1, y1 = box
    jj, ii = np.mgrid[0:(y1 - y0) * s, 0:(x1 - x0) * s]
    x, y = (ii + 0.5) / s + x0, (jj + 0.5) / s + y0
    for rel, clip in textures:
        a = np.kron(c.art(rel, box), np.ones((s, s, 1)))
        al = np.clip(0.5 - c.sdf.sd(clip, x, y) * s, 0, 1)[..., None]
        img = img * (1 - al) + a * al
    c.sdf.render(over, img, s, origin=(x0, y0))
    return img


def score_tex(under, textures, over, rel, box, ground, s=4):
    x0, y0, x1, y1 = box
    low = render_tex(under, textures, over, box, s, ground).reshape(y1 - y0, s, x1 - x0, s, 3).mean(axis=(1, 3))
    a = c.art(rel, box, ground if np.ndim(ground) == 1 else c.FLAT)
    _, m, _ = c.sheet(rel)
    d = np.abs(low - a)[m[y0:y1, x0:x1]]
    return float(d.mean())


def compare_tex(rows, name, z):
    tiles = []
    for under, textures, over, rel, box, ground in rows:
        x0, y0, x1, y1 = box
        a = np.kron(c.art(rel, box, ground), np.ones((z, z, 1)))
        r = render_tex(under, textures, over, box, z, ground)
        sep = np.full((a.shape[0], 8, 3), 255.0)
        tiles.append(np.concatenate([a, sep, r], axis=1))
    W = max(t.shape[1] for t in tiles)
    sheet = np.concatenate([np.pad(t, ((0, 6), (0, W - t.shape[1]), (0, 0)), constant_values=255) for t in tiles])
    p = os.path.join(c.OUT, name + ".png")
    Image.fromarray(np.clip(sheet + 0.5, 0, 255).astype(np.uint8)).save(p)
    return p


# ------------------------------------------------------------------------------------------------------------ pieces
def ok_button_layers(rel, box, ground, dy=0.0, dx=0.0):
    """The pair check button: a soft pad (shadow and sunken well, measured), a thin navy ring r 33 (c (728, 537), read on
    eight radial profiles: dark minimum at r 31-35 left / right, 32-34 up / down), a glass bubble r 26 (its navy rim's
    minimum at r 23-26.5 about (728.3, 537.0); lit surface measured) and a navy block check read as each row's dark run."""
    cx, cy = 728.3 + dx, 537.0 + dy
    pad = c.circ(728.0 + dx, 537.0 + dy, 37.5)
    ring = {"t": "outline", "of": c.circ(728.0 + dx, 537.0 + dy, 33.0), "w": 1.7}
    bub = c.circ(cx, cy, 26.0)
    check = poly((714.0, 537.0), (722.0, 537.0), (725.2, 542.0), (726.2, 542.0), (733.0, 525.0), (741.0, 525.0), (731.0, 543.5),
                 (731.0, 550.0), (721.0, 550.0), (720.3, 546.0), (719.2, 543.2))
    check = auto.mv(check, dx, dy)
    parts = [(pad, {"bevel": {"stops": [0.5, 1.5, 2.5, 3.5, 5.5, 7.0, 8.5, 10.0, 11.5], "L": 1.0, "radial": (728.0 + dx, 537.0 + dy)}}),
             (ring, "flat"),
             (bub, {"bevel": {"stops": [0.023, 0.06, 0.1, 0.15, 0.23, 0.33, 0.46, 0.62, 0.8, 1.0], "L": 26.0, "radial": (cx, cy)}}),
             (check, "flat")]
    it = auto.measured_item(rel, box, parts, ground=ground)
    P = it["layers"][2]["fill"]
    sp = bubbles.spot(rel, box, (cx, cy, 26.0), P, [check], ground)
    if sp:
        it["layers"].insert(3, sp)
    return it


def pair_ok_button():
    box = (693, 502, 768, 577)
    idle = ok_button_layers(PB, box, np.array(BG, float))
    hover = ok_button_layers(PBt, box, c.FLAT)
    states = [("idle", box, idle), ("hover", box, hover)]
    rows, out, mads = [], {}, []
    for st, b, it in states:
        g = np.array(BG, float) if st == "idle" else c.FLAT
        mads.append(c.score([it], PB if st == "idle" else PBt, b, g)[0])
        out[st] = {"box": list(b), "scene": [it]}
    p1 = c.compare([([idle], PB, box, np.array(BG, float)), ([hover], PBt, box, c.FLAT)], "pair-ok-button", zoom_k=5)
    c.save_scene("pair-ok-button", {"id": "pair-ok-button", "sheets": {"idle": "Interface/" + PB, "hover": "Interface/" + PBt},
                                    "states": out})
    e = c.edge_px([idle], PB, box, np.array(BG, float))
    c.record("pair-ok-button", "done", "a soft pad (the shadow round it and the sunken well inside the ring, measured), a thin "
             "navy ring r 33, a glass bubble r 26 with its navy rim (blue idle, yellow hover; lit surface and specular measured "
             "per state) and a navy block check read as each row's dark run; centres from eight radial profiles", float(np.mean(mads)), e,
             "")
    return p1


def title_pill_layers():
    """The title pill: a rounded box x 121.6-555.4, y 8-38, corner radius 7, a 2 px navy outline, its lilac inside shaded
    darker along the top and left edges (6 px) and slightly along the bottom (measured), and the pill's soft shadow on the
    page below it."""
    outer = c.rr(121.6, 8.0, 555.4, 38.0, 7.0)
    inner = c.rr(123.6, 10.0, 553.4, 36.0, 5.0)
    return outer, inner


def pair_title_pill():
    box = (118, 4, 562, 44)
    outer, inner = title_pill_layers()
    shadow = auto.mv(outer, 1.0, 3.0)
    parts = [(shadow, [40, 40, 72], {"alpha": 0.45, "soft": 2.2}),
             (outer, "flat"),
             (inner, {"bevel": {"stops": [0.5, 1.5, 2.5, 3.5, 4.5, 6.0, 8.0]}})]
    it = auto.measured_item(PB, box, parts, ground=np.array(BG, float))
    return auto.deliver("pair-title-pill", PB, [("idle", box, it)], ground=np.array(BG, float), how="a long rounded box "
                        "(x 121.6-555.4, y 8-38, corner radius 7, read on its rows and columns) in a 2 px navy outline; its lilac "
                        "inside is shaded darker along the top and left edges (measured as a bevel paint), and a soft shadow on "
                        "the page below it", z=2)


# ------------------------------------------------------------------------------------------------------------ the page
SHEET_ARC = (-898.26, 320.85, 1580.49)     # the sheet's right edge: circle fitted through its edge at 13 rows (resid 0.5 px)


def sheet_shape(r=9.0):
    """The cream sheet: left x 6, top y 46, bottom y 594, its right side the arc SHEET_ARC; corners rounded r 9 (rows 46-55
    of the left edge: x 14, 12, 10, 8, 6)."""
    ax, ay, R = SHEET_ARC
    core = {"t": "inter", "of": [c.rr(6 + r, 46 + r, 760, 594 - r, 0.0), c.circ(ax, ay, R - r)]}
    return {"t": "grow", "of": core, "d": r}


def pair_base():
    box = (0, 0, 800, 600)
    sheet = sheet_shape()
    wall = {"t": "grow", "of": sheet, "d": 6.0}
    # the sheet sits in a sunken window: a 6 px wall round it, dark on the top and left (in shadow), light on the right and
    # bottom; each side's six rows read from the sheet's edge outward, listed here from the wall's outer edge inward
    top = [[98, 98, 146], [86, 86, 126], [82, 82, 118], [74, 74, 106], [66, 66, 98], [66, 66, 94]]
    bot = [[118, 118, 162], [126, 126, 170], [134, 134, 174], [138, 138, 178], [142, 138, 178], [142, 142, 178]]
    rig = [[114, 114, 162], [126, 126, 170], [134, 134, 174], [138, 138, 178], [142, 138, 178], [142, 142, 178]]
    wpaint = paint.bevel_from_bands(top, bot, top, rig, [255, 247, 231])
    wpaint["bevel"]["of"] = wall
    wire1 = {"t": "stroke", "pts": [[555.0, 23.4], [700.0, 23.4], [708.0, 28.5], [714.0, 38.5], [718.5, 48.0], [722.5, 59.0]],
             "w": 2.0, "cap": "butt", "smooth": 2}
    wire2 = {"t": "stroke", "pts": [[735.5, 138.0], [741.5, 155.0], [746.5, 200.0], [750.5, 300.0], [749.5, 360.0], [746.5, 410.0],
                                    [743.5, 455.0], [739.5, 485.0], [737.0, 506.0]], "w": 2.0, "cap": "butt", "smooth": 2}
    outer, inner = title_pill_layers()
    under = [c.item(box, [
        {"shape": c.rr(-1, -1, 801, 601, 0.0), "fill": BG},
        {"shape": wall, "fill": wpaint},
        {"shape": sheet, "fill": [255, 247, 231]},
        # each wire: a 2 px navy line with a soft shadow 2 px to its lower right (rows / columns of lum 69, 48, then 90-107)
        {"shape": auto.mv(wire1, 1.5, 2.0), "fill": [40, 40, 72], "alpha": 0.35, "soft": 1.6},
        {"shape": auto.mv(wire2, 2.0, 1.0), "fill": [40, 40, 72], "alpha": 0.35, "soft": 1.6},
        {"shape": wire1, "fill": NAVY},
        {"shape": wire2, "fill": NAVY},
    ], "pair-base page")]
    pill = auto.measured_item(PB, (118, 4, 562, 44), [(auto.mv(outer, 1.0, 3.0), [40, 40, 72], {"alpha": 0.45, "soft": 2.2}),
                                                       (outer, "flat"), (inner, {"bevel": {"stops": [0.5, 1.5, 2.5, 3.5, 4.5, 6.0, 8.0]}})],
                              ground=np.array(BG, float), name="pair-title-pill")
    okb = ok_button_layers(PB, (693, 502, 768, 577), np.array(BG, float))
    under.append(pill)
    under.append(okb)
    # the book medallion stays art: its pixels inside the circle the medallion's dark ring encloses (c (738.5, 99.5), r 47)
    book_clip = c.circ(738.5, 100.5, 47.5)
    textures = [(PB, book_clip)]
    mad = score_tex(under, textures, [], PB, box, c.FLAT, s=2)
    p = compare_tex([(under, textures, [], PB, box, c.FLAT)], "pair-base", 1)
    c.save_scene("pair-base", {"id": "pair-base", "sheet": "Interface/" + PB, "states": {"idle": {
        "box": list(box), "scene": under, "textures": [{"texture": {"sheet": "Interface/" + PB, "rect": [688, 52, 100, 100]},
                                                         "clip": book_clip, "element": "pair-book-icon (kept art)"}]}}})
    c.record("pair-base", "done", "the full page: plain navy-lilac ground; the cream sheet (left x 6, top 46, bottom 594, its right "
             "side an arc of radius 1580 fitted through 13 rows of its edge, corners r 9) sunk into a 6 px wall that is dark on "
             "the top and left and light on the right and bottom (rows read side by side); the two wires (2 px navy with a soft "
             "shadow, traced by their darkest pixel every few rows); the title pill and the check button as drawn for "
             "pair-title-pill / pair-ok-button; the book medallion kept as art inside its ring", mad, None,
             "the book medallion is pasted art (pair-book-icon is marked art); the medallion ring round it is part of that art")
    return p


BALL = {"idle": (67.56, 65.65), "hover": (67.25, 64.76), "selected": (67.57, 65.73)}    # ball centres per state (y in the cell)


def capsule_state(st, y0):
    """One state of the pair capsule (cell at y0). Read on columns 130, 203, 204, 250, 305-320 and rows 0-135 of the idle cell:
    the capsule's outline is navy, 2 px (top edge y 2.45, bottom 127.7, right x 311.5, corners r 9); a cream header (y 4.45-28.3)
    over a tan body (y 29.9-125.6), the navy between them the divider (rows 28-29); the body's first row is a darker tan; the
    body's right part is ruled: a vertical line at x 203-204 and two horizontal ones at rows 61-62 and 93-94 (each a grey row
    then a dark row); a soft grey shadow below and right on the cream page round it; the golf ball (kept art) inside its navy
    ring (outer edge r 63 fitted on 87 points of the left half and the free arcs, ring 2 px)."""
    box = (0, y0, 329, y0 + 136)
    def Y(v):
        return v + y0
    outer = c.rr(0.0, Y(2.45), 311.5, Y(127.7), 9.0)
    header = c.rr(2.0, Y(4.45), 309.5, Y(40.0), 7.0)
    header = {"t": "inter", "of": [header, c.rr(-5, Y(0), 340, Y(28.3), 0.0)]}
    body = c.rr(2.0, Y(29.9), 309.5, Y(125.6), 0.0)
    page = c.rr(0.0, Y(-3.0), 328.5, Y(136.0), 14.0)
    bx, by = BALL[st]
    ball_ring = {"t": "outline", "of": c.circ(bx, Y(by) - (0 if st != "hover" else 0), 62.0), "w": 2.0}
    lines = [({"t": "stroke", "pts": [[203.5, Y(30.0)], [203.5, Y(125.6)]], "w": 1.0}, "flat"),
             ({"t": "stroke", "pts": [[204.5, Y(30.0)], [204.5, Y(125.6)]], "w": 1.0}, "flat")]
    for ry in (61, 93):
        lines += [({"t": "stroke", "pts": [[205.0, Y(ry + 0.5)], [309.5, Y(ry + 0.5)]], "w": 1.0}, "flat"),
                  ({"t": "stroke", "pts": [[205.0, Y(ry + 1.5)], [309.5, Y(ry + 1.5)]], "w": 1.0}, "flat")]
    parts = [(page, "flat"),
             (auto.mv(outer, 3.0, 3.5), [120, 122, 112], {"alpha": 0.55, "soft": 3.2}),
             (outer, "flat"),
             (header, "flat"),
             (body, "flat"),
             ({"t": "stroke", "pts": [[2.0, Y(30.4)], [309.5, Y(30.4)]], "w": 1.0}, "flat")] + lines
    it = auto.measured_item(PBt, box, parts, ground=c.FLAT, name=f"pair-capsule {st}")
    under = [it]
    clip = c.circ(bx, Y(by), 61.0)
    over = [c.item(box, [{"shape": ball_ring, "fill": NAVY}])]
    return box, under, [(PBt, clip)], over, clip


def pair_capsule():
    rows, out, mads = [], {}, []
    for st, y0 in (("idle", 0), ("hover", 136), ("selected", 272)):
        box, under, tex, over, clip = capsule_state(st, y0)
        mads.append(score_tex(under, tex, over, PBt, box, c.FLAT, s=3))
        rows.append((under, tex, over, PBt, box, c.FLAT))
        out[st] = {"box": list(box), "scene": under + over,
                   "textures": [{"texture": {"sheet": "Interface/" + PBt, "rect": [box[0], box[1], 329, 136]}, "clip": clip,
                                 "element": "the golf ball (kept art, Mariano 2026-11-24)"}]}
        print(f"  pair-capsule {st:9s} mad {mads[-1]:6.2f}")
    p = compare_tex(rows, "pair-capsule", 2)
    c.save_scene("pair-capsule", {"id": "pair-capsule", "sheet": "Interface/" + PBt, "states": out})
    c.record("pair-capsule", "done", "the capsule as chrome round the kept golf ball: navy outline 2 px (corners r 9), cream "
             "header over a tan body (yellow when selected) with the navy divider between them, a darker first body row, the "
             "ruled right part (one vertical and two horizontal lines, each a grey row and a dark row), the soft grey shadow on "
             "the cream page round it, and the ball's navy ring; the ball itself is the art's pixels inside the ring (per state: "
             "pale on hover)", float(np.mean(mads)), None,
             "the capsule's right edge bulges about 1.5 px in the middle in the art; drawn straight")
    return p


FNS = {"pair-ok-button": pair_ok_button, "pair-title-pill": pair_title_pill, "pair-base": pair_base,
       "pair-capsule": pair_capsule}

if __name__ == "__main__":
    todo = sys.argv[1:] or list(FNS)
    for k, eid in enumerate(todo):
        print(f"[{k + 1}/{len(todo)}] {eid} {FNS[eid]()}", flush=True)
