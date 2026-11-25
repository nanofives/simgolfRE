"""P41 frontend area: the Load Game screen (Title_LoadGame.pcx + Title_LoadGame_MO.pcx).

The parent's work is reused read-only: the bubbles and the Back disc (re/tools/uivec/load.py through title.scene, with
load_geom.json / glyphs_load.json / style_load.json, and the grey cells through loaddis_*), the flat boxes
(log/roadmap/P41/load/boxes_scene.json) and the hand-read chrome (load_extras.scene()). New here: the preview frame, the two
trays behind the pills and the list with their wires, the hover paint of the scroll arrows and the backdrop composite.

Usage: py -3.12 re/tools/uivec/areas/frontend/load_screen.py
"""
import copy
import json
import os

import numpy as np

import common as c
import parts
from common import fit, sdf

BASE = "Interface/Title_LoadGame.pcx"
MO = "Interface/Title_LoadGame_MO.pcx"
P41 = os.path.join(c.ROOT, "log", "roadmap", "P41")


def jcopy(o):
    return json.loads(json.dumps(o))


def bubbles():
    """Scenes of ok / delete / back for idle, hover and disabled, from the parent's measured geometry and paint."""
    import title
    import load
    import recipes
    keep = copy.deepcopy(recipes.STYLE)
    load.setup()
    meas = title.measure(verbose=False)
    out = {}
    for n in ("ok", "delete", "back"):
        out[n] = {"idle": jcopy(title.scene(meas, False, only=(n,))), "hover": jcopy(title.scene(meas, True, only=(n,)))}
    load.setup_disabled()
    # the grey cells: the measured grey paint (style_loaddis) on the IDLE geometry. The grey cells' own circle fit came out
    # 1 px larger with a body 2 px smaller and a grey rim, which the art does not show (it is the same bubble, greyed), so
    # the bubble keeps the idle circle, body and glyph, and its rim colour is read on the grey cell's own rim pixels.
    st = copy.deepcopy(recipes.STYLE["bubble"])
    base, hover, dis, _ = arts()
    for n in ("ok", "delete"):
        g = meas[n]["idle"]
        cx, cy, r = g["c"]
        yy, xx = np.mgrid[0:600, 0:800]
        rr_ = np.hypot(xx + 0.5 - cx, yy + 0.5 - cy)
        ring = (rr_ > r - 1.4) & (rr_ < r - 0.3) & (fit.lum(dis) < 90)
        st["yellow"]["rim"] = [round(float(v), 1) for v in np.median(dis[ring], axis=0)]
        out[n]["disabled"] = [jcopy(recipes.bubble(cx, cy, r, "yellow", g.get("shape") or g["glyph"], style=st, body=g["body"]))]
    # the socket ring round OK and Delete, measured per state on that state's art (the MO cells repaint it)
    for n in ("ok", "delete"):
        cx, cy, r = meas[n]["idle"]["c"]
        for stt, art in (("idle", base), ("hover", hover), ("disabled", dis)):
            lay, info = parts.socket(art, cx, cy, r)
            item = out[n][stt][0]
            item["layers"] = lay + item["layers"]
            out[n]["socket"] = info
    recipes.STYLE.clear()
    recipes.STYLE.update(keep)
    return out


def arts():
    import load
    b_idx, b_pal = fit.load_sheet(BASE)
    base = b_pal[b_idx].astype(float)
    _, _, _, _, h_idx, h_pal = load.load()
    hover = base.copy()
    m = h_idx < 0xfe
    hover[m] = h_pal[h_idx][m]
    _, _, _, _, d_idx, d_pal = load.load_disabled()
    dis = base.copy()
    m = d_idx < 0xfe
    dis[m] = d_pal[d_idx][m]
    full_mo = c.over(BASE, MO)
    return base, hover, dis, full_mo


def boxes_in(rect, boxes, scene):
    x, y, w, h = rect
    out = []
    for e, it in zip(boxes, scene):
        x0, y0, x1, y1 = e["box"]
        if x0 >= x - 1 and y0 >= y - 1 and x1 <= x + w + 1 and y1 <= y + h + 1:
            out.append(it)
    return out


