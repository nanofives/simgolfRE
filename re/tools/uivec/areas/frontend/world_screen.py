"""P41 frontend area: the world map / course action menu (WorldBase.pcx, WorldButton.pcx).

Measured: the plates and capsule boxes (boxes.py over WorldBase), the capsule's navy cap (a rounded rectangle read off a dump:
left side a half circle of radius 22), the oval round the top-right plate (an ellipse fitted to its edge), the bubbles
(title.measure_bubble / measure_body; socket ring measured), the Back disc. Authored from dumps (2026-11-24): the refresh
glyph (two arcs of one circle, each with a tapered tail and a triangular head, half a turn apart), the floppy disk and the
open folder. The globe and the site markers stay pictures.

Usage: py -3.12 re/tools/uivec/areas/frontend/world_screen.py
"""
import json

import numpy as np

import common as c
import parts
from common import fit, sdf

BASE = "Interface/WorldBase.pcx"
BTN = "Interface/WorldButton.pcx"
NAVY = [29, 36, 70]


def U(*s):
    return {"t": "union", "of": list(s)}


def box(x0, y0, x1, y1, r=0.0):
    return parts.rr(x0, y0, x1, y1, r)


def poly(*pts):
    return {"t": "poly", "rings": [[list(p) for p in pts]]}


def arc_taper(cx, cy, r, a0, a1, w0, w1):
    """Part of a circle's line from angle a0 to a1 whose width goes from w0 to w1 (a quadratic curve through the arc's
    ends with its control point where their tangents cross; the arcs here span 35 degrees, where it stays on the circle
    within 0.1 px)."""
    t0, t1 = np.deg2rad(a0), np.deg2rad(a1)
    p0 = [cx + r * np.cos(t0), cy + r * np.sin(t0)]
    p2 = [cx + r * np.cos(t1), cy + r * np.sin(t1)]
    tm = (t0 + t1) / 2
    k = r / np.cos((t1 - t0) / 2)
    p1 = [cx + k * np.cos(tm), cy + k * np.sin(tm)]
    return {"t": "qstroke", "p": [[round(v, 2) for v in p0], [round(v, 2) for v in p1], [round(v, 2) for v in p2]], "w": [w0, w1]}


def refresh_glyph():
    """Two curved arrows round one circle (centre 286.0, 476.5; radius 14 to the line's middle; line 4.2 px), read off the
    dump: the left one from a tapered tail at the lower left (130 deg) up to a triangular head at the top pointing right; the
    right one from a tapered tail at the upper right (314 deg) down to a head at the bottom pointing left."""
    cx, cy, r, w = 286.0, 476.5, 14.0, 4.2
    left = U(arc_taper(cx, cy, r, 130.0, 165.0, 1.2, w), {"t": "arc", "c": [cx, cy], "r": r, "a0": 165.0, "sweep": 85.0, "w": w},
             poly((280.0, 457.2), (293.2, 463.4), (283.0, 468.0)))
    right = U(arc_taper(cx, cy, r, 314.0, 349.0, 1.2, w), {"t": "arc", "c": [cx, cy], "r": r, "a0": 349.0, "sweep": 79.0, "w": w},
              poly((277.3, 488.6), (290.0, 484.2), (290.0, 495.0)))
    return U(left, right)


def floppy_glyph():
    """A floppy disk read off the dump (bubble at 576.5, 487.5): 3 px sides, a shutter block across the top, a dark band
    across the middle, under it a line, a label block on the left and a thin bar on the right of a small window, a bottom line with
    its corners cut, and a notch at the top right."""
    return U(box(566.0, 477.0, 569.0, 496.0), box(583.0, 480.0, 586.0, 496.0), box(583.0, 477.0, 586.0, 478.0),
             box(570.0, 477.0, 582.0, 485.0), box(566.0, 486.0, 586.0, 490.0), box(571.0, 491.0, 582.0, 492.0),
             box(571.0, 492.0, 577.0, 496.0), box(580.0, 492.0, 581.0, 496.0), box(568.0, 496.0, 582.0, 497.0))


def folder_glyph():
    """An open folder read off the dump (bubble at 596.5, 429.5): the back (a tab, its top bar, its left side), the flap
    leaning left as it goes down, and the lid's curved line above, from the tab over to the right."""
    flap = poly((592.0, 430.4), (603.0, 430.4), (599.6, 438.0), (588.6, 438.0))
    # the back shows a light gap one pixel wide along the flap's top and left edges
    back = {"t": "sub", "a": U(box(589.0, 426.0, 595.0, 427.0), box(589.0, 427.0, 599.0, 429.0), box(588.0, 429.0, 591.0, 438.0)),
            "b": {"t": "grow", "of": flap, "d": 0.9}}
    lid = {"t": "qstroke", "p": [[594.0, 422.6], [602.0, 421.0], [603.0, 427.4]], "w": [2.0, 2.0]}
    return U(back, flap, lid)


