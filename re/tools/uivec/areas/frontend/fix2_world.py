"""P41 fix round: the world map backdrop (WorldBase.pcx), Mariano: "shape of the text boxes to the right of the images of each
location is wrong".

Re-read (dump of the first capsule, x 396-430, rows 8-59): the cream and tan text boxes do not end in a straight, round-
cornered side. Their right end is ONE curve bulging to the right: x 420 at row 15, 423 from row 23 to row 41, back to 418 at
row 53, meeting the flat top and bottom lines in sharp corners. Each capsule's curve is fitted as a circle to the rightmost
navy pixel of every row (the wire's row left out); the boxes are the region left of that circle between the top line and the
bottom line, in a navy line (1 px on top, about 1.2 px on the curve and the bottom), the cream half above the 1 px navy
divider and the tan half below it, the soft shadow cast to the lower right by that same shape. The navy cap over their left
ends is the first round's.
"""
import numpy as np

import common as c
import fix2 as F
import fix2_bubbles as FB
import parts
import world_screen as W
from common import fit, sdf

BASE = W.BASE


def capsules(art, boxes):
    L = fit.lum(art)
    creams = [b for b in boxes if b["fill"] == [255, 255, 239] and (b["box"][3] - b["box"][1]) == 26]
    tans = [b for b in boxes if b["fill"] == [220, 197, 155]]
    items, info = [], []
    for cb in creams:
        x0, y0, x1, y1 = cb["box"]
        tb = min(tans, key=lambda t: abs(t["box"][0] - x0) + abs(t["box"][1] - y1))
        top, div, bot = y0 - 1.0, float(y1), tb["box"][3] + 1.0
        xr = max(x1, tb["box"][2])
        pts = []
        for y in range(int(top) + 2, int(bot) - 2):
            row = L[y, xr - 10:min(800, xr + 8)]
            dk = np.nonzero(row < 80)[0]
            if len(dk) == 0:
                continue
            k = dk[-1]
            run = 1
            while k - run >= 0 and row[k - run] < 80:
                run += 1
            if run > 3 or k >= len(row) - 1:          # the wire leaving the box, or something past the window
                continue
            pts.append((xr - 10 + k + 1.0, y + 0.5))
        pts = np.array(pts)
        cx, cy, R, res = fit.fit_circle(pts, trim=0.85)
        lw = 1.2
        outline = cb.get("outline", [33, 45, 82])
        # the circle only shapes the right end: left of its centre the boxes run straight on under the cap
        right = lambda rr_: F.U(F.circ(cx, cy, rr_), F.box(-100.0, -100.0, cx, 1000.0))
        outer = {"t": "inter", "of": [F.box(x0 - 2.0, top, 2000.0, bot), right(R)]}
        cream_in = {"t": "inter", "of": [F.box(x0 - 2.0, top + 1.0, 2000.0, div), right(R - lw)]}
        tan_in = {"t": "inter", "of": [F.box(x0 - 2.0, div + 1.0, 2000.0, bot - 1.17), right(R - lw)]}
        cap = W.box(x0 - 58.0, y0 - 1.0, x0 + 6.0, tb["box"][3] + 2.0, 22.0)
        lay = [{"shape": F.mv(cap, 2.5, 3.5), "fill": [40, 44, 90], "soft": 3.0, "alpha": 0.45},
               {"shape": F.mv(outer, 2.5, 3.5), "fill": [40, 44, 90], "soft": 3.0, "alpha": 0.45},
               {"shape": cap, "fill": W.NAVY},
               {"shape": outer, "fill": outline},
               {"shape": cream_in, "fill": cb["fill"]},
               {"shape": tan_in, "fill": tb["fill"]}]
        items.append({"name": f"capsule {x0},{y0}", "box": [x0 - 62, y0 - 6, cx + R + 8, bot + 8], "layers": lay})
        info.append({"cream": cb["box"], "tan": tb["box"], "edge_circle": [round(cx, 2), round(cy, 2), round(R, 2)],
                     "res": round(res, 3)})
    return items, info


def main():
    import boxes as bx
    base, _ = c.sheet(BASE)
    bidx, bpal = fit.load_sheet(BASE)
    boxes, _ = bx.measure(bpal[bidx])
    caps, info = capsules(base, boxes)
    for k in info[:3]:
        print(k)
    title_p = [W.bx_item(b) for b in boxes if b["box"][:2] == [18, 15]]
    bottom_p = [W.bx_item(b) for b in boxes if b["box"][:2] == [17, 546]]
    tr, E = W.topright_plate(base, boxes)
    save = F.deliver
    F.deliver = lambda *a, **k: None
    only = F._only
    F._only = lambda eid: True
    try:
        FB.world_back()
    finally:
        F.deliver = save
        F._only = only
    back = FB.BACKDROP["world-back"]
    scene = caps + title_p + bottom_p + tr + [back]
    F.deliver("world-backdrop", [{"name": "idle", "art": base, "scene": scene, "rect": (0, 0, 800, 600)}],
              "the whole screen: the cloud, the golf-ball globe and the wires kept as texture, the 16 capsules (navy cap, and "
              "text boxes whose right end is one curve fitted per capsule, cream over tan with a navy divider, cast shadow), "
              "the plates, the oval and the back disc drawn over it",
              "the text boxes' right end is now the art's curve bulging to the right (a circle fitted per capsule, sharp "
              "corners at the top and bottom lines) instead of a straight side with round corners; the back disc is the "
              "fixed one", z=1, extra={"capsules": info})
    # a close-up of two capsules at 5x beside the art (an extra picture, not a results row)
    if F._only("world-backdrop"):
        pick = [it for it, i in zip(caps, info) if i["cream"][1] in (14, 65)]
        save_r = c.result
        c.result = lambda *a, **k: None
        try:
            c.element("world-backdrop--capsules-closeup", [{"name": "close-up", "art": base, "scene": pick + [back],
                                                             "rect": (244, 6, 446, 100)}], "", z=3)
        finally:
            c.result = save_r


if __name__ == "__main__":
    main()
