"""P41 popups area: the strip, capsule and text field of TransPopups.pcx (three-piece and one-piece flat chrome).

- trans-bottom-strip: three 16 px pieces (x 300 / 317 / 334, the sheet keeps one pixel between pieces) of one assembled strip
  48 px wide: a band 5 px tall (y 45-50, lit from above: five rows light to dark) with a taller rounded cap at each end (10 x 10,
  top corners r 2.5, lit from the upper left, measured with paint.fit_bevel); each cap's lower right melts into the band over
  about 4 px in the art, drawn as the cap's paint carried a few pixels on with a fading alpha (a falloff from the cap).
- trans-capsule: three pieces of an assembled white rounded box 48 x 19, corner radius 5 (read on the corner rows: insets
  2.8, 1.4, 0.7 px), a 2 px lilac edge read side by side (top / left: light outer, darker inner; bottom / right: the reverse)
  and one pale row inside it on the left and right.
- trans-text-field: one piece: a white oval knob (ellipse x 32-78, y 100-124, the same 2 px edge, its inside shaded toward the
  lower right: measured), a 3 px connector bar from the knob to the field (light, mid, dark rows), and the field: a white box
  x 83-222.8, y 102-121 with the same edge, corners r 5 on the left and r 7 on the right (read on the corner rows).
The art's outer corner pixels were anti-aliased against black (dark specks along the lower right edges); they are not drawn.

Usage: py -3.12 fields.py [id ...]
"""
import copy
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import common as c      # noqa: E402
import paint            # noqa: E402

TP = "TransPopups.pcx"


def px(x, y):
    rgb, m, _ = c.sheet(TP)
    return [float(v) for v in rgb[y, x]]


def mv(S, d):
    return {"t": "move", "of": S, "d": list(d)}


def pieces(layers, origin, n, size, pitch):
    out = []
    for i in range(n):
        dx = origin[0] + (pitch - size[0]) * i
        cx0 = origin[0] + pitch * i
        clip = c.rr(cx0, origin[1], cx0 + size[0], origin[1] + size[1], 0.0)
        for L in layers:
            L2 = copy.deepcopy(L)
            L2["shape"] = mv(L["shape"], (dx, origin[1]))
            if isinstance(L2["fill"], dict) and "bevel" in L2["fill"] and "of" in L2["fill"]["bevel"]:
                L2["fill"]["bevel"]["of"] = mv(L["fill"]["bevel"]["of"], (dx, origin[1]))
            if "falloff" in L2:
                L2["falloff"]["c"] = [L["falloff"]["c"][0] + dx, L["falloff"]["c"][1] + origin[1]]
            L2["clip"] = clip if "clip" not in L else {"t": "inter", "of": [clip, mv(L["clip"], (dx, origin[1]))]}
            out.append(L2)
    return out


def cap_shape(cx, cy, r, y1):
    """A cap: a round top (circle) on a square foot down to the band's bottom y1."""
    return {"t": "union", "of": [{"t": "circle", "c": [cx, cy], "r": r}, c.rr(cx - r, cy, cx + r, y1 + 6, 0.0)]}


def cap_paint(cx, cy, r, y1, piece_dx):
    """The cap's lit surface measured on its own pixels, lit like a dome: the toolkit's bevel paint on a circle 7 px in radius
    about (cx, cy + 1.5) that holds the whole cap, round top and square foot alike (radial directions, depth from that circle)."""
    x0, y0, x1 = int(cx - r), int(cy - r), int(cx + r)
    a = c.art(TP, (x0, y0, x1, y1))
    rgb, m, _ = c.sheet(TP)
    keep = m[y0:y1, x0:x1].copy()
    dome = {"t": "circle", "c": [cx, cy + 1.5], "r": 7.0}
    P, _ = paint.fit_bevel(dome, a, (x0, y0), [0.0, 0.12, 0.28, 0.48, 0.72, 1.0], 7.0, keep, radial_c=(cx, cy + 1.5))
    P["bevel"]["of"] = {"t": "circle", "c": [cx - 300 - piece_dx, cy + 1.5 - 34], "r": 7.0}
    return P


def strip():
    ox, oy = 300, 34
    band_rows = [px(325, y) for y in range(45, 50)]
    band = {"shape": c.rr(-6, 11, 54, 16, 0.0), "fill": paint.bevel_from_bands(band_rows[:3], band_rows[::-1][:3], [], [], band_rows[2])}
    Lc = cap_paint(305.0, 45.0, 5.0, 50, 0)
    Rc = cap_paint(345.0, 45.0, 5.0, 50, 2)     # the right cap sits in the third piece (2 px further than assembled)
    lcap = cap_shape(5.0, 11.0, 5.0, 16)      # assembled coordinates; the foot runs under the band's bottom and is clipped there
    rcap = cap_shape(43.0, 11.0, 5.0, 16)
    under = c.rr(-6, 6, 54, 16, 0.0)
    layers = [band,
              {"shape": lcap, "fill": Lc, "clip": under},
              {"shape": rcap, "fill": Rc, "clip": under},
              ]
    box = (ox, oy, ox + 50, oy + 18)
    scene = [c.item(box, pieces(layers, (ox, oy), 3, (16, 18), 17), "trans-bottom-strip")]
    mad, w16 = c.score(scene, TP, box)
    e = c.edge_px(scene, TP, box)
    p = c.compare([(scene, TP, box, c.FLAT)], "trans-bottom-strip", zoom_k=8)
    c.save_scene("trans-bottom-strip", {"id": "trans-bottom-strip", "sheet": "Interface/" + TP,
                                        "states": {"pieces": {"box": list(box), "scene": scene}}})
    c.record("trans-bottom-strip", "done", "an assembled strip 48 px wide cut in three pieces: a band 5 px tall lit from above "
             "(rows read on the middle piece) with a cap at each end: a round top r 5 on a square foot, lit like a ball from the "
             "upper left (measured on a 7 px dome that holds the whole cap)",
             mad, e, "in the art each cap melts into the band over about 4 px on its band side; here the cap ends at its own edge "
             "(a fading copy of its paint read as a dark smear and was dropped)")
    return p


