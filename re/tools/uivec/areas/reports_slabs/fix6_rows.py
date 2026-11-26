"""P41 round 6, reports_slabs item 2: the Course Report's hole-row label boxes match the art's rows.

Reading of the art (coursereport.pcx, sheet pixels; dumps in log/roadmap/P41/work6/reports_slabs/_read/):
- The slab's left wire is a straight vertical line at x 6..7 through every strip (head, hole rows, subtotal).
- The label box ("Hole N") is its own navy frame, separate from that wire: left side x 11..12, right side x 104..105,
  bottom y 144..145 of the 17-row strip, open at the strip's top (the row above, or the head's line at y 102..103,
  closes it). Its bottom-left corner is round (about 2.5 px), the bottom-right nearly square (about 1.5 px). The head
  piece holds the first box's top line (y 102..103, round top-left corner); the subtotal piece's top holds the last
  box's bottom (y 420..421, both ends rising).
- Inside, the cream is sunken: 3 rows of shade under the top (lum 188, 209, 232 against 244), 2 columns at each side
  (209, 232) and 1 row over the bottom (232).
- The next cell's bottom line starts at x 112 (x 106..111 is slab, no line).

The approved scene joined the box's bottom line to the slab wire with a corner (wires 120 / 122 / 124 / 125), turned
the next cell's bottom line up the box's right side (121 / 123), drew the left side short (151 / 152, to y 142) and
left the slab's bevel showing as a grey band inside the box. This rewrites exactly those items; nothing else changes.

py -3.12 fix6_rows.py      writes work6/reports_slabs/coursereport-slab.json and coursereport-slab.png
"""
import os

import numpy as np

import common as C
import slabkit as K
import fix6_common as F
import fix6_scenic as S
import fix6_stubs as T

EID = "coursereport-slab"
LX, RX = 12.05, 105.0            # centres of the box's left (x 11..12) and right (x 104..105) sides
RL, RR = 1.5, 1.5                # corner radii of the rows and subtotal: small, both ends
RH = 2.5                         # the head's top-left corner is rounder (y 102..103 reaches x 12 only at y 104)
SLAB_X = 7.1                     # the slab wire's centre (x 6..7), as the approved head wire 119
CREAM = np.array([255.0, 247.0, 231.0])
# shade factors of the sunken cream, read at pixel depth 0.5 / 1.5 / 2.5 / 3.5 from the edge (lum / 244):
# top 0.770 / 0.856 / 0.950 / 1, sides 0.856 / 0.950 / 1, bottom 0.950 / 1


# the sunken cream's shade, as the art has it: each wall darkens the cream by a factor and the factors multiply where two
# walls meet (corner pixel 174 = cream x 0.77 x 0.856 within 2 %). Each wall is a soft black rectangle outside that wall,
# clipped to the well; a black layer of alpha a multiplies what is under it by (1 - a). (A bevel paint on the well gave
# the same walls but a mitre line along each corner's diagonal, which the art does not have.)
# (edge offset of the wall from the well, alpha, soft): fitted so the pixel rows at depth 0.5 / 1.5 / 2.5 / 3.5 come out at
# 1 - TOP / SIDE / BOT above: top 0.23 / 0.144 / 0.05 / 0, sides 0.144 / 0.05 / 0, bottom 0.05 / 0.
WALLS = {"top": (0.566, 4.0), "side": (0.41, 2.5), "bottom": (0.193, 1.5)}


def well_item(top):
    x0, x1, y0, y1 = 13.0, 104.0, float(top), top + 15.0
    well = {"t": "rrect", "c": [(x0 + x1) / 2, (y0 + y1) / 2], "half": [(x1 - x0) / 2, (y1 - y0) / 2], "r": 0.0, "rot": 0.0}

    def rect(a, b, c, d):
        return {"t": "rrect", "c": [(a + b) / 2, (c + d) / 2], "half": [(b - a) / 2, (d - c) / 2], "r": 0.0, "rot": 0.0}
    walls = [("top", rect(x0 - 10, x1 + 10, y0 - 10, y0)), ("side", rect(x0 - 10, x0, y0 - 10, y1 + 10)),
             ("side", rect(x1, x1 + 10, y0 - 10, y1 + 10)), ("bottom", rect(x0 - 10, x1 + 10, y1, y1 + 10))]
    layers = [{"shape": well, "fill": [float(v) for v in CREAM]}]
    for kind, S in walls:
        a, soft = WALLS[kind]
        layers.append({"shape": S, "fill": [0.0, 0.0, 0.0], "alpha": a, "soft": soft, "clip": well})
    return {"box": [x0 - 1, y0 - 1, x1 + 1, y1 + 1], "layers": layers}


