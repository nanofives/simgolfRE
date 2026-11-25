"""P41 fix round: the Load Game screen (Title_LoadGame.pcx) and the Pick a Pro bottom bar.

Mariano's notes and what was re-read:
- load-info-bar "rounded corners": the corners are 6 px round, not 4.5 (dump rows 435-442: the light line's outer edge is at
  x 381, 379, 378, 377, 376 on rows 435-440). The line is 2 px in two tones (an outer lighter pixel and an inner one), the
  dark line under it is on the right (x 771) and below (row 491), and the bar's right edge is x 771, not 772.
- load-scroll-track "rounded corners": a slot with 2 px round corners: a 1 px light line round it (lighter along the bottom),
  a dark line down its left side (x 778) and across its top (row 143) that the first round left out, the slot the frame's
  colour.
- load-backdrop "some buttons rendered below": the bottom row of pills (rows 266-282) was covered by the list's cream
  background box (boxes.py's 'rest' boxes were drawn after the pills). Now drawn in the game's order: frame, trays, the
  other boxes, wires, list stripes, then the pills, the page, plates, scroll chrome, info bar and the bubbles on top.
"""
import json
import os

import numpy as np

import common as c
import fix2 as F
import parts
from common import fit, sdf

LG = "Interface/Title_LoadGame.pcx"


def col(a, x, y):
    return [round(float(v), 1) for v in a[y, x]]


def info_bar(a):
    return {"name": "info bar", "box": [372, 431, 776, 496], "layers": [
        {"shape": F.box(377.0, 436.0, 772.0, 492.0, 6.0), "fill": col(a, 500, 491)},
        {"shape": F.box(376.0, 435.0, 771.0, 491.0, 6.0), "fill": [round((196 + 174 + 201) / 3, 1), round((198 + 175 + 202) / 3, 1),
                                                                    round((233 + 204 + 217) / 3, 1)]},
        {"shape": F.box(377.0, 436.0, 770.0, 490.0, 5.0), "fill": [round((147 + 146 + 148) / 3, 1), round((150 + 153 + 148) / 3, 1),
                                                                    round((190 + 210 + 195) / 3, 1)]},
        {"shape": F.box(378.0, 437.0, 769.0, 489.0, 4.3), "fill": col(a, 500, 460)}]}


def track(a):
    light = [184.0, 181.0, 181.0]
    return {"name": "scroll track", "box": [775, 140, 793, 382], "layers": [
        {"shape": F.box(777.7, 142.8, 789.4, 378.4, 2.2), "fill": col(a, 778, 250)},
        {"shape": F.box(779.0, 144.0, 789.0, 379.0, 2.0), "fill": light},
        {"shape": F.box(779.0, 377.6, 789.0, 379.0, 1.4), "fill": col(a, 783, 378), "clip": F.box(779.0, 144.0, 789.0, 379.0, 2.0)},
        {"shape": F.box(780.0, 145.0, 788.0, 378.0, 1.2), "fill": col(a, 783, 250)},
        {"shape": F.box(780.0, 145.0, 788.0, 146.0), "fill": col(a, 783, 144), "alpha": 0.6}]}


