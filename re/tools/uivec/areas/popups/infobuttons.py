"""P41 popups area: the ten info buttons of InfoButtons.pcx (rows of 50 px, columns idle / hover / selected at x 0, 50, 100).

Each button = a lilac tab (flat box strip, cut at both cell sides, light lines on top, a dark line two-to-five px thick below)
+ a disc: navy rim circle over a body circle whose lit surface is measured per state (tan, lighter tan, yellow); hover adds
the disc's soft shadow on the tab. The glyph of each button is authored in glyphs_ib.py from the art read on its pixel grid.

Usage: py -3.12 infobuttons.py [key ...]      -> log/roadmap/P41/work/popups/info-button-<key>.png / .json, results.tsv rows
"""
import json
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import common as c      # noqa: E402
import paint            # noqa: E402

SHEET = "InfoButtons.pcx"
KEYS = ("flag", "notice", "pair", "pin", "money", "chart", "checklist", "trophy", "globe", "tiles")
STATES = ("idle", "hover", "selected")
W, H = 46, 40
LILAC = [152, 152, 200]
# the tab, read on column x=40 of every cell (rows from the cell top): 4 light rows from y 8, fill, 5 dark rows to y 39
TAB_Y0, TAB_Y1 = 8.0, 40.0
TAB_TOP = [[200, 200, 224], [200, 200, 224], [192, 192, 224], [176, 176, 216]]
TAB_BOT = [[64, 64, 88], [56, 56, 80], [80, 80, 104], [104, 112, 144], [136, 136, 184]]
BODY_STOPS = [0.0, 0.08, 0.18, 0.32, 0.5, 0.7, 1.0]


def cell(r, col):
    return (50 * col, 50 * r, 50 * col + W, 50 * r + H)


def tab_layer(box):
    x0, y0, x1, y1 = box
    sh = c.rr(x0 - 20, y0 + TAB_Y0, x1 + 20, y0 + TAB_Y1, 0.0)
    clip = c.rr(x0, y0 - 5, x1, y1 + 5, 0.0)
    return {"shape": sh, "fill": paint.bevel_from_bands(TAB_TOP, TAB_BOT, [], [], LILAC), "clip": clip}


def measure_disc(r, col):
    """(cx, cy, R) of the rim's outer edge: the outermost dark (rim) pixel along 120 rays, moved out by the half pixel a pixel
    centre sits inside its edge, circle-fitted; then the body (inner edge of the rim) from the first light pixel inward."""
    box = cell(r, col)
    a = c.art(SHEET, box)
    Lm = c.fit.lum(a)
    jj, ii = np.mgrid[0:H, 0:W]
    g = (17.0, 18.3)
    near = np.hypot(ii + 0.5 - g[0], jj + 0.5 - g[1]) < 19.5
    dark = near & (Lm < 75)
    pts = c.fit.outermost(dark, g, rays=120)
    cx, cy, R, res = c.fit.fit_circle(pts, trim=1.2)
    return cx + box[0], cy + box[1], R + 0.6


def base(r, col, disc, style):
    """The button without its glyph: tab, (hover) shadow, rim, body."""
    box = cell(r, col)
    cx, cy, R = disc
    Ls = [tab_layer(box)]
    if style.get("shadow"):
        s = style["shadow"]
        Ls.append({"shape": c.circ(cx + s[0], cy + s[1], R + s[2]), "fill": [40, 40, 80], "alpha": s[3], "soft": s[4],
                   "clip": c.rr(box[0], box[1] + TAB_Y0, box[2], box[1] + TAB_Y1, 0.0)})
    Ls.append({"shape": c.circ(cx, cy, R), "fill": style["rim"]})
    body = c.circ(cx, cy, R - style["rim_w"])
    Ls.append({"shape": body, "fill": style["body"]})
    return c.item(box, Ls), body


def measure_style(r, col, disc, glyph_mask=None, rim_w=1.9):
    """Rim colour (median of the pixels deep in the ring) and the body's lit surface (fit_bevel on the body, glyph excluded)."""
    box = cell(r, col)
    a = c.art(SHEET, box)
    cx, cy, R = disc
    jj, ii = np.mgrid[0:H, 0:W]
    x, y = ii + 0.5 + box[0], jj + 0.5 + box[1]
    rho = np.hypot(x - cx, y - cy)
    ring = (rho > R - rim_w + 0.45) & (rho < R - 0.45)
    rim = [round(float(v), 1) for v in np.median(a[ring], axis=0)]
    keep = rho < R - rim_w - 0.6
    if glyph_mask is not None:
        keep &= ~glyph_mask
    body = c.circ(cx, cy, R - rim_w)
    P, used = paint.fit_bevel(body, a, box[:2], BODY_STOPS, R - rim_w, keep, radial_c=(cx, cy))
    return {"rim": rim, "rim_w": rim_w, "body": P}, used


def moved(S, dx, dy):
    return {"t": "move", "of": S, "d": [dx, dy]}