def row_frame(top, by, right_only=False):
    """the label box of one hole-row strip: down the left side, round bottom-left, along the bottom, up the right.
    right_only: the part from the bottom-left corner on (its cast shadow; the left side casts onto the cream, which the
    well paint already shades)"""
    p = [[LX, top], [LX, by - RL]]
    p += F.arc_pts((LX + RL, by - RL), RL, 180, 90)[1:]
    p += [[RX - RR, by]]
    p += F.arc_pts((RX - RR, by - RR), RR, 90, 0)[1:]
    p += [[RX, top]]
    if right_only:
        p = p[2:]
    return p


def head_line(y):
    """the first box's top line in the head piece: right side up into a near-square corner, along, round top-left"""
    p = [[RX, y + RR + 1.0]]
    p += F.arc_pts((RX - RR, y + RR), RR, 0, -90)
    p += F.arc_pts((LX + RH, y + RH), RH, -90, -180)
    p += [[LX, y + RH + 1.0]]
    return p


def pts_of(it):
    S = it["layers"][0]["shape"]
    return (S["of"] if S["t"] == "move" else S)["pts"]


def expect(sc, i, first, last):
    p = pts_of(sc[i])
    assert np.allclose(p[0], first, atol=0.15) and np.allclose(p[-1], last, atol=0.15), (i, p[0], p[-1])


def build(prev):
    sc = F.deep(prev["scene"])
    # check that the items are the ones read above before rewriting them
    expect(sc, 120, (106.0, 103.2), (7.1, 87.0))
    expect(sc, 2, (106.0, 103.2), (7.1, 87.0))
    for i, s in ((121, 3), (123, 5)):
        expect(sc, i, pts_of(sc[s])[0], pts_of(sc[s])[-1])
    expect(sc, 122, (106.0, 145.07), (7.1, 129.0))
    expect(sc, 124, (106.0, 185.07), (7.1, 169.0))
    expect(sc, 125, (105.0, 421.1), (7.1, 449.0))
    expect(sc, 151, (12.09, 129.0), (12.09, 142.0))
    expect(sc, 152, (12.09, 169.0), (12.09, 182.0))
    assert sc[64]["layers"][0]["shape"]["c"] == [58.5, 137.5] and sc[77]["layers"][0]["shape"]["c"] == [58.5, 177.5]

    w_head = sc[120]["layers"][0]["shape"]["w"]
    w_row = sc[122]["layers"][0]["shape"]["w"]
    w_sub = sc[125]["layers"][0]["shape"]["w"]
    w_cell = sc[121]["layers"][0]["shape"]["w"]
    rep = {}
    # head: the first box's top line, no corner at the slab wire (the wire itself is 119, unchanged)
    rep[120] = F.wire_item(head_line(103.2), w_head)
    rep[2] = F.shadow_item(head_line(103.2), w_head)
    for top, wi, sh, ci, csh, li, lsh, cream in ((129, 122, 4, 121, 3, 151, 33, 64), (169, 124, 6, 123, 5, 152, 34, 77)):
        by = top + 16.07
        rep[wi] = F.wire_item([[SLAB_X, top], [SLAB_X, top + 17]], w_row)          # straight slab wire
        # the wire runs on through the strips above and below, so its shadow (moved 2 down) starts 2 above the strip
        rep[sh] = F.shadow_item([[SLAB_X, top - 2.0], [SLAB_X, top + 17]], w_row)
        cy = top + 16.0
        rep[ci] = F.wire_item([[112.0, cy], [153.0, cy]], w_cell)                  # next cell's bottom, from x 112
        rep[csh] = F.shadow_item([[112.0, cy], [153.0, cy]], w_cell)
        rep[li] = F.wire_item(row_frame(top, by), w_row)                           # the label box frame
        rep[lsh] = F.shadow_item(row_frame(top, by, True), w_row)                 # shadow right of the box (x 106)
        well = {"t": "rrect", "c": [58.5, top + 7.5], "half": [45.5, 7.5], "r": 0.0, "rot": 0.0}
        rep[cream] = well_item(top)
    # subtotal: straight slab wire, and the last box's bottom line as its own path
    rep[125] = F.wire_item([[SLAB_X, 420.0], [SLAB_X, 449.0]], w_sub)
    rep[7] = F.shadow_item([[SLAB_X, 418.0], [SLAB_X, 449.0]], w_sub)
    sub_bottom = row_frame(417.0, 421.1)
    out = []
    for i, it in enumerate(sc):
        if i in rep:
            if rep[i] is not None:
                out.append(rep[i])
        else:
            out.append(it)
        if i == 7:
            out.append(F.shadow_item(row_frame(417.0, 421.1, True), w_sub))
        if i == 125:
            out.append(F.wire_item(sub_bottom, w_sub))
    doc = F.deep(prev)
    doc["scene"] = out
    doc["round6"] = ("hole-row label boxes: own navy frame (round bottom-left), straight slab wire, sunken cream well "
                     "(well_item), next cell's line from x 112; head top line and subtotal bottom line likewise")
    return doc


