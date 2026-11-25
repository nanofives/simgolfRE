"""P41 popups fix round: pair-base ("rounded corners") and pair-title-pill ("better corners").

pair-base: the first round built the sheet as grow(inter(rect, big circle), 9). An intersection's distance is a max(), not a
Euclidean distance outside its corner, so growing it kept the two right corners (where the top and bottom edges meet the
right arc) sharp; only the left corners, which come from the plain rounded rectangle, were round. Now the sheet is one exact
polygon: straight left, top and bottom edges, the right arc (c (-898.26, 320.85), r 1580.49, as before) and four true fillet
arcs, each tangent to both edges it joins. Radii read on the art's edge rows (first cream pixel per row; for the right corners
the gap between the edge and the arc): top left / bottom left 9.5 (rows 46-54 / 585-593: x 13, 11, 10, 9, 8, 7, 7, 7, 6),
top right 11.5 (8.3 px off the arc on row 46, 2.3 on row 50), bottom right 10 (5.1 off on row 593, 2.0 on row 589).

pair-title-pill: re-read. Its ends are radius 11, not 7 (outer edge x 121.9 on rows 18-25, then 122.2 / 123.0 / 124.3 /
126.6 / 128.2 on rows 16 / 14 / 12 / 10 / 9, the top edge y 8.33 from x 133; the bottom line's centre per column 30.5 / 33.0 /
35.5 / 36.3 / 37.0 at x 123.5 / 124.5 / 127.5 / 129.5 / 131.5 fit a line-centre radius of 10), the box is x 121.9-556.2,
y 8.4-38.0, the navy line 1.55 px (row 8 is anti-aliased, row 9 full; column 122 anti-aliased, 123 full), and its shadow
falls to the right (row 30: 46 at the line, then 56, 69, 78, 90, 99, 107 out to the page's 109 over 6 px; nothing on the left).

py -3.12 pages2.py [pair-title-pill] [pair-base]
"""
import os
import sys

import numpy as np
from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import w2               # noqa: E402
import common as c      # noqa: E402
import auto             # noqa: E402
import pages            # noqa: E402
import paint            # noqa: E402

PB, BG, NAVY = pages.PB, pages.BG, pages.NAVY
PILL = (121.9, 8.4, 556.2, 38.0, 11.0, 1.55)
PILL_SHADOW = (2.2, 1.2, 0.5, 2.6)        # offset x, offset y, strength, softness
CORNERS = {"tl": 9.5, "bl": 9.5, "tr": 11.5, "br": 10.0}
LEFT, TOP, BOT = 6.0, 46.0, 594.0


def _arc_pts(c0, r, a0, a1, n):
    return [[c0[0] + r * np.cos(t), c0[1] + r * np.sin(t)] for t in np.linspace(a0, a1, n)]


def sheet_poly():
    ax, ay, R = pages.SHEET_ARC
    pts = []
    # top left fillet: from the left edge up round to the top edge
    r = CORNERS["tl"]
    pts += _arc_pts((LEFT + r, TOP + r), r, np.pi, 1.5 * np.pi, 10)
    # top right fillet: centre r below the top edge and r inside the big circle
    r = CORNERS["tr"]
    cy = TOP + r
    cx = ax + np.sqrt((R - r) ** 2 - (cy - ay) ** 2)
    t_arc = np.arctan2(cy - ay, cx - ax)                      # direction of the tangent point on the big arc
    pts += _arc_pts((cx, cy), r, 1.5 * np.pi, 2 * np.pi + t_arc, 12)
    # the big arc down to the bottom right fillet
    r2 = CORNERS["br"]
    cy2 = BOT - r2
    cx2 = ax + np.sqrt((R - r2) ** 2 - (cy2 - ay) ** 2)
    t_arc2 = np.arctan2(cy2 - ay, cx2 - ax)
    pts += _arc_pts((ax, ay), R, t_arc, t_arc2, 80)[1:-1]
    pts += _arc_pts((cx2, cy2), r2, t_arc2, 0.5 * np.pi, 12)
    r = CORNERS["bl"]
    pts += _arc_pts((LEFT + r, BOT - r), r, 0.5 * np.pi, np.pi, 10)
    return {"t": "poly", "rings": [[[round(float(p[0]), 3), round(float(p[1]), 3)] for p in pts]]}


def pill_shapes():
    x0, y0, x1, y1, r, w = PILL
    return c.rr(x0, y0, x1, y1, r), c.rr(x0 + w, y0 + w, x1 - w, y1 - w, r - w)


def pill_parts():
    outer, inner = pill_shapes()
    dx, dy, a, s = PILL_SHADOW
    return [(auto.mv(outer, dx, dy), [40, 40, 72], {"alpha": a, "soft": s}),
            (outer, "flat"),
            (inner, {"bevel": {"stops": [0.5, 1.5, 2.5, 3.5, 4.5, 6.0, 8.0]}})]


