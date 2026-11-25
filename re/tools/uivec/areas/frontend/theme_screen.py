"""P41 frontend area: Theme Packs (Title_ThemePacks.pcx, hover / current-row cells in Title_ThemePacks_MO.pcx).

Measured here: the OK bubble's and the Back disc's circles (title.measure_bubble / measure_body, a circle fit to the rim),
the socket ring, the disc's lit paint, the cream name column (its curved left edge fitted to a circle row by row, its
hairlines read per row), the flat boxes (boxes.py over the sheet), the wires (thin dark runs), the arrows and the track (read
off dumps, numbers in the code). The OK glyph and paint are Load Game's (the same check mark on the same glass, moved to this
bubble's centre; the radii agree within half a pixel, printed when run).

Usage: py -3.12 re/tools/uivec/areas/frontend/theme_screen.py
"""
import copy
import json
import os

import numpy as np

import common as c
import parts
from common import fit, sdf

SHEET = "Interface/Title_ThemePacks.pcx"
MO = "Interface/Title_ThemePacks_MO.pcx"
CREAM = [239, 220, 171]
OK_CELL = (0x245, 0x208, 0x47, 0x50)
OK_MO = (0x109, 1)
BACK_CELL = (0x2d6, 0x215, 0x30, 0x31)
BACK_MO = (0x151, 1)
UP_CELL, UP_MO = (0x301, 0x51, 0x13, 0x23), (0x182, 1)
DN_CELL, DN_MO = (0x301, 0x1d7, 0x13, 0x23), (0x196, 1)


def jcopy(o):
    return json.loads(json.dumps(o))


def bx_item(b):
    import boxes as bx
    return bx.item(b)


def tri(tip, base_y, half, white=False, name=""):
    """A scroll arrow as Load Game's (load_extras.tri): a plain triangle, a soft dark edge on the unlit side, the light line
    under an up arrow's base or the dark line along a down arrow's base; white instead of cream when hovered."""
    import load_extras
    it = load_extras.tri(tip, base_y, half, name)
    if white:
        for L in it["layers"]:
            if L["fill"] == load_extras.CREAM:
                L["fill"] = [231, 239, 239]
    return it


def arrows(white=False):
    # read off dumps (2026-11-24): up tips at (778.5, 84) and (778.5, 100), bases at rows 97 / 113, half width 5;
    # down bases at rows 475 / 491, tips at (778.5, 487.6) and (778.5, 503.6), half width 4.8
    up = [tri((778.5, 84.0), 97.2, 5.0, white, "up 1"), tri((778.5, 100.0), 113.2, 5.0, white, "up 2")]
    dn = [tri((778.5, 487.6), 475.0, 4.8, white, "down 1"), tri((778.5, 503.6), 491.0, 4.8, white, "down 2")]
    return up, dn


def track():
    # a 1 px light line at columns 772 / 781 and rows 117 / 468.5, round a slot the frame's colour, a dark line on top
    o = parts.rr(772.0, 117.0, 782.0, 469.5, 1.0)
    return {"name": "track", "box": [768, 112, 786, 474], "layers": [
        {"shape": {"t": "stroke", "pts": [[772.6, 115.5], [781.4, 115.5]], "w": 1.0, "cap": "butt"}, "fill": [66, 70, 112]},
        {"shape": {"t": "outline", "of": o, "w": 1.0}, "fill": [189, 190, 214]}]}