def bx_item(b):
    import boxes as bx
    return bx.item(b)


def capsule_items(art, boxes):
    """Every region capsule: the navy cap (a rounded rectangle with a half circle of radius 22 on its left, from the cap's
    left edge to under the boxes) with its soft shadow to the lower right, then the cream top box and the tan bottom box."""
    creams = [b for b in boxes if b["fill"] == [255, 255, 239] and (b["box"][3] - b["box"][1]) == 26]
    tans = [b for b in boxes if b["fill"] == [220, 197, 155]]
    items = []
    for cb in creams:
        x0, y0, x1, y1 = cb["box"]
        tb = min(tans, key=lambda t: abs(t["box"][0] - x0) + abs(t["box"][1] - y1))
        # the cap: in the first capsule its left edge is at x 251 (58 px left of the boxes), top 13, bottom 57
        cap = box(x0 - 58.0, y0 - 1.0, x0 + 6.0, tb["box"][3] + 2.0, 22.0)
        L = [{"shape": {"t": "move", "of": cap, "d": [2.5, 3.5]}, "fill": [40, 44, 90], "soft": 3.0, "alpha": 0.45},
             {"shape": {"t": "move", "of": box(x0, y0, max(x1, tb["box"][2]), tb["box"][3], 6.0), "d": [2.5, 3.5]},
              "fill": [40, 44, 90], "soft": 3.0, "alpha": 0.45},
             {"shape": cap, "fill": NAVY}]
        it = {"name": f"capsule {x0},{y0}", "box": [x0 - 62, y0 - 6, x1 + 10, tb["box"][3] + 10], "layers": L}
        items.append(it)
        items.append(bx_item(tb))
        items.append(bx_item(cb))
    return items


def topright_plate(art, boxes):
    """The cream plate at the top right sits in a lilac oval: an ellipse fitted to the oval's edge against the cloud (it
    runs off the screen on the right), painted with a lit surface measured on its pixels, the plate as boxes.py found it."""
    Lm = fit.lum(art)
    x0, y0, x1, y1 = 650, 0, 800, 60
    sub = Lm[y0:y1, x0:x1]
    m = sub < 170
    pts = fit.edge_points(m) + [x0, y0]
    pts = pts[(pts[:, 0] < 795) & (pts[:, 1] > 1) & (pts[:, 0] > 652)]
    E = parts.ellipse_of(pts, rot=0.0, tol=0.04)
    o = {"t": "ellipse", "c": E[:2], "r": E[2:4], "rot": 0.0}
    X, Y = parts.grid(x0, y0, x1, y1)
    pl = [b for b in boxes if b["box"][:2] == [683, 15]][0]
    px0, py0, px1, py1 = pl["box"]
    mm = (sdf.sd(o, X, Y) < -0.8) & ~((X > px0 - 3) & (X < px1 + 3) & (Y > py0 - 3) & (Y < py1 + 3))
    paint = parts.bevel_fit(art[y0:y1, x0:x1], o, mm, x0, y0, 12.0, [0.0, 0.1, 0.25, 0.5, 1.0])
    return [{"name": "oval", "box": [x0, y0, x1, y1], "layers": [{"shape": o, "fill": paint, "soft": 0.8}]}, bx_item(pl)], E