def preview_frame(art):
    """The oval frame round the preview window: a lit outer band, a thin navy line, a sunken lavender field round a black
    window whose top is an arc, a pale sill under the window, all cut flat by the tray at the bottom."""
    Lm = fit.lum(art)
    x0, y0, x1, y1 = 14, 0, 238, 119
    X, Y = parts.grid(x0, y0, x1, y1)
    sub = Lm[y0:y1, x0:x1]
    # the navy line: its pixel centres (dark, not the black window), fitted to an ellipse
    line = (sub < 62) & (sub > 8) & (Y < 116)
    j, i = np.nonzero(line)
    E_line = parts.ellipse_of(np.c_[i + 0.5 + x0, j + 0.5 + y0], rot=0.0, tol=0.04)
    # the outer band: a ring of constant width round the line (rays from the line's centre, 2026-11-24: 11 px at every
    # angle, its top lit almost to the cloud's colour, its sides and bottom dark), so the line's ellipse grown by it
    cx, cy, a, b, _ = E_line
    widths = []
    for deg in range(180, 361, 5):
        t = np.deg2rad(deg)
        ex, ey = cx + a * np.cos(t), cy + b * np.sin(t)
        for k in np.arange(1.0, 16.0, 0.25):
            px, py = ex + k * np.cos(t), ey + k * np.sin(t)
            if py < 0.5:
                break
            if Lm[int(py), int(px)] > 196:
                if deg <= 200 or deg >= 340:
                    widths.append(k)
                break
    band = round(float(np.median(widths)), 2) if widths else 11.0
    E_out = [cx, cy, a + band, b + band, 0.0]
    # the black window: its sides and bottom are straight, its top follows an arc
    blk = sub < 4
    j, i = np.nonzero(blk)
    wx0, wx1, wy1 = i.min() + x0, i.max() + 1 + x0, j.max() + 1 + y0
    top = fit.edge_points(blk) + [x0, y0]
    top = top[(top[:, 1] < j.min() + y0 + 12) & (top[:, 0] > wx0 + 1) & (top[:, 0] < wx1 - 1)]
    E_win = parts.ellipse_of(top, rot=0.0, tol=0.08)
    sill_y1 = 119.0
    clip = {"t": "half", "p": [0, sill_y1], "n": [0, 1]}         # nothing below the tray's top
    ln = {"t": "ellipse", "c": E_line[:2], "r": E_line[2:4], "rot": 0.0}
    o = {"t": "grow", "of": ln, "d": band}
    win = {"t": "inter", "of": [parts.rr(wx0, 0, wx1, wy1, 0.0), {"t": "ellipse", "c": E_win[:2], "r": E_win[2:4], "rot": 0.0}]}
    # paint: the outer band and the field measured as lit surfaces (accept.solve_bevel) on the art's own pixels
    ring_mask = (sdf.sd(o, X, Y) < -0.6) & (sdf.sd(ln, X, Y) > 1.4) & (Y < 114)
    field_mask = (sdf.sd(ln, X, Y) < -1.4) & (sdf.sd(win, X, Y) > 0.8) & (Y < 113)
    crop = art[y0:y1, x0:x1]
    ring_paint = parts.bevel_fit(crop, o, ring_mask, x0, y0, band, [0.0, 0.15, 0.3, 0.5, 0.7, 0.85, 1.0])
    # the field: a lit surface measured against the depth inside the line (it is shaded for 3 px inside it), and the
    # shadow round the sunken window: 6 px wide, from 85 (sides) / 63 (top) at the window's edge to the field's 109
    field_mask &= sdf.sd(win, X, Y) > 6.5
    field_paint = parts.bevel_fit(crop, ln, field_mask, x0, y0, 12.0, [0.0, 0.1, 0.25, 0.5, 1.0])
    sill = (Y > wy1) & (Y < sill_y1 - 0.5) & (sdf.sd(ln, X, Y) < -1.2)
    sill_rgb = [round(float(v), 1) for v in np.median(crop[sill], axis=0)]
    layers = [
        {"shape": o, "fill": ring_paint, "clip": clip},
        {"shape": {"t": "outline", "of": ln, "w": 1.6}, "fill": [33, 44, 80], "clip": clip},
        {"shape": {"t": "grow", "of": ln, "d": -0.8}, "fill": field_paint, "clip": clip},
        {"shape": {"t": "inter", "of": [{"t": "grow", "of": ln, "d": -0.8}, parts.rr(0, wy1, 800, sill_y1, 0.0)]},
         "fill": sill_rgb},
        {"shape": {"t": "move", "of": {"t": "grow", "of": win, "d": 2.6}, "d": [-1.8, -1.0]}, "fill": [64, 64, 100], "soft": 3.2,
         "alpha": 0.85,
         "clip": {"t": "grow", "of": ln, "d": -0.8}},
        {"shape": win, "fill": [0, 0, 0]},
    ]
    info = {"band": band, "outer": E_out, "line": E_line, "window_arc": E_win, "window": [int(wx0), int(wx1), int(wy1)]}
    return {"name": "preview frame", "box": [x0, y0, x1, y1 + 1], "layers": layers}, info