def edge_bands(x_left, x_right, y_mid, y_top, y_bot, x_mid, inner=True):
    top = [px(x_mid, y_top), px(x_mid, y_top + 1)]
    bot = [px(x_mid, y_bot - 1), px(x_mid, y_bot - 2)]
    lef = [px(x_left, y_mid), px(x_left + 1, y_mid)] + ([px(x_left + 2, y_mid)] if inner else [])
    rig = [px(x_right - 1, y_mid), px(x_right - 2, y_mid)] + ([px(x_right - 3, y_mid)] if inner else [])
    return top, bot, lef, rig


def capsule():
    ox, oy = 0, 60
    fill = px(25, 70)
    top, bot, lef, rig = edge_bands(0, 50, 70, 60, 79, 25)
    rr = c.rr(0, 0, 48, 19, 5.0)
    layers = [{"shape": rr, "fill": paint.bevel_from_bands(top, bot, lef, rig, fill)}]
    box = (ox, oy, ox + 51, oy + 19)
    scene = [c.item(box, pieces(layers, (ox, oy), 3, (16, 19), 17), "trans-capsule")]
    mad, w16 = c.score(scene, TP, box)
    e = c.edge_px(scene, TP, box)
    p = c.compare([(scene, TP, box, c.FLAT)], "trans-capsule", zoom_k=8)
    c.save_scene("trans-capsule", {"id": "trans-capsule", "sheet": "Interface/" + TP, "states": {"pieces": {"box": list(box), "scene": scene}}})
    c.record("trans-capsule", "done", "an assembled white rounded box 48 x 19 (corner radius 5, read on the corner rows) with a "
             "2 px lilac edge read side by side (light outer and darker inner on top and left, the reverse below and right) and a "
             "pale inner row at both ends; cut into the three 16 px pieces", mad, e, "")
    return p


def text_field():
    box = (32, 100, 223, 124)
    fill = px(120, 110)
    # the knob: an ellipse with the 2 px edge; the inside's shading toward the lower right measured on its pixels
    kc, kr = (55.0, 112.0), (23.0, 12.0)
    knob = {"t": "ellipse", "c": list(kc), "r": list(kr)}
    kin = {"t": "ellipse", "c": list(kc), "r": [kr[0] - 2.0, kr[1] - 2.0]}
    a = c.art(TP, box)
    rgb, m, _ = c.sheet(TP)
    jj, ii = np.mgrid[box[1]:box[3], box[0]:box[2]]
    x, y = ii + 0.5, jj + 0.5
    keep = (c.sdf.sd(knob, x, y) < -0.3)
    Pk, _ = paint.fit_bevel(knob, a, box[:2], [0.5, 1.5, 2.5, 3.5, 5.0, 7.0, 9.0, 12.0], 1.0, keep)
    bar_rows = [px(80, 110), px(80, 111), px(80, 112)]
    # the field: left corners r 5, right corners r 7 (two rounded boxes intersected)
    fx0, fy0, fx1, fy1 = 83.0, 102.0, 222.8, 121.0
    field = {"t": "inter", "of": [c.rr(fx0, fy0, fx1 + 20, fy1, 5.0), c.rr(fx0 - 20, fy0, fx1, fy1, 7.0)]}
    top = [px(150, 102), px(150, 103)]
    bot = [px(150, 120), px(150, 119)]
    lef = [px(83, 112), px(84, 112), px(85, 112)]
    rig = [px(222, 112), px(221, 112), px(220, 112)]
    Pf = paint.bevel_from_bands(top, bot, lef, rig, fill)
    Pf["bevel"]["of"] = field
    layers = [
        {"shape": c.rr(76.0, 110.0, 85.0, 113.0, 0.0), "fill": paint.bevel_from_bands(bar_rows[:2], bar_rows[::-1][:2], [], [], bar_rows[1])},
        {"shape": knob, "fill": Pk},
        {"shape": field, "fill": Pf},
    ]
    scene = [c.item(box, layers, "trans-text-field")]
    mad, w16 = c.score(scene, TP, box)
    e = c.edge_px(scene, TP, box)
    p = c.compare([(scene, TP, box, c.FLAT)], "trans-text-field", zoom_k=4)
    c.save_scene("trans-text-field", {"id": "trans-text-field", "sheet": "Interface/" + TP, "states": {"idle": {"box": list(box), "scene": scene}}})
    c.record("trans-text-field", "done", "an oval knob (ellipse 46 x 24 with the 2 px lilac edge, its white inside shaded toward "
             "the lower right, measured), a 3 px connector bar (light / mid / dark rows) and the long white field (2 px edge read "
             "side by side, corners r 5 on the left and r 7 on the right, read on the corner rows)", mad, e,
             "the art's dark anti-aliasing specks against black along the lower right edges are not drawn")
    return p


def main():
    fns = {"trans-bottom-strip": strip, "trans-capsule": capsule, "trans-text-field": text_field}
    todo = sys.argv[1:] or list(fns)
    for k, eid in enumerate(todo):
        print(f"[{k + 1}/{len(todo)}] {eid} {fns[eid]()}", flush=True)


if __name__ == "__main__":
    main()