def mv_layer(S, fill, kw, dx, dy):
    """A glyph layer moved: its shape, a bevel paint's own shape and a clip shape all go with it."""
    import copy
    fill = copy.deepcopy(fill)
    if isinstance(fill, dict) and "bevel" in fill and "of" in fill["bevel"]:
        fill["bevel"]["of"] = moved(fill["bevel"]["of"], dx, dy)
    kw = dict(kw)
    if "clip" in kw:
        kw["clip"] = moved(kw["clip"], dx, dy)
    return moved(S, dx, dy), fill, kw


def tuned_glyph(k, r, disc0, do_tune=True):
    """The glyph as read (glyphs_ib), in sheet coordinates of the idle cell, its numbers nudged by tune.py (at most 0.6 px)
    against the idle art inside the body. Returns (layers as (shape, colour, kw), largest move)."""
    import glyphs_ib
    import ptune as tune
    oy = 50 * r
    glyph = [mv_layer(S, col_, kw, 0.0, oy) for S, col_, kw in glyphs_ib.GLYPHS[k]()]
    if not do_tune or k in NO_TUNE:
        return glyph, 0.0
    box = cell(r, 0)
    jj, ii = np.mgrid[0:H, 0:W]
    x, y = ii + 0.5 + box[0], jj + 0.5 + box[1]
    gm = np.zeros((H, W), bool)
    for S, _, _ in glyph:
        gm |= c.sdf.sd(S, x, y) < 1.3
    style, _ = measure_style(r, 0, disc0, gm)
    it, body = base(r, 0, disc0, style)
    layers = []
    for S, col_, kw in glyph:
        Lr = {"shape": S, "fill": col_ if isinstance(col_, dict) else list(col_), "clip": body}
        Lr.update(kw)
        layers.append(Lr)
    s = 4
    under = tune.under_image(it["layers"], box, s, c.FLAT)
    mask = np.hypot(x - disc0[0], y - disc0[1]) < disc0[2] - style["rim_w"] - 0.4
    f, b, mv = tune.tune(layers, under, c.art(SHEET, box), box, mask, s=s, reach=0.6)
    print(f"  {k}: tune {f:.0f} -> {b:.0f} (largest move {mv:.2f} px)")
    return [(L["shape"], L["fill"], {kk: v for kk, v in L.items() if kk not in ("shape", "fill", "clip")}) for L in layers], mv


NO_TUNE = ()


def build(k, z=8, verbose=True, do_tune=True):
    r = KEYS.index(k)
    rows, scenes, stats = [], {}, []
    disc0 = measure_disc(r, 0)
    glyph0, mv = tuned_glyph(k, r, disc0, do_tune)
    for col, st in enumerate(STATES):
        box = cell(r, col)
        disc = measure_disc(r, col)
        dx, dy = disc[0] - disc0[0], disc[1] - disc0[1]
        glyph = [mv_layer(S, col_, kw, dx, dy) for S, col_, kw in glyph0]
        # the glyph's pixels (grown 1.2 px) are kept out of the body's paint measurement
        jj, ii = np.mgrid[0:H, 0:W]
        x, y = ii + 0.5 + box[0], jj + 0.5 + box[1]
        gm = np.zeros((H, W), bool)
        for S, _, _ in glyph:
            gm |= c.sdf.sd(S, x, y) < 1.3
        style, used = measure_style(r, col, disc, gm)
        if st == "hover":
            style["shadow"] = SHADOW
        it, body = base(r, col, disc, style)
        for S, col_, kw in glyph:
            Lr = {"shape": S, "fill": col_ if isinstance(col_, dict) else list(col_), "clip": body}
            Lr.update(kw)
            it["layers"].append(Lr)
        it["name"] = f"info-button-{k} {st}"
        scene = [it]
        mad, w16 = c.score(scene, SHEET, box)
        e = c.edge_px(scene, SHEET, box)
        stats.append((st, mad, w16, e))
        scenes[st] = scene
        rows.append((scene, SHEET, box, c.FLAT))
    p = c.compare(rows, f"info-button-{k}", zoom_k=z)
    c.save_scene(f"info-button-{k}", {"id": f"info-button-{k}", "sheet": "Interface/" + SHEET,
                                      "states": {st: {"box": list(cell(r, i)), "scene": scenes[st]} for i, st in enumerate(STATES)}})
    if verbose:
        for st, mad, w16, e in stats:
            print(f"  {k:10s} {st:9s} mad {mad:6.2f} w16 {w16:5.3f} edge {e if e is None else round(e, 2)}")
    return p, stats, mv


# the hover state's shadow of the disc on the tab: offset x, offset y, growth, strength, softness (read on the hover cells:
# darkest under the disc's lower right, fading over about 4 px)
SHADOW = [2.3, 4.15, -0.6, 0.66, 3.05]   # fitted on the money hover cell, tab pixels outside the disc (MAD 2.3)


if __name__ == "__main__":
    keys = sys.argv[1:] or list(KEYS)
    for i, k in enumerate(keys):
        p, st, mv = build(k)
        print(f"[{i + 1}/{len(keys)}] info-button-{k} {p}")