def measure(scene, box, s=4):
    img = K.down(K.render(scene, box, s), s)
    return img.mean(axis=2)


def main():
    prev = F.approved(EID)
    doc = build(prev)
    legend_prev = F.approved("coursereport-legend")
    doc = S.apply_slab(doc, legend_prev["scene"])                 # item 3: the scenic icon (fix6_scenic.py)
    doc = T.apply_slab(doc, legend_prev["scene"])                 # item 4: the legend's connector stubs (fix6_stubs.py)
    F.save(EID, doc)
    legend = T.fix_legend(S.legend_doc())
    F.save("coursereport-legend", legend)
    art = C.art("coursereport.pcx")[0]
    L = measure(doc["scene"], (0, 126, 160, 150))
    A = art[126:150, 0:160].mean(axis=2)
    print("row y136  x4..19  art", " ".join("%3d" % v for v in A[10, 4:20]))
    print("          new", " ".join("%3d" % v for v in L[10, 4:20]))
    print("row y136 x96..115 art", " ".join("%3d" % v for v in A[10, 96:116]))
    print("          new", " ".join("%3d" % v for v in L[10, 96:116]))
    print("col x50 y129..145 art", " ".join("%3d" % v for v in A[3:20, 50]))
    print("          new", " ".join("%3d" % v for v in L[3:20, 50]))
    print("col x12 y129..145 art", " ".join("%3d" % v for v in A[3:20, 12]))
    print("          new", " ".join("%3d" % v for v in L[3:20, 12]))
    m = (A[3:20, 0:160] - L[3:20, 0:160])
    print("hole row strip x0..160 mean |d| %.1f" % np.abs(m).mean())
    rows = [("head bottom (first box's top line) 8x", (0, 90, 64, 104), 8),
            ("head bottom, box right end 8x", (60, 90, 124, 104), 8),
            ("hole row, label box left 8x", (0, 129, 64, 146), 8),
            ("hole row, label box right end and next cell 8x", (60, 129, 124, 146), 8),
            ("subtotal top (last box's bottom) 8x", (0, 416, 64, 430), 8),
            ("subtotal top, right end 8x", (60, 416, 124, 430), 8)]
    rows += [("hole row strip 2x", (x, 129, x + 200, 146), 2) for x in (0, 200, 400, 600)]
    rows += [("foot: Scenic Hole icon 12x", (416, 471, 452, 495), 12)]
    rows += [("foot: legend stub 8x", (x, 476, x + 30, 492), 8) for x in (110, 268, 420)]
    rows += [("foot 2x", (x, 468, x + 200, 541), 2) for x in (0, 200, 400, 600)]
    print(F.three_up(os.path.join(F.OUT6, EID + ".png"), "coursereport.pcx", prev["scene"], doc["scene"], rows, art))
    stacked = game_rows(prev["scene"], doc["scene"], art)
    print(stacked)
    # the legend element alone (the slab's foot under it, as both scenes are drawn in the game: legend, then slab foot)
    print(F.three_up(os.path.join(F.OUT6, "coursereport-legend.png"), "coursereport.pcx", legend_prev["scene"],
                     legend["scene"], [("legend, Scenic Hole icon 12x", (416, 471, 452, 495), 12),
                                       ("legend 2x", (108, 470, 323, 500), 2), ("legend 2x", (323, 470, 538, 500), 2)], art))


def game_rows(prev_sc, new_sc, art, n=5, s=4):
    """the head bottom, n hole rows and the subtotal stacked the way the game composes them (x 0..130): art | prev | new"""
    from PIL import Image
    def stack(src):
        parts = [src(0, 84, 130, 104)] + [src(0, 129, 130, 146)] * n + [src(0, 420, 130, 449)]
        top = np.concatenate(parts, axis=0)
        foot = [src(x, 468, x + 130, 500) for x in (100, 258, 410)]      # the three legend entries (game: the footer)
        return np.concatenate([top] + foot, axis=0)
    A = stack(lambda x0, y0, x1, y1: np.kron(art[y0:y1, x0:x1], np.ones((s, s, 1))))
    P = stack(lambda *b: K.render(prev_sc, b, s))
    N = stack(lambda *b: K.render(new_sc, b, s))
    gap = np.zeros((A.shape[0], 8, 3))
    gap[:] = (20, 20, 20)
    im = Image.fromarray(K.to8(np.concatenate([A, gap, P, gap, N], axis=1)))
    p = os.path.join(F.OUT6, EID + "--stacked.png")
    im.save(p)
    return p


if __name__ == "__main__":
    main()