def table(art, boxes):
    """The cream name column: its left edge is one arc (fitted to the edge row by row), its right edge straight against
    the first navy divider, top and bottom straight; hairlines across it every 32 rows; a soft lilac shading 4 px wide inside
    its curved edge and along its top. Then the tan cells as boxes.py measured them, and the dark lip above the table."""
    Lm = fit.lum(art)
    # the left edge: where the luminance crosses half way from the frame (110) to the cream (247), row by row with the
    # sub-pixel blend; the edge is nearly straight in the middle and turns in hard at both ends, so it is fitted with an
    # axis-aligned ellipse, not a circle
    pts = []
    for y in range(92, 507):
        row = Lm[y, 36:130]
        k = np.nonzero(row > 178)[0]
        if len(k) and k[0] > 0:
            x = 36 + k[0]
            a_, b_ = row[k[0] - 1], row[k[0]]
            pts.append((x - (b_ - 178) / max(b_ - a_, 1) + 0.5, y + 0.5))
    pts = np.array(pts, float)
    E = parts.ellipse_of(pts, rot=0.0, tol=0.03)
    top, bot, right = 90.0, 508.0, 294.0
    rect = parts.rr(30.0, top, right, bot, 0.0)
    # the column is the rectangle with its left side replaced by the ellipse's left half
    col = {"t": "inter", "of": [rect, {"t": "union", "of": [{"t": "ellipse", "c": E[:2], "r": E[2:4], "rot": 0.0},
                                                            parts.rr(E[0], top - 6, 800.0, bot + 6, 0.0)]}]}
    cream = [round(float(v), 1) for v in np.median(art[200:400, 150:250].reshape(-1, 3), axis=0)]
    # hairlines: rows where column 200 is dark inside the column, each starting where it does on the art
    lines = [y for y in range(int(top) + 2, int(bot) - 1) if Lm[y, 200] < 120]
    hl = []
    for y in lines:
        x0 = next(x for x in range(60, 200) if Lm[y, x] < 120)
        hl.append({"shape": {"t": "seg", "a": [x0, y + 0.5], "b": [right, y + 0.5], "w": 1.0},
                   "fill": [round(float(v), 1) for v in art[y, 150]]})
    # the edge is soft: the cream fades into the frame over about 3 px (read off the zoom)
    L = [{"shape": col, "fill": cream, "soft": 1.6}] + hl
    items = [{"name": "name column", "box": [30, top - 1, right + 1, bot + 1], "layers": L}]
    cells = [b for b in boxes if b["box"][1] >= 92 and b["box"][0] >= 300]
    items += [bx_item(b) for b in cells]
    return items, {"edge_ellipse": E, "hairlines": lines, "cream": cream}