def main():
    base, hover, dis, full_mo = arts()
    bx = json.load(open(os.path.join(P41, "load", "boxes_scene.json")))
    boxes, bscene = bx["boxes"], bx["scene"]
    import load_extras
    ex = {it["name"]: it for it in load_extras.scene()}
    bub = bubbles()
    done = []

    # ---- bubbles and the back disc (parent's measured recipes)
    for n, eid, how in (("ok", "load-ok-button", "glass bubble in a socket ring with an authored check mark (load.py / glyphs.py)"),
                        ("delete", "load-delete-button", "glass bubble with an authored open folder and X (load.py / glyphs.py)")):
        cx, cy, r = None, None, None
        it = bub[n]["idle"][0]
        x0, y0, x1, y1 = it["box"]
        rect = (int(x0) - 2, int(y0) - 2, int(x1 - x0) + 4, int(y1 - y0) + 4)
        st = [{"name": "idle", "art": base, "scene": bub[n]["idle"], "rect": rect},
              {"name": "hover", "art": hover, "scene": bub[n]["hover"], "rect": rect},
              {"name": "disabled", "art": dis, "scene": bub[n]["disabled"], "rect": rect}]
        sc = c.element(eid, st, how + "; hover yellow and disabled grey are the same shapes with the paint measured on "
                       "each state's own cells (style_load / style_loaddis)")
        print(eid, sc)
        done.append((eid, bub[n]))
    it = bub["back"]["idle"][0]
    x0, y0, x1, y1 = it["box"]
    rect = (int(x0), int(y0), int(x1 - x0) + 1, int(y1 - y0) + 1)
    rect = (rect[0], rect[1], min(rect[2], 800 - rect[0]), min(rect[3], 600 - rect[1]))
    sc = c.element("load-back-button", [{"name": "idle", "art": base, "scene": bub["back"]["idle"], "rect": rect},
                                        {"name": "hover", "art": hover, "scene": bub["back"]["hover"], "rect": rect}],
                   "matte lilac disc (measured disc bevel) with an authored left arrow; hover turns the arrow yellow")
    print("load-back-button", sc)

    # ---- scroll arrows: hover is the same triangles one pixel up, painted the white of the MO cells
    Lb, Lh = fit.lum(base), fit.lum(hover)
    for k, names, rect in (("up", ("scroll up, first arrow", "scroll up, second arrow"), (776, 100, 20, 40)),
                           ("down", ("scroll down, first arrow", "scroll down, second arrow"), (776, 380, 20, 40))):
        x, y, w, h = rect
        jb, ib = np.nonzero(Lb[y:y + h, x:x + w] > 160)
        jh, ih = np.nonzero(Lh[y:y + h, x:x + w] > 160)
        dy = round(float(jh.mean() - jb.mean()), 1)
        dx = round(float(ih.mean() - ib.mean()), 1)
        light = hover[y:y + h, x:x + w][Lh[y:y + h, x:x + w] > 200]
        white = [round(float(v), 1) for v in np.median(light, axis=0)]
        idle = [jcopy(ex[nm]) for nm in names]
        hov = []
        for it in idle:
            it2 = jcopy(it)
            for L in it2["layers"]:
                L["shape"] = {"t": "move", "of": L["shape"], "d": [dx, dy]}
            it2["layers"][-1 if k == "up" else -1]["fill"] = white
            # the cream body is the last layer of an up arrow but not of a down one (a dark line is inserted before it)
            for L in it2["layers"]:
                if L.get("fill") == load_extras.CREAM:
                    L["fill"] = white
            hov.append(it2)
        sc = c.element(f"load-scroll-{k}", [{"name": "idle", "art": base, "scene": idle, "rect": rect},
                                            {"name": "hover", "art": hover, "scene": hov, "rect": rect}],
                       f"two plain cream triangles pointing {k} with a soft dark edge on the unlit side (load_extras.py); hover: "
                       f"the same triangles moved ({dx}, {dy}) px, painted the measured white {white}")
        print(f"load-scroll-{k}", sc, dx, dy)

    sc = c.element("load-scroll-track", [{"name": "idle", "art": base, "scene": [ex["scroll track"]], "rect": (774, 140, 22, 242)}],
                   "a 1 px light line round a slot the frame's colour, a dark line across its top (load_extras.py)")
    print("load-scroll-track", sc)
    sc = c.element("load-info-bar", [{"name": "idle", "art": base, "scene": [ex["info bar"]], "rect": (372, 430, 404, 66)}],
                   "dark navy rounded box in a 2 px light lilac line with a dark line under it (load_extras.py)", z=3)
    print("load-info-bar", sc)

    # ---- flat boxes (boxes.py) and the trays behind them
    title_plate = boxes_in((324, 33, 362, 38), boxes, bscene)
    sc = c.element("load-title-plate", [{"name": "idle", "art": base, "scene": title_plate, "rect": (320, 30, 370, 44)}],
                   "lilac stadium plate: fill rounded rectangle over its navy line, thin on top and thick below, with the "
                   "sunken plate's shaded pixels inside the line as nested boxes (boxes.py)", z=4)
    print("load-title-plate", sc)
    page = boxes_in((300, 104, 476, 314), boxes, bscene)
    sc = c.element("load-page", [{"name": "idle", "art": base, "scene": page, "rect": (300, 104, 476, 314)}],
                   "cream rounded page, its corner radius and edge measured (boxes.py)", z=2)
    print("load-page", sc)

    pills = boxes_in((22, 114, 210, 175), boxes, bscene)
    listb = boxes_in((20, 270, 212, 325), boxes, bscene)
    excl = c.shape_mask(pills + listb, 0, 0, 800, 600, 2.2)
    t1, info1 = parts.tray(base, (21.0, 128.9, 229.0, 275.0), r=9.5, exclude=excl)
    t2, info2 = parts.tray(base, (21.0, 274.0, 229.0, 593.0), r=9.5, exclude=excl)
    tray1 = {"name": "pills tray", "box": [17, 125, 233, 279], "layers": t1}
    tray2 = {"name": "list tray", "box": [17, 270, 233, 597], "layers": t2}
    # wires: thin dark lines between the pills (inside the tray) and the two connector lines from the frame down to it
    trmask = c.shape_mask([tray1], 0, 0, 800, 600, -2.2)
    trline = c.shape_mask([tray1, tray2], 0, 0, 800, 600, 0.6) & ~trmask & ~c.shape_mask([tray2], 0, 0, 800, 600, -2.2)
    segs = parts.wires(base, (24, 118, 227, 273), excl | trline | (~trmask & (np.mgrid[0:600, 0:800][0] > 128)))
    # only navy runs are wires; the dark band under the first row of pills (rows 137-140, read off a dump) is the shadow
    # that row casts on the tray, drawn as its own soft band
    segs = [s_ for s_ in segs if fit.lum(np.array(s_["rgb"])) < 50]
    band = {"shape": parts.rr(23.0, 136.6, 227.0, 140.6, 1.0), "fill": [60, 62, 92], "soft": 0.9}
    wires = {"name": "pills wires", "box": [20, 112, 232, 275], "layers": [band] + parts.seg_layers(segs)}
    sc = c.element("load-detail-pills", [{"name": "idle", "art": base, "scene": [tray1, wires] + pills, "rect": (18, 112, 214, 166)}],
                   "a dark rounded tray (navy line 2 px at the sides, 1 px on top; its fill a measured linear gradient) "
                   f"with {len(segs)} thin navy wire segments read off the pixel rows between the pills and the shadow band the first row casts, then the lilac and cream "
                   "pills as boxes.py measured them", z=3)
    print("load-detail-pills", sc, len(segs))
    sc = c.element("load-save-list", [{"name": "idle", "art": base, "scene": [tray2] + listb, "rect": (18, 270, 214, 327)}],
                   "a dark rounded tray (measured outline, linear fill) holding the cream and tan list stripes and their "
                   "hairlines as boxes.py measured them", z=2)
    print("load-save-list", sc)

    pf, pinfo = preview_frame(base)
    sc = c.element("load-preview-frame", [{"name": "idle", "art": base, "scene": [pf], "rect": (14, 0, 224, 122)}],
                   "an oval frame: a lit outer band (ellipse, measured lit-surface paint), a 1.6 px navy line (ellipse "
                   "fitted to its pixel centres), a sunken lavender field shaded toward the window, a black window with "
                   "straight sides and bottom and an arched top (rectangle cut by a fitted ellipse), a pale sill under it, "
                   "all cut flat by the tray's top", z=3,
                   unsure="the frame's two connector lines to the tray (x 40 and 226) are drawn with the pills' wires",
                   extra={"measured": pinfo})
    print("load-preview-frame", sc, pinfo)

    # ---- the backdrop: every shape of the screen over the art, the cloud and the arched frame kept as they are
    allsc = [pf, tray1, tray2, wires] + pills + listb + title_plate + page + [ex[k] for k in ex] + \
        bub["ok"]["idle"] + bub["delete"]["idle"] + bub["back"]["idle"]
    rest = [it for it in bscene if it not in pills + listb + title_plate + page]
    allsc += rest
    sc = c.element("load-backdrop", [{"name": "idle", "art": base, "scene": allsc, "rect": (0, 0, 800, 600)}],
                   "the whole screen: the cloud and the big arched navy frame (with the faint ball inside it) stay as the "
                   "texture, every element of the screen drawn over it as its own shapes", z=1,
                   unsure="the arched frame's lines are still texture, not shapes")
    print("load-backdrop", sc)
    json.dump({"scene": allsc}, open(os.path.join(c.OUT, "_load_screen_scene.json"), "w"))


if __name__ == "__main__":
    main()
