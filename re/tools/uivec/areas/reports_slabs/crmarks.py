"""Course Report legend and marks (round 2, 2026-11-24), authored shapes read on coursereport.pcx.

Read (text dumps in log/roadmap/P41/work2/reports_slabs/_read/crmarks.txt):
- TILE: a 14 x 15 raised square (legend: orange 114..128 x 477..492; marks row: five tiles at y 557..572 between green
  guide columns). Two-pixel bevel: lit top rows and left columns, shaded right columns and bottom rows (the bottom row is
  the darkest). Painted as a bevel whose four sides are MEASURED on each tile (depth 0.5 / 1.5 / body).
- BUG: a 10 x 13 navy pictogram, mirror-symmetric about the column of its stem (legend axis x 430.5, bar row 483): two
  top lobes joined in the middle (mandibles), a small head disc under them, a thin antenna line down each side, a block on
  each side carried by a full-width bar (the front legs), a stem down the middle, and a hooked leg on each side that
  curls in to the stem's foot. Drawn as those parts in local coordinates (u = x - axis, v = y - bar top), left half
  mirrored.
- ARROWS (the three markers, 11 x 13 cells at x 547..558): hole-shape arrows. Straight: a triangular head (tip 552.5, 563;
  base row 568) on a 5 px stem. Dogleg right: a 4 px stem bottom-left, a 45 degree shaft up-right, a right-angle head
  whose corner is the top-right of the cell. Dogleg left: the same turned the other way (corner top-left, stem
  bottom-right). The faces turned to the upper right are lit (a lighter strip along them).
- STUBS: a 2 px line (rows 483-484) from each legend symbol to its ring pill, in the ring's own colour.
"""
import json
import os

import numpy as np

import common as C
import slabkit as K

SHEET = "coursereport.pcx"


def _art():
    return C.art(SHEET)


def side_bevel(top, bottom, left, right, body):
    """A bevel paint from four measured side profiles (colours of the outer and the second pixel row of each side), as
    crisp one-pixel bands (each band flat, a short ramp between), then the body."""
    stops = [0.0, 0.9, 1.1, 1.9, 2.1]
    prof = lambda side: [side[0], side[0], side[1], side[1], body]
    T, B, Lf, R = prof(top), prof(bottom), prof(left), prof(right)
    coef = []
    for k in range(len(stops)):
        t, b, l, r = (np.array(v[k], float) for v in (T, B, Lf, R))
        c1 = (r - l) / 2
        c2 = (b - t) / 2
        lr = (l + r) / 2
        tb = (t + b) / 2
        coef.append([list((lr + tb) / 2), list(c1), list(c2), list((lr - tb) / 2), [0.0, 0.0, 0.0]])
    return {"bevel": {"L": 1.0, "stops": stops, "coef": coef}}


def tile(raw, x0, y0, w=14, h=15):
    """A raised tile: its rectangle and its four sides measured on the art (row / column means, corners left out, and
    the pixels of a glyph drawn on the tile left out: luminance under 120)."""
    sub = raw[y0:y0 + h, x0:x0 + w].astype(float)

    def m(a):
        a = a.reshape(-1, 3)
        k = C.fit.lum(a) >= 120
        a = a[k] if k.sum() >= 2 else a
        return [float(v) for v in a.mean(axis=0)]
    top = [m(sub[0, 2:-2]), m(sub[1, 2:-2])]
    bottom = [m(sub[-1, 2:-2]), m(sub[-2, 2:-2])]
    left = [m(sub[2:-2, 0]), m(sub[2:-2, 1])]
    right = [m(sub[2:-2, -1]), m(sub[2:-2, -2])]
    inner = sub[2:-2, 2:-2].reshape(-1, 3)
    k = C.fit.lum(inner) >= 120
    body = [float(v) for v in np.median(inner[k], axis=0)]
    S = {"t": "rrect", "c": [x0 + w / 2, y0 + h / 2], "half": [w / 2, h / 2], "r": 0.0, "rot": 0.0}
    return {"shape": S, "fill": side_bevel(top, bottom, left, right, body)}, body


def _mir(pts):
    return [[-u, v] for u, v in pts]