def main():
    import title
    import load
    import recipes
    art, _ = c.sheet(SHEET)
    idx, pal = fit.load_sheet(SHEET)
    import boxes as bx
    boxes, _ = bx.measure(pal[idx])

    # ---- the OK bubble and the Back disc
    load.setup()
    lmeas = title.measure(verbose=False)
    cx, cy, r, res = title.measure_bubble(idx, pal, OK_CELL, near=(617.0, 556.0, 25.0))
    body = title.measure_body(art, cx, cy, r)
    lo = lmeas["ok"]["idle"]
    dx, dy = cx - lo["c"][0], cy - lo["c"][1]
    print("theme ok", round(cx, 2), round(cy, 2), round(r, 2), "load ok r", lo["c"][2], "body", body)
    glyph = {"t": "move", "of": lo["shape"], "d": [round(dx, 2), round(dy, 2)]}
    hov = c.over(SHEET, MO, (OK_MO[0], OK_MO[1], OK_CELL[2], OK_CELL[3]), OK_CELL[:2])
    st = copy.deepcopy(recipes.STYLE["bubble"])
    ok_idle = jcopy(recipes.bubble(cx, cy, r, "blue", glyph, style=st, body=body))
    ok_hov = jcopy(recipes.bubble(cx, cy, r, "yellow", glyph, style=st, body=body))
    for it, a in ((ok_idle, art), (ok_hov, hov)):
        lay, sinfo = parts.socket(a, cx, cy, r)
        it["layers"] = lay + it["layers"]
    rect = (OK_CELL[0], OK_CELL[1] - 4, OK_CELL[2], OK_CELL[3] - 2)
    sc = c.element("theme-ok-button", [{"name": "idle", "art": art, "scene": [ok_idle], "rect": rect},
                                       {"name": "hover", "art": hov, "scene": [ok_hov], "rect": rect}],
                   "glass bubble (circle fitted to its rim, body measured) with Load Game's authored check mark and paint, in "
                   f"a socket ring measured on this art (navy line at r {sinfo['R']}, lit band inside it); hover the same "
                   "shapes in the yellow paint", unsure=f"glyph and glass paint are Load Game's (radius {lo['c'][2]} there, "
                   f"{round(r, 2)} here)")
    print("theme-ok-button", sc)

    # the back disc: circle and paint measured here, the arrow authored from a dump (block arrow, tip left)
    Lm = fit.lum(art)
    x, y, w, h = BACK_CELL
    sub = Lm[y:y + h, x:x + w]
    # the disc against the frame: every pixel that differs from the frame colour by more than 6 levels, outermost on rays
    frame = np.array([103, 103, 159], float)
    m = np.abs(art[y:y + h, x:x + w] - frame).max(axis=2) > 10
    pts = fit.outermost(m, (w / 2, h / 2))
    dcx, dcy, dr, dres = fit.fit_circle(pts, trim=0.8)
    dcx, dcy = dcx + x, dcy + y
    print("theme back disc", round(dcx, 2), round(dcy, 2), round(dr, 2), round(dres, 3))
    arrow = {"t": "arrow", "a": [762.0, 556.0], "b": [737.0, 556.0], "w": 10.0, "hl": 11.0, "hw": 18.0}
    disc = {"t": "circle", "c": [dcx, dcy], "r": dr}
    bx0, by0 = int(dcx - dr - 2), int(dcy - dr - 2)
    bx1, by1 = int(dcx + dr + 3), int(dcy + dr + 3)
    X, Y = parts.grid(bx0, by0, bx1, by1)
    gm = sdf.sd(arrow, X, Y) > 1.3
    hova = c.over(SHEET, MO, (BACK_MO[0], BACK_MO[1], w, h), (x, y))
    out = {}
    for stn, a in (("idle", art), ("hover", hova)):
        dm = (sdf.sd(disc, X, Y) < -0.6) & gm
        paint = parts.bevel_fit(a[by0:by1, bx0:bx1], disc, dm, bx0, by0, dr, [0.0, 0.05, 0.12, 0.25, 0.45, 0.7, 1.0])
        g = a[by0:by1, bx0:bx1][sdf.sd(arrow, X, Y) < -1.0]
        grgb = [round(float(v), 1) for v in np.median(g, axis=0)]
        out[stn] = {"box": [bx0, by0, bx1, by1], "layers": [{"shape": disc, "fill": paint, "soft": 0.6},
                                                            {"shape": arrow, "fill": grgb, "clip": disc}]}
    rect = (x - 2, y - 2, w + 4, h + 4)
    sc = c.element("theme-back-button", [{"name": "idle", "art": art, "scene": [out["idle"]], "rect": rect},
                                         {"name": "hover", "art": hova, "scene": [out["hover"]], "rect": rect}],
                   "matte disc (circle fitted to its outline, lit paint measured on its own pixels: dark rim upper left, "
                   "light lip lower right) with a block arrow pointing left (shaft 10 px tall, head 11 long and 18 tall, "
                   "read off a dump); hover the same shapes with the paint measured on the MO cell, the arrow yellow")
    print("theme-back-button", sc)

    # ---- arrows and track
    up, dn = arrows()
    uph, dnh = arrows(True)
    for k, idl, hv, cell, mo in (("up", up, uph, UP_CELL, UP_MO), ("down", dn, dnh, DN_CELL, DN_MO)):
        x, y, w, h = cell
        ha = c.over(SHEET, MO, (mo[0], mo[1], w, h), (x, y))
        sc = c.element(f"theme-scroll-{k}", [{"name": "idle", "art": art, "scene": idl, "rect": (x - 1, y - 2, w + 2, h + 4)},
                                             {"name": "hover", "art": ha, "scene": hv, "rect": (x - 1, y - 2, w + 2, h + 4)}],
                       f"two plain triangles pointing {k}, 16 px apart, overlapping (Load Game's arrow: soft dark edge on the "
                       "unlit side and the line under the base); hover the same triangles in the MO cell's white")
        print(f"theme-scroll-{k}", sc)
    sc = c.element("theme-scroll-track", [{"name": "idle", "art": art, "scene": [track()], "rect": (766, 110, 22, 364)}],
                   "a 1 px light rounded outline round a slot the frame's colour, a dark line across its top (read off dumps)",
                   z=3)
    print("theme-scroll-track", sc)

    # ---- plates, header pills and their wires
    plate = [bx_item(b) for b in boxes if b["box"][:2] == [70, 30]]
    sc = c.element("theme-title-plate", [{"name": "idle", "art": art, "scene": plate, "rect": (62, 22, 210, 40)}],
                   "lilac rounded plate in a navy line thin on top and thick below, sunken (shaded pixels inside the line), "
                   "as boxes.py measured it", z=4)
    print("theme-title-plate", sc)
    hdr = [b for b in boxes if b["box"][1] in (39, 63)]
    hitems = [bx_item(b) for b in hdr]
    excl = c.shape_mask(hitems + plate, 0, 0, 800, 600, 2.0)
    segs = [s for s in parts.wires(art, (262, 30, 762, 80), excl, dark=72) if fit.lum(np.array(s["rgb"])) < 72]
    wires = {"name": "header wires", "box": [262, 30, 762, 80], "layers": parts.seg_layers(segs)}
    sc = c.element("theme-header-pills", [{"name": "idle", "art": art, "scene": [wires] + hitems, "rect": (266, 28, 498, 54)}],
                   f"five lilac pills in two staggered rows (boxes.py) joined by {len(segs)} navy wire segments read as runs "
                   "of dark pixels between them", z=3)
    print("theme-header-pills", sc, len(segs))

    # ---- the table
    tb, tinfo = table(art, boxes)
    print(tinfo)
    sc = c.element("theme-table", [{"name": "idle", "art": art, "scene": tb, "rect": (38, 82, 720, 432)}],
                   "a cream name column whose left edge is an ellipse arc (fitted to the edge row by row: nearly straight "
                   "in the middle, turning in hard at both ends; straight top, bottom and right) with 1 px hairlines every "
                   "32 rows and a soft edge, and five tan columns of flat cells split by navy lines (boxes.py)", z=2,
                   extra={"measured": tinfo})
    print("theme-table", sc)

    # ---- current row bar and cell highlight (MO cells, drawn where the game draws them: over the first row)
    mo, mm = c.sheet(MO)
    ext = {"row": (1, 0x21, 0xdd, 0x1f), "plain": (1, 1, 0x57, 0x1f), "lamp lit": (0x59, 1, 0x57, 0x1f),
           "lamp plain": (0xb1, 1, 0x57, 0x1f)}
    # on its own sheet the yellow is flat (boxes.py found the bar and the plain cell as flat (255, 214, 8) boxes)
    yb = [b for b in bx.measure(fit.load_sheet(MO)[1][fit.load_sheet(MO)[0]])[0]]
    for b in yb:
        print("MO box", b["box"], b["fill"], b.get("outline"))
    ground = np.empty(mo.shape)
    ground[:] = [70, 110, 90]
    moa = ground.copy()
    moa[mm] = mo[mm]
    # the extents boxes.py measured on the MO sheet (one pixel inside the cut rectangles)
    ext = {"row": (1, 34, 221, 30), "plain": (2, 2, 86, 30), "lamp lit": (90, 2, 86, 30), "lamp plain": (178, 2, 86, 30)}
    x, y, w, h = ext["row"]
    bar = {"name": "row bar", "box": [x - 2, y - 2, x + w + 2, y + h + 2], "layers": [
        {"shape": parts.rr(x, y, x + w, y + h, 0.0), "fill": [255, 214, 8]}]}
    sc = c.element("theme-row-bar", [{"name": "idle", "art": moa, "bg": ground, "scene": [bar], "rect": (x - 1, y - 1, w + 2, h + 2)}],
                   "a flat yellow rectangle (its colour and extent read on the MO sheet, boxes.py)", z=4)
    print("theme-row-bar", sc)
    cells = []
    lamp = lamp_shapes(moa, ext)
    for nm in ("plain", "lamp lit", "lamp plain"):
        x, y, w, h = ext[nm]
        L = [{"shape": parts.rr(x, y, x + w, y + h, 0.0), "fill": [255, 214, 8] if nm != "lamp plain" else
              [round(float(v), 1) for v in np.median(moa[y + 2:y + 6, x + 4:x + 20].reshape(-1, 3), axis=0)]}]
        if nm != "plain":
            L += lamp[nm]
        cells.append({"name": nm, "art": moa, "bg": ground, "rect": (x - 1, y - 1, w + 2, h + 2),
                      "scene": [{"name": nm, "box": [x - 2, y - 2, x + w + 2, y + h + 2], "layers": L}]})
    sc = c.element("theme-cell", cells, "a flat yellow cell; the lamp cells add a lamp: a green disc lit from the upper "
                   "left with a pale spot and a soft dark rim (radial paint measured on the lamp's pixels), on the yellow or "
                   "the plain tan ground", z=4)
    print("theme-cell", sc)

    # ---- the backdrop
    allsc = tb + plate + hitems + [wires, track(), ok_idle, out["idle"]] + up + dn
    sc = c.element("theme-backdrop", [{"name": "idle", "art": art, "scene": allsc, "rect": (0, 0, 800, 600)}],
                   "the whole screen: the navy frame and its curved line kept as texture, every element drawn over it",
                   z=1, unsure="the frame's long curved line round the table and its wire to the title plate are texture")
    print("theme-backdrop", sc)
    c.kept("logo-sheet", "the theme pack logo pictures stay the picture", c.sheet("logo.pcx")[0], (0, 0, 800, 600))