def main():
    import load
    import boxes as bx
    base, _ = c.sheet(BASE)
    bidx, bpal = fit.load_sheet(BASE)
    btn, _ = c.sheet(BTN)
    widx, wpal = fit.load_sheet(BTN)
    boxes, _ = bx.measure(bpal[bidx])
    load.setup()                     # Load Game's measured glass (style_load.json) for the bubbles

    # ---- bubbles on the WorldButton sheet: idle and hover are separate cells
    specs = [("world-refresh", (0xfa, 0x198, 0x4c, 0x72), (0x2d4, 0x198), (287.5, 477.0, 25.5), refresh_glyph(),
              "two curved arrows round one circle, each a tapered tail, a 4.2 px arc and a triangular head, half a turn apart"),
             ("world-save", (0x226, 0x1ca, 0x4c, 0x40), (0x190, 0x1ca), (576.5, 487.5, 17.5), floppy_glyph(),
              "a floppy disk from its parts: 3 px sides, shutter block, middle band, label block, window bar, bottom line"),
             ("world-load", (0x226, 0x198, 0x4c, 0x32), (0x190, 0x198), (596.5, 429.5, 16.5), folder_glyph(),
              "an open folder from its parts: back with tab, a flap leaning left, the lid's curved line above")]
    allsc = []
    for eid, cell, hcell, hint, glyph, how in specs:
        x, y, w, h = cell
        dx = hcell[0] - x
        it_i, info_i = parts.bubble_button(widx, wpal, btn, cell, hint, glyph, "blue", socket_art=btn)
        hc = (hcell[0], hcell[1], w, h)
        # where the hover drawing sits relative to the idle one: the shift (within 3 px) that best overlaps their dark
        # pixels (rim, socket line, glyph), the bubble's measured idle centre moved by it
        Lb = fit.lum(btn)
        di = Lb[y:y + h, x:x + w] < 70
        best = None
        for sy in range(-3, 4):
            for sx in range(-3, 4):
                dh = Lb[y + sy:y + sy + h, hcell[0] + sx:hcell[0] + sx + w] < 70
                if dh.shape != di.shape:
                    continue
                v = (di & dh).sum() / max((di | dh).sum(), 1)
                if best is None or v > best[0]:
                    best = (v, sx, sy)
        # the hover bubble is the idle one moved by the cell offset and the measured shift (its own circle fit on the
        # yellow cell's soft rim drifted by up to 1.4 px)
        it_h = parts.moved_bubble(it_i, btn, info_i, dx + best[1], best[2], "yellow", glyph)
        info_h = {"shift": [dx + best[1], best[2]], "overlap": round(float(best[0]), 3)}
        print(eid, info_i, info_h)
        sc = c.element(eid, [{"name": "idle", "art": btn, "scene": [it_i], "rect": cell},
                             {"name": "hover", "art": btn, "scene": [it_h], "rect": hc}],
                       f"glass bubble in its socket ring (circle fitted to the rim, body measured, socket measured, Load Game's "
                       f"glass paint) with {how}; hover the same shapes in the yellow paint",
                       extra={"measured": {"idle": info_i, "hover": info_h}},
                       unsure="the glass paint is Load Game's; the glyph is drawn in the rim's navy, the art's has a lighter "
                              "edge on its lit side")
        print(eid, sc)
        allsc.append(it_i)

    # ---- back: matte disc, a block arrow read off the dump
    arrow = {"t": "arrow", "a": [781.0, 557.8], "b": [755.8, 557.8], "w": 10.0, "hl": 12.0, "hw": 18.0}
    cell = (0x2dc, 0x214, 0x44, 0x44)
    out, dinfo = parts.disc_button({"idle": base, "hover": btn}, cell, arrow)
    print("world back", dinfo)
    sc = c.element("world-back", [{"name": "idle", "art": base, "scene": [out["idle"]], "rect": cell},
                                  {"name": "hover", "art": btn, "scene": [out["hover"]], "rect": cell}],
                   "matte lilac disc (circle fitted to its outline, lit paint measured per state) with a block arrow "
                   "pointing left (shaft 10 px, head 12 long and 18 tall, read off a dump); navy idle, yellow on hover")
    print("world-back", sc)
    allsc.append(out["idle"])

    # ---- capsule and plates
    caps = capsule_items(base, boxes)
    first = caps[:3]
    sc = c.element("world-capsule", [{"name": "idle", "art": base, "scene": first, "rect": (244, 8, 186, 56)}],
                   "a navy cap (rounded rectangle, a half circle of radius 22 on the left) with a soft shadow to the lower "
                   f"right, a cream top box and a tan bottom box in navy lines (boxes.py); the same for all {len(caps) // 3} "
                   "capsules (all in the backdrop's scene)", z=3)
    print("world-capsule", sc)
    title_p = [bx_item(b) for b in boxes if b["box"][:2] == [18, 15]]
    bottom_p = [bx_item(b) for b in boxes if b["box"][:2] == [17, 546]]
    tr, E = topright_plate(base, boxes)
    sc = c.element("world-plates", [{"name": "title", "art": base, "scene": title_p, "rect": (12, 10, 222, 44)},
                                    {"name": "bottom", "art": base, "scene": bottom_p, "rect": (12, 540, 360, 46)},
                                    {"name": "top right", "art": base, "scene": tr, "rect": (650, 0, 150, 58)}],
                   "flat rounded plates in navy lines thin on top and thick below (boxes.py): lilac title, cream bottom, and "
                   "a cream plate in a lilac oval (an ellipse fitted to its edge, lit paint measured)", z=2,
                   extra={"oval": E})
    print("world-plates", sc)
    allsc += caps + title_p + bottom_p + tr
    sc = c.element("world-backdrop", [{"name": "idle", "art": base, "scene": allsc[3:], "rect": (0, 0, 800, 600)}],
                   "the whole screen: the cloud, the golf-ball globe and the wires kept as texture (globe as Mariano asked), "
                   "the capsules, plates, the oval and the back disc drawn over it", z=1,
                   unsure="the wires between the capsules and the big lilac shapes on the right are texture")
    print("world-backdrop", sc)
    for k in ("parkland", "desert", "tropical", "links"):
        s, _ = c.sheet(f"Interface/Choose{k.capitalize()}Buttons.pcx")
        c.kept(f"site-plate-{k}", f"the {k} site marker (bubble with the emblem picture) stays art, as marked", s, (0, 0, 800, 80))


if __name__ == "__main__":
    main()