def bug_shapes(ax, bar_y):
    """The bug's parts as shapes, in sheet coordinates (axis x of its stem's centre, top edge of its bar row)."""
    def P(pts):
        return [[ax + u, bar_y + v] for u, v in pts]

    lobe = [[-4.6, -7.0], [-1.5, -7.0], [-0.55, -6.1], [-0.55, -5.4], [-1.5, -5.3], [-1.5, -4.0], [-3.5, -4.0], [-4.6, -6.0]]
    block = [[-4.5, -1.0], [-1.5, -1.0], [-1.5, 2.0], [-4.5, 2.0]]
    # the foot: a block that leans in toward the stem (rows 486-488: u -4.5..-2.5, then -3.5..-1.5), joined to the stem's
    # foot by a thin bridge on the last row
    foot = [[-4.6, 3.0], [-2.2, 3.0], [-1.4, 4.0], [-1.4, 6.0], [-3.6, 6.0], [-4.0, 4.4]]
    out = []
    for f in (lambda p: p, _mir):
        out.append({"t": "poly", "rings": [P(f(lobe))]})
        out.append({"t": "seg", "a": P(f([[-3.0, -4.2]]))[0], "b": P(f([[-3.0, -0.8]]))[0], "w": 1.0})
        out.append({"t": "poly", "rings": [P(f(block))]})
        out.append({"t": "poly", "rings": [P(f(foot))]})
        out.append({"t": "poly", "rings": [P(f([[-1.5, 5.25], [0.0, 5.25], [0.0, 5.95], [-1.5, 5.95]]))]})
    out.append({"t": "rrect", "c": [ax, bar_y + 0.5], "half": [4.9, 0.5], "r": 0.0, "rot": 0.0})       # the bar
    out.append({"t": "rrect", "c": [ax, bar_y + 3.5], "half": [0.5, 2.5], "r": 0.0, "rot": 0.0})       # the stem
    # the head: a small diamond (pixels 430 / 429-431 / 430 on rows 479-481)
    out.append({"t": "poly", "rings": [P([[0.0, -4.1], [1.6, -2.5], [0.0, -0.9], [-1.6, -2.5]])]})
    return {"t": "union", "of": out}


def glyph_colour(raw, box):
    x0, y0, x1, y1 = box
    sub = raw[y0:y1, x0:x1].reshape(-1, 3).astype(float)
    L = C.fit.lum(sub)
    return [float(v) for v in np.median(sub[L < 70], axis=0)]


def _band(edge_a, edge_b, inward, depth):
    """The half plane within `depth` of the line edge_a..edge_b on its inner side (inward: unit normal into the shape)."""
    n = np.array(inward, float) / np.hypot(*inward)
    p = np.array(edge_a, float) + depth * n
    return {"t": "half", "p": [float(p[0]), float(p[1])], "n": [float(n[0]), float(n[1])]}


def arrow_shapes():
    """(name, body shape, lit-strip shape) of the three markers. The lit strips are the faces turned up / up-right:
    a band about 1.3 px deep along that edge (rows of light pixels in the dump)."""
    straight_head = {"t": "poly", "rings": [[[552.5, 562.8], [558.0, 569.0], [547.0, 569.0]]]}
    straight = {"t": "union", "of": [straight_head, {"t": "rrect", "c": [552.5, 572.5], "half": [2.5, 3.5], "r": 0.0, "rot": 0.0}]}
    # the right slope (tip to the base's right end) is the lit face
    straight_lit = {"t": "inter", "of": [straight_head, _band([552.5, 562.8], [558.0, 569.0], [-6.2, 5.5], 1.4)]}
    # dogleg right: head corner top-right (558, 549), legs along the top and the right edge, shaft down-left to the stem
    r_head = {"t": "poly", "rings": [[[549.6, 549.0], [558.0, 549.0], [558.0, 557.4]]]}
    r_shaft = {"t": "stroke", "pts": [[550.8, 557.0], [555.0, 552.8]], "w": 4.8, "cap": "butt"}
    r_body = {"t": "union", "of": [r_head, r_shaft, {"t": "rrect", "c": [551.0, 558.5], "half": [2.0, 3.5], "r": 0.0, "rot": 0.0}]}
    r_lit = {"t": "inter", "of": [r_head, _band([549.6, 549.0], [558.0, 549.0], [0.0, 1.0], 1.5)]}
    # dogleg left: corner top-left (547, 577), stem 551..555 from 584 to 590; lit: the top row and the up-right face of
    # the hypotenuse and the shaft (one line, x + y ~ 633)
    l_head = {"t": "poly", "rings": [[[547.0, 577.0], [555.6, 577.0], [547.0, 585.6]]]}
    l_shaft = {"t": "stroke", "pts": [[550.4, 580.6], [554.2, 584.4]], "w": 4.8, "cap": "butt"}
    l_body = {"t": "union", "of": [l_head, l_shaft, {"t": "rrect", "c": [553.0, 587.0], "half": [2.0, 3.0], "r": 0.0, "rot": 0.0}]}
    l_lit = {"t": "union", "of": [
        {"t": "inter", "of": [l_head, _band([547.0, 577.0], [555.6, 577.0], [0.0, 1.0], 1.5)]},
        {"t": "inter", "of": [{"t": "union", "of": [l_head, l_shaft]}, _band([551.7, 579.3], [555.5, 583.1], [-1.0, 1.0], 1.5),
                              {"t": "half", "p": [0.0, 584.6], "n": [0.0, 1.0]}]}]}
    return [("straight", straight, straight_lit), ("dogleg right", r_body, r_lit), ("dogleg left", l_body, l_lit)]


def icon_tiles():
    """The five tiles of the marks row: x of each tile between the green guide columns (y 557..572)."""
    art, alpha, raw = _art()
    row = raw[564]
    g = (row[:, 0] == 0) & (row[:, 1] == 255) & (row[:, 2] == 0)
    xs = [x for x in range(190, 272) if g[x]]
    tiles = []
    for a, b in zip(xs, xs[1:]):
        if b - a > 10:
            tiles.append((a + 1, b - a - 1))
    return tiles