def pair_title_pill():
    box = (114, 2, 566, 44)
    it = auto.measured_item(PB, box, pill_parts(), ground=np.array(BG, float))
    rows = [([it], PB, box, np.array(BG, float))]
    mad, w16 = c.score([it], PB, box, np.array(BG, float))
    p = c.compare(rows, "pair-title-pill", zoom_k=2)
    # the two ends at 10x below (art left, shapes right), so the corners can be judged
    ends = []
    for ex in (114, 536):
        eb = (ex, 2, ex + 30, 44)
        a = np.kron(c.art(PB, eb, np.array(BG, float)), np.ones((10, 10, 1)))
        rimg = c.render_box([it], eb, 10, np.array(BG, float))
        ends.append(np.concatenate([a, np.full((a.shape[0], 8, 3), 255.0), rimg], axis=1))
    top = np.asarray(Image.open(p).convert("RGB")).astype(float)
    band = np.concatenate([ends[0], np.full((ends[0].shape[0], 16, 3), 255.0), ends[1]], axis=1)
    W = max(top.shape[1], band.shape[1])
    out = np.concatenate([np.pad(top, ((0, 6), (0, W - top.shape[1]), (0, 0)), constant_values=255),
                          np.pad(band, ((0, 0), (0, W - band.shape[1]), (0, 0)), constant_values=255)])
    Image.fromarray(np.clip(out + 0.5, 0, 255).astype(np.uint8)).save(p)
    c.save_scene("pair-title-pill", {"id": "pair-title-pill", "sheet": "Interface/" + PB, "states": {"idle": {"box": list(box), "scene": [it]}}})
    print("  pair-title-pill mad %.2f" % mad)
    return p


def pair_base():
    box = (0, 0, 800, 600)
    sheet = sheet_poly()
    wall = {"t": "grow", "of": sheet, "d": 6.0}
    top = [[98, 98, 146], [86, 86, 126], [82, 82, 118], [74, 74, 106], [66, 66, 98], [66, 66, 94]]
    bot = [[118, 118, 162], [126, 126, 170], [134, 134, 174], [138, 138, 178], [142, 138, 178], [142, 142, 178]]
    rig = [[114, 114, 162], [126, 126, 170], [134, 134, 174], [138, 138, 178], [142, 138, 178], [142, 142, 178]]
    wpaint = paint.bevel_from_bands(top, bot, top, rig, [255, 247, 231])
    wpaint["bevel"]["of"] = wall
    wire1 = {"t": "stroke", "pts": [[555.0, 23.4], [700.0, 23.4], [708.0, 28.5], [714.0, 38.5], [718.5, 48.0], [722.5, 59.0]],
             "w": 2.0, "cap": "butt", "smooth": 2}
    wire2 = {"t": "stroke", "pts": [[735.5, 138.0], [741.5, 155.0], [746.5, 200.0], [750.5, 300.0], [749.5, 360.0], [746.5, 410.0],
                                    [743.5, 455.0], [739.5, 485.0], [737.0, 506.0]], "w": 2.0, "cap": "butt", "smooth": 2}
    under = [c.item(box, [
        {"shape": c.rr(-1, -1, 801, 601, 0.0), "fill": BG},
        {"shape": wall, "fill": wpaint},
        {"shape": sheet, "fill": [255, 247, 231]},
        {"shape": auto.mv(wire1, 1.5, 2.0), "fill": [40, 40, 72], "alpha": 0.35, "soft": 1.6},
        {"shape": auto.mv(wire2, 2.0, 1.0), "fill": [40, 40, 72], "alpha": 0.35, "soft": 1.6},
        {"shape": wire1, "fill": NAVY},
        {"shape": wire2, "fill": NAVY},
    ], "pair-base page")]
    pill = auto.measured_item(PB, (114, 2, 566, 44), pill_parts(), ground=np.array(BG, float), name="pair-title-pill")
    okb = pages.ok_button_layers(PB, (693, 502, 768, 577), np.array(BG, float))
    under += [pill, okb]
    book_clip = c.circ(738.5, 100.5, 47.5)
    textures = [(PB, book_clip)]
    mad = pages.score_tex(under, textures, [], PB, box, c.FLAT, s=2)
    p = pages.compare_tex([(under, textures, [], PB, box, c.FLAT)], "pair-base", 1)
    # the four sheet corners at 8x (art left, shapes right), appended under the page
    tiles = []
    for (x, y) in ((0, 40), (0, 560), (630, 40), (630, 560)):
        cb = (x, y, x + 40, y + 40)
        a = np.kron(c.art(PB, cb), np.ones((8, 8, 1)))
        rimg = pages.render_tex(under, textures, [], cb, 8, c.FLAT)
        tiles.append(np.concatenate([a, np.full((320, 6, 3), 255.0), rimg], axis=1))
    band = np.concatenate([np.concatenate([t, np.full((320, 12, 3), 255.0)], axis=1) for t in tiles[:2]], axis=1)
    band2 = np.concatenate([np.concatenate([t, np.full((320, 12, 3), 255.0)], axis=1) for t in tiles[2:]], axis=1)
    pg = np.asarray(Image.open(p).convert("RGB")).astype(float)
    W = pg.shape[1]
    rows = [pg] + [np.pad(b, ((6, 0), (0, W - b.shape[1]), (0, 0)), constant_values=255) for b in (band, band2)]
    Image.fromarray(np.clip(np.concatenate(rows) + 0.5, 0, 255).astype(np.uint8)).save(p)
    c.save_scene("pair-base", {"id": "pair-base", "sheet": "Interface/" + PB, "states": {"idle": {
        "box": list(box), "scene": under, "textures": [{"texture": {"sheet": "Interface/" + PB, "rect": [688, 52, 100, 100]},
                                                         "clip": book_clip, "element": "pair-book-icon (kept art)"}]}}})
    print("  pair-base mad %.2f" % mad)
    return p


FNS = {"pair-title-pill": pair_title_pill, "pair-base": pair_base}

if __name__ == "__main__":
    todo = sys.argv[1:] or list(FNS)
    for k, eid in enumerate(todo):
        print(f"[{k + 1}/{len(todo)}] {eid} {FNS[eid]()}", flush=True)