def main():
    import load_screen as LS
    import load_extras
    base, hover, dis, full_mo = LS.arts()
    bx = json.load(open(os.path.join(LS.P41, "load", "boxes_scene.json")))
    boxes, bscene = bx["boxes"], bx["scene"]
    ex = {it["name"]: it for it in load_extras.scene()}
    ex["info bar"] = info_bar(base)
    ex["scroll track"] = track(base)

    F.deliver("load-info-bar", [{"name": "idle", "art": base, "scene": [ex["info bar"]], "rect": (372, 430, 404, 66)}],
              "dark navy box with 6 px round corners in a 2 px light lilac line (two tones: lighter outside), a dark line "
              "under it on the right and below", "corners 6 px round (were 4.5); right edge at x 771 as the art", z=3)
    F.deliver("load-scroll-track", [{"name": "idle", "art": base, "scene": [ex["scroll track"]], "rect": (774, 138, 22, 246)}],
              "a slot with 2 px round corners: a dark line down its left side and across its top, a 1 px light line round "
              "it, lighter along the bottom, the slot in the frame's colour",
              "2 px round corners (were 1 px); the dark left and top line the first round left out added", z=6)

    bub = LS.bubbles()
    title_plate = LS.boxes_in((324, 33, 362, 38), boxes, bscene)
    page = LS.boxes_in((300, 104, 476, 314), boxes, bscene)
    pills = LS.boxes_in((22, 114, 210, 175), boxes, bscene)
    listb = LS.boxes_in((20, 270, 212, 325), boxes, bscene)
    excl = c.shape_mask(pills + listb, 0, 0, 800, 600, 2.2)
    t1, _ = parts.tray(base, (21.0, 128.9, 229.0, 275.0), r=9.5, exclude=excl)
    t2, _ = parts.tray(base, (21.0, 274.0, 229.0, 593.0), r=9.5, exclude=excl)
    tray1 = {"name": "pills tray", "box": [17, 125, 233, 279], "layers": t1}
    tray2 = {"name": "list tray", "box": [17, 270, 233, 597], "layers": t2}
    trmask = c.shape_mask([tray1], 0, 0, 800, 600, -2.2)
    trline = c.shape_mask([tray1, tray2], 0, 0, 800, 600, 0.6) & ~trmask & ~c.shape_mask([tray2], 0, 0, 800, 600, -2.2)
    segs = parts.wires(base, (24, 118, 227, 273), excl | trline | (~trmask & (np.mgrid[0:600, 0:800][0] > 128)))
    segs = [s_ for s_ in segs if fit.lum(np.array(s_["rgb"])) < 50]
    band = {"shape": parts.rr(23.0, 136.6, 227.0, 140.6, 1.0), "fill": [60, 62, 92], "soft": 0.9}
    wires = {"name": "pills wires", "box": [20, 112, 232, 275], "layers": [band] + parts.seg_layers(segs)}
    pf, _ = LS.preview_frame(base)
    rest = [it for it in bscene if it not in pills + listb + title_plate + page]
    # the game's order: frame, trays, the remaining boxes (the list's cream ground among them), wires, list stripes, then
    # the pills over the list's top, the page and plates, the scroll chrome and info bar, the bubbles last
    allsc = [pf, tray1, tray2] + rest + [wires] + listb + pills + title_plate + page + [ex[k] for k in ex] + \
        bub["ok"]["idle"] + bub["delete"]["idle"] + bub["back"]["idle"]
    F.deliver("load-backdrop", [{"name": "idle", "art": base, "scene": allsc, "rect": (0, 0, 800, 600)}],
              "the whole screen in the game's order (frame, trays, list ground, wires, list stripes, pills, page, plates, "
              "scroll chrome, info bar, bubbles); the cloud and the big arched frame stay the texture",
              "the bottom row of pills is drawn over the list's ground (it was covered); the new info bar corners and "
              "scroll track are in", z=1)




def pro_bottom_bar():
    """Pick a Pro's bottom bar, Mariano: "bottom part of the rectangle looks like other rectangle with different underneath
    lightning". The bar is a 51 % black veil (alpha and colour fitted in the first round) over the backdrop, so what shows
    through it is the backdrop. The first round drew it over a background INPAINTED under the bar, where the fill coming down
    from above met the fill coming up from below in a seam two thirds of the way down, which read as a second rectangle. The
    veil is linear, so the backdrop under it is recovered exactly from the art itself: bg = (art - a C) / (1 - a) (checked:
    the recovered rows match the backdrop just above and below the bar to within 6 levels). The veil over that background is
    the art's lighting, one surface top to bottom. Added: the 1-row darker band along the inside of the top line (row 553,
    another 14 % of the veil's colour, read off the recovered rows)."""
    import pro_screen as P
    import boxes as bxm
    art, _ = c.sheet(P.SHEET)
    idx, pal = fit.load_sheet(P.SHEET)
    boxes, _ = bxm.measure(pal[idx])
    items = P.bottom_bar(art, boxes)
    bar = items[0]
    inner = [L for L in bar["layers"] if L.get("alpha") == 0.51][0]
    C = np.asarray(inner["fill"], float)
    a = inner["alpha"]
    bar["layers"].append({"shape": {"t": "inter", "of": [inner["shape"], F.box(0.0, 552.0, 800.0, 554.0)]},
                          "fill": inner["fill"], "alpha": 0.14})
    rect = (24, 534, 538, 50)
    x, y, w, h = rect
    X, Y = parts.grid(x, y, x + w, y + h)
    under = sdf.sd(inner["shape"], X, Y) < -0.6
    pill = c.shape_mask(items[1:], x, y, w, h, 1.0)
    bg = F.wiped(art, items, rect, grow=1.0)
    rec = (art[y:y + h, x:x + w] - a * C) / (1 - a)
    sub = bg[y:y + h, x:x + w]
    m = under & ~pill
    sub[m] = np.clip(rec[m], 0, 255)
    F.deliver("pro-bottom-bar", [{"name": "idle", "art": art, "bg": bg, "scene": items, "rect": rect}],
              "a long rounded bar: a navy line round a 51 % black veil over the backdrop (alpha and colour fitted by least "
              "squares) with a 1-row darker band inside the top line, under the lilac stadium pill (boxes.py); the backdrop "
              "under the veil is the art's own, recovered by undoing the veil",
              "the seam two thirds down is gone: the shapes are drawn over the true backdrop (recovered from the art by "
              "undoing the linear veil) instead of an inpainted one, so the bar reads as one surface; top inner band added",
              z=2)


if __name__ == "__main__":
    if F._only("pro-bottom-bar"):
        pro_bottom_bar()
    main()