def legend_items(sh):
    """Items for the legend (drawn by build.py over the slab): the two tiles, the bug, the three stubs."""
    raw = sh.raw
    items = []
    navy = glyph_colour(raw, (424, 474, 437, 490))
    ring = [p for p in sh.pieces if p["group"] == "pill_ring"]
    ring_col = sh.paints.get("pill_ring", {}).get("line", [92, 98, 141])
    # the legend symbols: (kind, x0, right edge x of the symbol)
    syms = []
    for x0 in _legend_tile_xs(raw):
        L, body = tile(raw, x0, 477)
        items.append(K.item([L]) | {"part": f"legend_tile{x0}"})
        syms.append(x0 + 14)
    items.append(K.item([{"shape": bug_shapes(430.5, 483.0), "fill": navy}]) | {"part": "legend_bug"})
    syms.append(435.4)
    for xr in syms:
        # the ring pill to its right on the same row
        cand = [p for p in ring if p["cen"][0] > xr and p["cen"][1] < 484 < p["cen"][3]]
        if not cand:
            continue
        p = min(cand, key=lambda q: q["cen"][0])
        items.append(K.item([{"shape": {"t": "seg", "a": [xr - 0.3, 484.0], "b": [p["cen"][0] + 0.5, 484.0], "w": 2.0},
                              "fill": ring_col}]) | {"part": f"legend_stub{int(xr)}"})
    return items


def _legend_tile_xs(raw):
    """Left x of each raised tile in the legend row (its lit top-left pixel e7cfad / f6f6.. starts a 14 px run)."""
    xs = []
    y = 484
    x = 100
    while x < 420:
        c = raw[y, x]
        sat = max(c) - min(c)
        if sat > 100 and c[0] > 150 and c[2] < 140:      # orange / yellow body
            # walk back to the tile's left edge
            x0 = x
            xs.append(int(x0))
            x += 16
            continue
        x += 1
    return xs


LEGEND_SKIP = [[112, 475, 130, 494], [260, 475, 290, 494]]


def build_marks():
    """coursereport-marks: the three arrows and the five icon tiles. The art shown is the colour sheet with its colour
    key (magenta) and green guide lines as the flat backdrop (the alpha sheet does not cover these cut sprites)."""
    art, alpha, raw = _art()
    idx, _ = C.fit.load_sheet(C.IS + SHEET)
    key = (idx >= 0xfe) | ((raw[..., 0] == 0) & (raw[..., 1] == 255) & (raw[..., 2] == 0)) | (
        (raw[..., 0] == 255) & (raw[..., 1] == 0) & (raw[..., 2] == 255))
    view = np.where(key[..., None], C.FLAT, raw)
    navy = glyph_colour(raw, (547, 549, 558, 590))
    sub = raw[549:590, 547:558].reshape(-1, 3).astype(float)
    Ls = C.fit.lum(sub)
    kk = key[549:590, 547:558].reshape(-1)
    lit = [float(v) for v in np.median(sub[(Ls > 150) & ~kk], axis=0)]
    scene = []
    for name, body, litS in arrow_shapes():
        scene.append(K.item([{"shape": body, "fill": navy}, {"shape": litS, "fill": lit}]) | {"part": name})
    bugcol = glyph_colour(raw, (226, 557, 237, 571))
    for x0, w in icon_tiles():
        L, body = tile(raw, x0, 557, w, 15)
        layers = [L]
        # a tile carrying the bug: its stem axis read on the bar row (565)
        row = C.fit.lum(raw[565, x0:x0 + w].astype(float))
        dark = np.nonzero(row < 70)[0]
        if len(dark) >= 7:
            ax = x0 + (dark.min() + dark.max() + 1) / 2
            layers.append({"shape": bug_shapes(ax, 565.0), "fill": bugcol})
        scene.append(K.item(layers) | {"part": f"icon{x0}"})
    rows = [("markers", (546, 548, 13, 43)), ("icons", (193, 556, 76, 17))]

    def mine(box, s):
        return K.render(scene, box, s)
    K.compare_png(os.path.join(C.OUT, "coursereport-marks.png"), view, mine, rows, scale=12)
    mads = []
    for st, (x, y, w, h) in rows:
        low = K.down(mine((x, y, x + w, y + h), 4), 4)
        m = ~key[y:y + h, x:x + w]
        mads.append(round(float(np.abs(low - view[y:y + h, x:x + w])[m].mean()), 2))
    json.dump({"id": "coursereport-marks", "sheet": SHEET, "frame": "800x600 sheet pixels", "crops": rows, "scene": scene,
               "read": __doc__}, open(os.path.join(C.OUT, "coursereport-marks.json"), "w"))
    print("coursereport-marks mad", mads)
    return mads


if __name__ == "__main__":
    build_marks()