def lamp_shapes(moa, ext):
    """The green lamp of a lamp cell: the disc is the cell's green pixels (circle fitted to their outline); its paint a
    radial gradient whose stops are the measured mean colours at 0, 0.35, 0.7 and 1 of the radius from the light spot."""
    out = {}
    for nm in ("lamp lit", "lamp plain"):
        x, y, w, h = ext[nm]
        sub = moa[y:y + h, x:x + w]
        g = (sub[..., 1] > sub[..., 0] + 25) & (sub[..., 1] > sub[..., 2] + 25)
        pts = fit.outermost(g, (w / 2, h / 2))
        cx, cy, r, _ = fit.fit_circle(pts, trim=0.8)
        cx, cy = cx + x, cy + y
        yy, xx = np.mgrid[y:y + h, x:x + w]
        L = fit.lum(sub)
        inside = np.hypot(xx + 0.5 - cx, yy + 0.5 - cy) < r
        j, i = np.nonzero(inside & (L >= np.percentile(L[inside], 95)))
        sx, sy = i.mean() + x + 0.5, j.mean() + y + 0.5
        dist = np.hypot(xx + 0.5 - sx, yy + 0.5 - sy)
        far = float(dist[inside].max())
        stops = []
        for t in (0.0, 0.35, 0.7, 1.0):
            m = inside & (np.abs(dist / far - t) < 0.12)
            stops.append([t, [round(float(v), 1) for v in sub[m].mean(0)]])
        out[nm] = [{"shape": {"t": "circle", "c": [cx, cy], "r": r + 0.8}, "fill": [40, 70, 30], "soft": 1.0, "alpha": 0.4},
                   {"shape": {"t": "circle", "c": [cx, cy], "r": r}, "fill": {"radial": {"c": [sx, sy], "r": far, "stops": stops}}}]
    return out


if __name__ == "__main__":
    main()
