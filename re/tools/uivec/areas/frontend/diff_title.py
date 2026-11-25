"""P41 frontend area: the title menu's and the difficulty screen's elements still marked todo.

Title menu: the centre ellipse (a cream ellipse fitted to the cream's edge, a navy outline fitted to the line's pixel
centres, two stems read off the rows) and the backdrop composite (the title buttons and pads as the title pipeline draws
them, title_geom.json / glyphs_title.json / style.json, read-only). Difficulty: the title plate (boxes.py), the Back disc
(circle and paint measured, arrow read off a dump) and the backdrop composite (the faces and pads as diff.py draws them).

Usage: py -3.12 re/tools/uivec/areas/frontend/diff_title.py
"""
import copy
import json

import numpy as np

import common as c
import parts
from common import fit, sdf

T = "Interface/TitleBASE.pcx"
D = "Interface/TitleSelDiffUnSel.pcx"
DM = "Interface/TitleSelDiffMO.pcx"
NAVY = [33, 43, 79]


def centre_ellipse(art):
    Lm = fit.lum(art)
    x0, y0, x1, y1 = 60, 165, 740, 395
    sub = art[y0:y1, x0:x1]
    cream = (sub[..., 0] > 235) & (sub[..., 2] > 200) & (sub[..., 0] >= sub[..., 2])
    cream = fit.largest_inner_component(cream) if hasattr(fit, "largest_inner_component") else cream
    pts = fit.edge_points(cream) + [x0, y0]
    E_in = parts.ellipse_of(pts, rot=None, tol=0.02)
    j, i = np.nonzero(Lm[y0:y1, x0:x1] < 70)
    lp = np.c_[i + x0 + 0.5, j + y0 + 0.5]
    cx, cy, a, b, rot = E_in
    t = np.deg2rad(rot)
    u = (lp[:, 0] - cx) * np.cos(t) + (lp[:, 1] - cy) * np.sin(t)
    v = -(lp[:, 0] - cx) * np.sin(t) + (lp[:, 1] - cy) * np.cos(t)
    k = np.hypot(u / a, v / b)
    E_line = parts.ellipse_of(lp[(k > 0.98) & (k < 1.06)], rot=None, tol=0.012)
    # the line's width: dark pixels across it on the top and bottom (rows through the centre column)
    # (read off dumps at the top, bottom and left: 2 px plus a blend on top, 3 px below, 2-3 px at the sides; the line is
    # thin on top and thick below, like the flat boxes' lines)
    w = 2.4
    # the stems: dark rows at the left and right edges of the screen
    ys = [y for y in range(y0, y1) if Lm[y, 20] < 120]
    sy0, sy1 = min(ys), max(ys) + 1
    xl = max(x for x in range(0, int(cx)) if Lm[(sy0 + sy1) // 2, x] < 120 and x < cx - E_line[2] + 3)
    cream_rgb = [round(float(v), 1) for v in np.median(art[int(cy) - 30:int(cy) + 30, int(cx) - 150:int(cx) + 150].reshape(-1, 3), axis=0)]
    # the line's own ellipse fit leans outward (its soft outer shadow is dark too); the line sits on the cream's edge:
    # navy from the cream's ellipse out by the line's width
    el = {"t": "ellipse", "c": E_in[:2], "r": [E_in[2] + w / 2, E_in[3] + w / 2], "rot": E_in[4]}
    sw = sy1 - sy0
    L = [{"shape": {"t": "seg", "a": [0.0, (sy0 + sy1) / 2], "b": [E_line[0], (sy0 + sy1) / 2], "w": float(sw)}, "fill": NAVY},
         {"shape": {"t": "seg", "a": [E_line[0], (sy0 + sy1) / 2], "b": [800.0, (sy0 + sy1) / 2], "w": float(sw)}, "fill": NAVY},
         {"shape": {"t": "move", "of": {"t": "grow", "of": el, "d": w / 2}, "d": [0.0, 0.45]}, "fill": NAVY},
         {"shape": {"t": "grow", "of": el, "d": -w / 2}, "fill": cream_rgb}]
    info = {"cream": E_in, "line": E_line, "line_w": w, "stem_rows": [sy0, sy1], "fill": cream_rgb}
    return {"name": "centre ellipse", "box": [0, y0 - 5, 800, y1 + 5], "layers": L}, info


def title_scene():
    import title
    import recipes
    keep = copy.deepcopy(recipes.STYLE)
    title.configure("title", title.CELLS, None, discs=("exit",))
    title.GEOM = __import__("os").path.join(c.UIVEC, "title_geom.json")
    title.GLYPHS = __import__("os").path.join(c.UIVEC, "glyphs_title.json")
    title.STYLE_FILE = __import__("os").path.join(c.UIVEC, "style.json")
    meas = title.measure(verbose=False)
    sc = json.loads(json.dumps(title.scene(meas, False)))
    recipes.STYLE.clear()
    recipes.STYLE.update(keep)
    return sc


def diff_scene():
    import title
    import diff
    import recipes
    keep = copy.deepcopy(recipes.STYLE)
    diff.setup() if hasattr(diff, "setup") else None
    meas = title.measure(verbose=False)
    sc = json.loads(json.dumps(title.scene(meas, False)))
    recipes.STYLE.clear()
    recipes.STYLE.update(keep)
    return sc


def main():
    # ---- title menu
    ta, _ = c.sheet(T)
    ce, info = centre_ellipse(ta)
    print(info)
    sc = c.element("title-centre-ellipse", [{"name": "idle", "art": ta, "scene": [ce], "rect": (0, 160, 800, 230)}],
                   f"a flat cream ellipse (fitted to the cream's edge) in a {info['line_w']} px navy line, thin on top and "
                   "thick below (read off dumps), with a 2 px navy stem along one row out to each screen edge", z=1,
                   extra={"measured": info})
    print("title-centre-ellipse", sc)
    tsc = title_scene()
    tua = c.over(T, "Interface/TitleUnSel.pcx")      # what the game shows: the backdrop with the idle buttons on it
    sc = c.element("title-backdrop", [{"name": "idle", "art": tua, "scene": [ce] + tsc, "rect": (0, 0, 800, 600)}],
                   "the title screen: the cloud backdrop and the giant golf ball stay the texture (Mariano: background the "
                   "same, just the shaped bubbles), with the centre ellipse and every pad, bubble and the exit disc drawn as "
                   "the title pipeline's shapes", z=1,
                   unsure="the Sim Golf logo (TitleMO) is drawn by the game over the ellipse and is not in this picture")
    print("title-backdrop", sc)
    for eid, how, sheet, rect in (("title-giant-ball", "the navy dimpled golf ball stays the picture (Mariano: art)", T, (150, 40, 510, 520)),
                                  ("title-publisher-logos", "the EA / Maxis / Firaxis marks stay the picture", T, (10, 505, 260, 85)),
                                  ("title-logo", "the green Sim Golf lettering stays the picture", "Interface/TitleMO.pcx", None),
                                  ("pict-backdrop", "the faint course map backdrop stays the picture", "Interface/Pictbckgrnd1.pcx",
                                   (0, 0, 800, 600))):
        if rect is None:
            c.kept(eid, how, c.over(T, sheet, (168, 198, 472, 176)), (168, 198, 472, 176))
        else:
            c.kept(eid, how, c.sheet(sheet)[0], rect)

    # ---- difficulty
    da, _ = c.sheet(D)
    idx, pal = fit.load_sheet(D)
    import boxes as bx
    boxes, _ = bx.measure(pal[idx])
    plate = [bx.item(b) for b in boxes if b["box"][:2] == [428, 39]]
    sc = c.element("diff-title-plate", [{"name": "idle", "art": da, "scene": plate, "rect": (420, 30, 365, 42)}],
                   "a long lilac stadium plate in a navy line thin on top and thick below, sunken (shaded pixels inside the "
                   "line), as boxes.py measured it", z=4)
    print("diff-title-plate", sc)
    arrow = {"t": "arrow", "a": [780.0, 557.8], "b": [754.8, 557.8], "w": 10.0, "hl": 11.0, "hw": 18.0}
    cell = (0x2dc, 0x214, 0x44, 0x44)
    hov = c.over(D, DM, cell)
    # the disc sits on the textured slab, so its circle is read off the dump (dark rim at x 748 on the left, light lips at
    # rows 538 and 581, right edge 786) instead of fitted to "differs from the ground"
    out, dinfo = parts.disc_button({"idle": da, "hover": hov}, cell, arrow, circle=(767.2, 559.8, 21.8))
    print("diff back", dinfo)
    sc = c.element("diff-back-button", [{"name": "idle", "art": da, "scene": [out["idle"]], "rect": (732, 528, 68, 68)},
                                        {"name": "hover", "art": hov, "scene": [out["hover"]], "rect": (732, 528, 68, 68)}],
                   "matte lilac disc (circle read off a dump, the slab under it is textured; lit paint measured per "
                   "state) with a block arrow pointing left (shaft 10 px, head 11 long and 18 tall, read off a dump); navy "
                   "idle, yellow on hover")
    print("diff-back-button", sc)
    try:
        dsc = diff_scene()
    except Exception as e:      # the difficulty pipeline is another session's; without it the faces stay out of the picture
        print("diff scene unavailable:", e)
        dsc = []
    sc = c.element("diff-backdrop", [{"name": "idle", "art": da, "scene": plate + [out["idle"]] + dsc, "rect": (0, 0, 800, 600)}],
                   "the difficulty screen: the cloud, the navy slab and the big ball stay the texture (Mariano: keep the "
                   "background), with the title plate, the Back disc and the four face bubbles and pads (diff.py) drawn over it",
                   z=1)
    print("diff-backdrop", sc)
    c.kept("diff-ball", "the large dimpled golf ball stays the picture", da, (530, 0, 270, 600))


if __name__ == "__main__":
    main()
