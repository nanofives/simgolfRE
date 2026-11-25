"""P41 popups area: the frame pieces and flat fields (popup-border, trans-frame, popup-ok-frame, trans-bottom-strip,
trans-capsule, trans-text-field).

A nine-piece frame is drawn ONCE as the assembled box (a rounded rectangle whose edge lines are read side by side from the
art: sdf.py's bevel paint solved from the four sides' pixel rows, paint.bevel_from_bands) and then cut into the sheet's
pieces: piece (i, j) is the assembled box's square [16 i, 16 i + 16) x [16 j, 16 j + 16) moved to the sheet cell
(x0 + 17 i, y0 + 17 j) and clipped to it (the sheet keeps one pixel between pieces). Only the corner radius is tuned
(scored over the corner pieces); band colours are read, not fitted.

Usage: py -3.12 frames.py [id ...]
"""
import copy
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import common as c      # noqa: E402
import paint            # noqa: E402

BLACK = np.array((0.0, 0.0, 0.0))


def px(rel, x, y):
    rgb, m, _ = c.sheet(rel)
    return [float(v) for v in rgb[y, x]]


def sides(rel, box, n, mid=None):
    """The n pixel rows of each side from the outer edge inward, read at the middle of the side: (top, bottom, left, right)."""
    x0, y0, x1, y1 = box
    mx, my = mid or ((x0 + x1) // 2, (y0 + y1) // 2)
    top = [px(rel, mx, y0 + k) for k in range(n)]
    bot = [px(rel, mx, y1 - 1 - k) for k in range(n)]
    lef = [px(rel, x0 + k, my) for k in range(n)]
    rig = [px(rel, x1 - 1 - k, my) for k in range(n)]
    return top, bot, lef, rig


def moved(S, d):
    return {"t": "move", "of": S, "d": list(d)}


def pieces(layers, origin, n=(3, 3), size=16, pitch=17):
    """The assembled box's layers (assembled coordinates, box from (0, 0)) cut into the sheet's pieces at `origin`."""
    out = []
    for j in range(n[1]):
        for i in range(n[0]):
            dx, dy = origin[0] + (pitch - size) * i, origin[1] + (pitch - size) * j
            cx0, cy0 = origin[0] + pitch * i, origin[1] + pitch * j
            clip = c.rr(cx0, cy0, cx0 + size, cy0 + size, 0.0)
            for L in layers:
                L2 = copy.deepcopy(L)
                L2["shape"] = moved(L["shape"], (dx, dy))
                if isinstance(L2["fill"], dict) and "bevel" in L2["fill"] and "of" in L2["fill"]["bevel"]:
                    L2["fill"]["bevel"]["of"] = moved(L["fill"]["bevel"]["of"], (dx, dy))
                L2["clip"] = clip if "clip" not in L else {"t": "inter", "of": [clip, moved(L["clip"], (dx, dy))]}
                out.append(L2)
    return out


def ring_box(w, h, r, bands, fill=None, ring=None, rim_alpha=1.0):
    """A rounded box (0, 0)-(w, h): its edge lines from `bands` = (top, bottom, left, right), each listed from the outer pixel
    row inward; `fill` the inside colour, or `ring` = the edge's width when the inside is empty (a frame line only)."""
    rr = c.rr(0, 0, w, h, r)
    top, bot, lef, rig = bands
    P = paint.bevel_from_bands(top, bot, lef, rig, fill if fill is not None else top[-1])
    if ring:
        shape = {"t": "sub", "a": rr, "b": {"t": "grow", "of": rr, "d": -ring}}
        P["bevel"]["of"] = rr
        return [{"shape": shape, "fill": P}]
    return [{"shape": rr, "fill": P}]


def best_radius(make, rel, box, ground, rs):
    best = None
    for r in rs:
        sc = make(r)
        mad, w16 = c.score(sc, rel, box, ground)
        if best is None or mad < best[1]:
            best = (r, mad, w16)
    return best


# ------------------------------------------------------------------------------------------------------------ elements
def nine_frame(eid, rel, origin, ground, fill_read=None, ring=None, nband=5, rs=np.arange(1.0, 9.01, 0.5), sets=None, r_read=None):
    """A frame of nine 16 px pieces at `origin` (sets: more origins with the same pieces)."""
    ox, oy = origin
    abox = (ox, oy, ox + 50, oy + 50)
    # the sides are read on the middle pieces (assembled middle = sheet middle piece)
    top = [px(rel, ox + 25, oy + k) for k in range(nband)]
    bot = [px(rel, ox + 25, oy + 49 - k) for k in range(nband)]
    lef = [px(rel, ox + k, oy + 25) for k in range(nband)]
    rig = [px(rel, ox + 49 - k, oy + 25) for k in range(nband)]
    fill = fill_read
    bands = (top, bot, lef, rig)

    def make(r):
        return [c.item(abox, pieces(ring_box(48, 48, r, bands, fill=fill, ring=ring), (ox, oy)), eid)]
    # the corner radius is read on the diagonal: where the outer edge crosses the corner pixel diagonal (k, k), r (1 - 1 / sqrt 2)
    # = k. A fitted radius comes out too large: the art's corner pixels were anti-aliased against black, and a rounder corner
    # hides those dark specks
    r = r_read if r_read is not None else best_radius(make, rel, abox, ground, rs)[0]
    scene = make(r)
    mad, w16 = c.score(scene, rel, abox, ground)
    e = c.edge_px(scene, rel, abox, ground)
    rows = [(scene, rel, abox, ground)]
    states = {"pieces": {"box": list(abox), "scene": scene}}
    for k, o in enumerate(sets or []):
        b = (o[0], o[1], o[0] + 50, o[1] + 50)
        sc = [c.item(b, pieces(ring_box(48, 48, r, bands, fill=fill, ring=ring), o), eid)]
        rows.append((sc, rel, b, ground))
        states[f"set {k + 2}"] = {"box": list(b), "scene": sc}
    p = c.compare(rows, eid, zoom_k=6 if not sets else 4)
    c.save_scene(eid, {"id": eid, "sheet": "Interface/" + rel, "corner_radius": r, "bands_outer_to_inner": {
        "top": top, "bottom": bot, "left": lef, "right": rig}, "states": states})
    return p, mad, w16, e, r


def popup_border():
    p, mad, w16, e, r = nine_frame("popup-border", "InfoButtons.pcx", (200, 0), c.FLAT, fill_read=[152, 152, 200], nband=6,
                                    r_read=3.1)
    c.record("popup-border", "done", f"a flat lilac box 48 x 48 with corner radius {r:.1f}: light lines on top (4 rows) and on "
             "the left (3-4 columns), the dark line below (5 rows, it doubles as the shadow) and on the right (5 columns), read "
             "side by side from the middle pieces; drawn once and cut into the nine 16 px pieces", mad, e,
             "the art's corner pixels were anti-aliased against black (dark specks at the four corners); the shapes are drawn "
             "on the sheet's transparent ground instead")
    return p


def trans_frame():
    p, mad, w16, e, r = nine_frame("trans-frame", "TransPopups.pcx", (0, 0), c.FLAT, ring=3.0, nband=3, r_read=6.1)
    c.record("trans-frame", "done", f"a frame line only (the window behind shows through): a rounded rectangle 48 x 48, corner "
             f"radius {r:.1f}, line 3 px lit from the upper left (top and left: light outer row, dark inner row; bottom and "
             "right: the reverse), read side by side; cut into the nine pieces", mad, e,
             "corner pixels anti-aliased against black in the art (dark specks outside the line) are not drawn")
    return p


def popup_ok_frame():
    p, mad, w16, e, r = nine_frame("popup-ok-frame", "Pop_UpOk.pcx", (0, 0), BLACK, fill_read=[255, 247, 231], nband=4,
                                   sets=[(0, 100), (0, 200), (0, 300)], r_read=6.1)
    c.record("popup-ok-frame", "done", f"a cream box (255, 247, 231) in the same 3 px lilac line as the translucent frame "
             f"(lit from the upper left), corner radius {r:.1f}; the four sets are pixel-identical in the sheet, so one "
             "drawing serves all four", mad, e, "")
    return p


def main():
    todo = sys.argv[1:] or ["popup-border", "trans-frame", "popup-ok-frame"]
    fns = {"popup-border": popup_border, "trans-frame": trans_frame, "popup-ok-frame": popup_ok_frame}
    for k, eid in enumerate(todo):
        p = fns[eid]()
        print(f"[{k + 1}/{len(todo)}] {eid} {p}", flush=True)


if __name__ == "__main__":
    main()
