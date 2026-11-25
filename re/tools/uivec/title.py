"""uivec: the title menu (P41). Measures the six pads and seven bubbles on the player's art, builds the scene from recipes,
renders it over the backdrop and writes the art beside it.
Usage: py -3.12 re/tools/uivec/title.py [scale] [--out DIR]
  -> DIR/title_scene.json (numbers only), title_art_x<s>.png, title_vec_x<s>.png, title_vec_hover_x<s>.png, title_fits.png
"""
import json
import os
import sys

import numpy as np
from PIL import Image, ImageDraw

sys.path.insert(0, os.path.dirname(__file__))
import fit      # noqa: E402
import recipes  # noqa: E402
import sdf      # noqa: E402

# kTitleButtons (log/roadmap/P40/art_catalogue.md 1b): x, y, w, h of each cell in TitleUnSel / TitleMO
CELLS = {"load": (0x2a, 0x15, 0x144, 0x79), "play": (0x1a0, 0x1f, 0x16b, 0x8d), "tutorial": (0x1fe, 0x167, 0x10f, 0x6f),
         "champ": (0x11f, 0x1da, 0x120, 0x73), "sandbox": (0xd, 0x17e, 0x11f, 0x73), "exit": (0x2d1, 0x210, 0x3a, 0x39)}


# Another screen reuses everything here through configure(): its own cells (search rectangles in screen coordinates),
# its art loader, which of its buttons are matte discs, a rough centre and radius per bubble where the art alone does not
# single it out, and its own files.
DISCS = {"exit"}
HINTS = {}
SCREEN_LOAD = None
NEAR_K = 1.3       # dark pixels within this many radii of the hinted bubble count as its rim (less where a socket ring surrounds it)
GLYPH_K = 0.938   # the glyph is looked for inside this share of the bubble's radius (less where the body darkens at its edge)
STYLE_FILE = os.path.join(os.path.dirname(__file__), "style.json")
POST = None       # a screen's own correction of the fresh measurement (its pads, say), called with the measurements
OUT = "title"


def configure(name, cells, load, discs=(), hints=None, post=None):
    global CELLS, DISCS, HINTS, SCREEN_LOAD, OUT, GEOM, GLYPHS, POST, STYLE_FILE
    CELLS, DISCS, HINTS, SCREEN_LOAD, OUT, POST = dict(cells), set(discs), dict(hints or {}), load, name, post
    # The recipes are shared; the PAINT can differ per sheet (this screen's wells are white where the title's are cream).
    # A screen that has measured its own keeps it in style_<name>.json, laid over the title menu's.
    STYLE_FILE = os.path.join(os.path.dirname(__file__), f"style_{name}.json")
    if os.path.exists(STYLE_FILE):
        with open(STYLE_FILE) as f:
            for k, v in json.load(f).items():
                recipes.STYLE[k].update(v)
    GEOM = os.path.join(os.path.dirname(__file__), f"{name}_geom.json")
    GLYPHS = os.path.join(os.path.dirname(__file__), f"glyphs_{name}.json")


def rgb_of(idx, pal):
    return pal[idx].astype(float)


def measure_bubble(idx, pal, cell, near=None):
    """The rim's circle. near: (cx, cy, r) of the same bubble in the other state; only dark pixels around it count (the hover
    cells also hold the pad, its outline and its shadow)."""
    x, y, w, h = cell
    sub = idx[y:y + h, x:x + w]
    rgb = rgb_of(sub, pal)
    opaque = sub < 0xfe
    L = fit.lum(rgb)
    dark = opaque & (L < 64)
    if near is not None:
        jj, ii = np.mgrid[0:h, 0:w]
        dark &= np.hypot(ii + 0.5 + x - near[0], jj + 0.5 + y - near[1]) < near[2] * NEAR_K
    j, i = np.nonzero(dark)
    c0 = (i.mean() + 0.5, j.mean() + 0.5)
    for _ in range(3):
        pts = fit.outermost(dark, c0)
        cx, cy, r, res = fit.fit_circle(pts, trim=0.8)
        c0 = (cx, cy)
    r += 0.15   # the outermost dark pixel CENTRES sit about half a pixel inside the antialiased edge
    return cx + x, cy + y, r, res


def measure_glyph(idx, pal, cell, cx, cy, r):
    x, y, w, h = cell
    sub = idx[y:y + h, x:x + w]
    rgb = rgb_of(sub, pal)
    jj, ii = np.mgrid[0:h, 0:w]
    bx, by, br = cx - 0.021 * r, cy - 0.024 * r, GLYPH_K * r - 0.9
    inside = (np.hypot(ii + 0.5 + x - bx, jj + 0.5 + y - by) < br) & (sub < 0xfe)
    rings, cov = fit.glyph_rings(rgb, inside, origin=(x, y))
    return rings


def measure_pad(base_idx, base_pal, mo_idx, cell, bub):
    x, y, w, h = cell
    rgb = rgb_of(base_idx[y:y + h, x:x + w], base_pal)
    well = (rgb[..., 0] >= rgb[..., 2] + 4) & (fit.lum(rgb) > 100)
    well = fit.largest_inner_component(well)
    if well is None:
        return None
    pts = fit.edge_points(well)
    wx, wy, a, b, rot = fit.fit_ellipse(pts)
    for _ in range(3):   # refit without the points far from the ellipse (strays, the plate's corner)
        t = np.deg2rad(rot)
        u = (pts[:, 0] - wx) * np.cos(t) + (pts[:, 1] - wy) * np.sin(t)
        v = -(pts[:, 0] - wx) * np.sin(t) + (pts[:, 1] - wy) * np.cos(t)
        k = np.hypot(u / a, v / b)
        pts = pts[np.abs(k - 1) < 0.08]
        wx, wy, a, b, rot = fit.fit_ellipse(pts)
    inner = (wx + x, wy + y, a + 0.3, b + 0.3, rot)
    # the ring's outer edge: along rays from the well's centre, the sharpest luminance step beyond the well (the ring's rim
    # against the backdrop). Only rays whose step is clear are kept; the shadowed side has none.
    Lb = fit.lum(rgb_of(base_idx, base_pal))
    cxw, cyw = wx + x, wy + y
    t = np.deg2rad(rot)
    sel = []
    for deg in np.arange(0, 360, 1.5):
        th = np.deg2rad(deg)
        du, dv = a * np.cos(th), b * np.sin(th)                 # the well's edge point along this parameter
        ex, ey = du * np.cos(t) - dv * np.sin(t), du * np.sin(t) + dv * np.cos(t)
        n = np.hypot(ex, ey)
        ux, uy = ex / n, ey / n
        k = np.arange(n + 3, n + 0.75 * b + 26, 0.5)
        px, py = cxw + ux * k, cyw + uy * k
        ok = (px > 2) & (px < 797) & (py > 2) & (py < 597)
        if ok.sum() < 12:
            continue
        px, py, k = px[ok], py[ok], k[ok]
        if (np.hypot(px - bub[0], py - bub[1]) < bub[2] + 5).any():
            continue
        x0, y0 = np.floor(px - 0.5).astype(int), np.floor(py - 0.5).astype(int)
        fx, fy = px - 0.5 - x0, py - 0.5 - y0
        v = (Lb[y0, x0] * (1 - fx) * (1 - fy) + Lb[y0, x0 + 1] * fx * (1 - fy) + Lb[y0 + 1, x0] * (1 - fx) * fy
             + Lb[y0 + 1, x0 + 1] * fx * fy)
        g = v[4:] - v[:-4]                                       # the step over 2 pixels
        m = int(np.argmax(np.abs(g)))
        if abs(g[m]) >= 22:
            sel.append((px[m + 2], py[m + 2]))
    sel = np.array(sel)
    ox, oy, oa, ob, _ = fit.fit_ellipse(sel - np.array([x, y]), rot=rot)
    for _ in range(4):
        u = (sel[:, 0] - x - ox) * np.cos(t) + (sel[:, 1] - y - oy) * np.sin(t)
        v = -(sel[:, 0] - x - ox) * np.sin(t) + (sel[:, 1] - y - oy) * np.cos(t)
        kk = np.hypot(u / oa, v / ob)
        keep = np.abs(kk - 1) < 0.05
        if keep.sum() < 20:
            break
        sel = sel[keep]
        ox, oy, oa, ob, _ = fit.fit_ellipse(sel - np.array([x, y]), rot=rot)
    outer = (ox + x, oy + y, oa, ob, rot)
    # a fit from too little of the rim (the pad lies over the big ball, or the rays caught another edge) is replaced by the
    # proportions the clean pads share
    if not (1.3 < oa / inner[2] < 1.5 and 1.25 < ob / inner[3] < 1.55 and abs(outer[1] - inner[1]) < 4):
        outer = (inner[0] + 0.15 * inner[2], inner[1] + 1.0, 1.41 * inner[2], 1.37 * inner[3], rot)
        return outer, inner, -len(sel)
    return outer, inner, len(sel)


def measure_disc(idx, pal, base_idx, base_pal, cell):
    """A matte disc (no rim line): the circle through the edge where the cell's colour is half way to the backdrop's."""
    x, y, w, h = cell
    sub = idx[y:y + h, x:x + w]
    L = fit.lum(rgb_of(sub, pal))
    Lb = fit.lum(rgb_of(base_idx[y:y + h, x:x + w], base_pal))
    opaque = sub < 0xfe
    # everything the cell paints differently from the backdrop: the face, its dark side and its light bevel edge
    dif = np.abs(rgb_of(sub, pal) - rgb_of(base_idx[y:y + h, x:x + w], base_pal)).max(axis=2)
    body = opaque & (dif > 14)
    hint = next((HINTS[n] for n, c in CELLS.items() if tuple(c) == tuple(cell) and n in HINTS), None)
    if body.sum() < 80 and hint is not None:
        # a disc painted into the backdrop (nothing to compare it with): along rays from the hinted centre, the point
        # near the hinted radius where the luminance changes most is its edge
        pts = []
        for th in np.deg2rad(np.arange(0, 360, 4.0)):
            k = np.arange(hint[2] - 5, hint[2] + 5.01, 0.25)
            px, py = hint[0] - x + np.cos(th) * k, hint[1] - y + np.sin(th) * k
            ok = (px > 1) & (px < w - 2) & (py > 1) & (py < h - 2)
            if ok.sum() < 20:
                continue
            px, py, k = px[ok], py[ok], k[ok]
            x0, y0 = np.floor(px - 0.5).astype(int), np.floor(py - 0.5).astype(int)
            fx, fy = px - 0.5 - x0, py - 0.5 - y0
            v = (L[y0, x0] * (1 - fx) * (1 - fy) + L[y0, x0 + 1] * fx * (1 - fy) + L[y0 + 1, x0] * (1 - fx) * fy
                 + L[y0 + 1, x0 + 1] * fx * fy)
            g = np.abs(v[6:] - v[:-6])
            m = int(np.argmax(g))
            if g[m] > 12:
                pts.append((px[m + 3], py[m + 3]))
        cx, cy, r, res = fit.fit_circle(pts, trim=1.0)
    else:
        j, i = np.nonzero(body)
        c0 = (i.mean() + 0.5, j.mean() + 0.5)
        for _ in range(3):
            cx, cy, r, res = fit.fit_circle(fit.outermost(body, c0), trim=0.8)
            c0 = (cx, cy)
        r += 0.5    # pixel centres of the mask to its edge
    jj, ii = np.mgrid[0:h, 0:w]
    inside = (np.hypot(ii + 0.5 - cx, jj + 0.5 - cy) < r * 0.74) & opaque
    rings, cov = fit.glyph_rings(rgb_of(sub, pal), inside, origin=(x, y))
    return cx + x, cy + y, r, res, rings


def load_art():
    if SCREEN_LOAD:
        return SCREEN_LOAD()
    base_idx, base_pal = fit.load_sheet("Interface/TitleBASE.pcx")
    un_idx, un_pal = fit.load_sheet("Interface/TitleUnSel.pcx")
    mo_idx, mo_pal = fit.load_sheet("Interface/TitleMO.pcx")
    return base_idx, base_pal, un_idx, un_pal, mo_idx, mo_pal


def composite(base_idx, base_pal, over_idx, over_pal):
    art = base_pal[base_idx].copy()
    for name, (x, y, w, h) in CELLS.items():
        sub = over_idx[y:y + h, x:x + w]
        m = sub < 0xfe
        art[y:y + h, x:x + w][m] = over_pal[sub][m]
    return art


def measure_body(art, cx, cy, r):
    """The bubble's body inside its rim: the circle through the points where, going outward along each ray, the luminance
    falls half way from the body's level to the rim's. art: the composed frame (H x W x 3)."""
    L = fit.lum(art)
    pts = []
    for th in np.deg2rad(np.arange(0, 360, 3.0)):
        k = np.arange(0.72 * r, r + 0.5, 0.2)
        px, py = cx + np.cos(th) * k, cy + np.sin(th) * k
        x0, y0 = np.floor(px - 0.5).astype(int), np.floor(py - 0.5).astype(int)
        fx, fy = px - 0.5 - x0, py - 0.5 - y0
        v = (L[y0, x0] * (1 - fx) * (1 - fy) + L[y0, x0 + 1] * fx * (1 - fy) + L[y0 + 1, x0] * (1 - fx) * fy
             + L[y0 + 1, x0 + 1] * fx * fy)
        n = len(k) // 3
        body = np.median(v[:n])
        rim = v[k > r - 0.12 * r].min()
        if body - rim < 40 or (v[:n] < (body + rim) / 2).any():     # the glyph reaches the rim on this ray
            continue
        m = np.nonzero(v < (body + rim) / 2)[0]
        if len(m):
            j = m[0]
            t = (v[j - 1] - (body + rim) / 2) / max(v[j - 1] - v[j], 1e-6)
            kk = k[j - 1] + t * (k[j] - k[j - 1])
            pts.append((cx + np.cos(th) * kk, cy + np.sin(th) * kk))
    if len(pts) < 20:
        return None
    bx, by, br, _ = fit.fit_circle(pts, trim=0.5)
    return [round(bx, 2), round(by, 2), round(br, 2)]


def measure_hover_well(mo_idx, mo_pal, cell, bub, inner):
    """The hover cell's yellow well: an ellipse through its edge away from the bubble that covers part of it."""
    x, y, w, h = cell
    sub = mo_idx[y:y + h, x:x + w]
    rgb = rgb_of(sub, mo_pal)
    jj, ii = np.mgrid[0:h, 0:w]
    away = np.hypot(ii + 0.5 + x - bub[0], jj + 0.5 + y - bub[1]) > bub[2] + 1.0
    well = (sub < 0xfe) & (rgb[..., 0] > rgb[..., 2] + 60) & away
    well &= np.hypot((ii + 0.5 + x - inner[0]) / (inner[2] + 6), (jj + 0.5 + y - inner[1]) / (inner[3] + 6)) < 1
    if well.sum() < 200:
        return None
    pts = fit.edge_points(well)
    pts = pts[np.hypot(pts[:, 0] + x - bub[0], pts[:, 1] + y - bub[1]) > bub[2] + 3.5]
    wx, wy, a, b, rot = fit.fit_ellipse(pts, rot=inner[4])
    for _ in range(3):
        t = np.deg2rad(rot)
        u = (pts[:, 0] - wx) * np.cos(t) + (pts[:, 1] - wy) * np.sin(t)
        v = -(pts[:, 0] - wx) * np.sin(t) + (pts[:, 1] - wy) * np.cos(t)
        pts = pts[np.abs(np.hypot(u / a, v / b) - 1) < 0.06]
        wx, wy, a, b, rot = fit.fit_ellipse(pts, rot=inner[4])
    return wx + x, wy + y, a + 0.3, b + 0.3, rot


GEOM = os.path.join(os.path.dirname(__file__), "title_geom.json")


def _measure(verbose=True, fresh=False):
    """Every title button, per state (the hover art places its bubbles a little apart from the idle ones). The refined
    geometry refine.py saved (title_geom.json) is returned when there is one, unless fresh."""
    if not fresh and os.path.exists(GEOM):
        with open(GEOM) as f:
            return json.load(f)
    base_idx, base_pal, un_idx, un_pal, mo_idx, mo_pal = load_art()
    arts = {"idle": (un_idx, un_pal, composite(base_idx, base_pal, un_idx, un_pal).astype(float)),
            "hover": (mo_idx, mo_pal, composite(base_idx, base_pal, mo_idx, mo_pal).astype(float))}
    meas = {}
    for name, cell in CELLS.items():
        m = {"kind": "disc" if name in DISCS else "bubble"}
        for st, (idx, pal, art) in arts.items():
            if m["kind"] == "disc":
                if st == "hover":       # the same disc; its glyph turns yellow
                    m[st] = dict(m["idle"])
                    continue
                cx, cy, r, res, rings = measure_disc(idx, pal, base_idx, base_pal, cell)
                m[st] = {"c": [round(cx, 2), round(cy, 2), round(r, 2)], "glyph": rings}
            else:
                cx, cy, r, res = measure_bubble(idx, pal, cell, near=m["idle"]["c"] if st == "hover" else HINTS.get(name))
                rings = measure_glyph(idx, pal, cell, cx, cy, r)
                m[st] = {"c": [round(cx, 2), round(cy, 2), round(r, 2)], "glyph": rings,
                         "body": measure_body(art, cx, cy, r)}
            if verbose:
                print(f"{name:9s} {st:5s} {m['kind']:6s} c=({cx:.2f}, {cy:.2f}) r={r:.2f} res={res:.2f} "
                      f"body={m[st].get('body')} rings={len(rings)} pts={sum(len(q) for q in rings)}")
        if m["kind"] == "bubble":
            pad = measure_pad(base_idx, base_pal, mo_idx, cell, m["idle"]["c"])
            if pad:
                m["outer"] = [round(v, 2) for v in pad[0]]
                m["inner"] = [round(v, 2) for v in pad[1]]
                # the hover cell repaints the pad; its yellow well gives where (the ring moves with it)
                hw = measure_hover_well(mo_idx, mo_pal, cell, m["hover"]["c"], m["inner"])
                dx, dy = (hw[0] - m["inner"][0], hw[1] - m["inner"][1]) if hw else (0.0, 0.0)
                m["idle"]["outer"], m["idle"]["inner"] = m["outer"], m["inner"]
                m["hover"]["inner"] = [round(v, 2) for v in hw] if hw else m["inner"]
                m["hover"]["outer"] = [round(m["outer"][0] + dx, 2), round(m["outer"][1] + dy, 2)] + m["outer"][2:]
                if verbose:
                    print(f"{name:9s} pad   outer={m['outer']} inner={m['inner']} n={pad[2]} "
                          f"hover well={m['hover']['inner']}")
        meas[name] = m
    if POST:
        POST(meas)
    return meas


GLYPHS = os.path.join(os.path.dirname(__file__), "glyphs_title.json")
USE_MIN = 0.88
# Glyphs kept on their measured outline although a parts version exists (the override). play: the ball on a tee reads
# right from parts but fails the acceptance pass's silhouette check in the idle state (0.869 against 0.90; its outline
# gives 0.934): the stem is too straight and the lit dimples merge into a blob. Remove the name when its parts pass.
OUTLINE_OVERRIDE = set(os.environ.get("UIVEC_OUTLINE", "").split(",")) - {""}
# the other way round: glyphs drawn from their parts although the overlap is under USE_MIN (a clean, orderly drawing can
# look right and still overlap a ragged original less than its own trace does)
# play: Mariano approved the parts ball's look on 2026-11-23 ("ball button looks fine now"): an orderly dimpled ball on a
# turned tee overlaps the art's ragged silhouette less (0.80) than a trace does, and looks right.
PARTS_FORCE = set(os.environ.get("UIVEC_PARTS", "play").split(",")) - {""}


def measure(verbose=True, fresh=False):
    """_measure plus the glyphs authored from their real parts (glyphs.py -> glyphs_title.json): a glyph that has one is
    drawn from it ("shape"); the hover state shows the same glyph moved with its bubble. The traced outline ("glyph") stays
    for the measurements that only need to know where the glyph is."""
    meas = _measure(verbose, fresh)
    if os.path.exists(GLYPHS):
        with open(GLYPHS) as f:
            authored = json.load(f)
        for name, a in authored.items():
            # a glyph built from parts is used only when it reproduces the art's silhouette (overlap >= USE_MIN);
            # below that the measured outline stays in the scene (the override) until the parts are right
            if (name in meas and a.get("shape") and name not in OUTLINE_OVERRIDE
                    and (a.get("overlap", 0) >= USE_MIN or name in PARTS_FORCE)):
                m = meas[name]
                m["idle"]["shape"] = a["shape"]
                dx = m["hover"]["c"][0] - m["idle"]["c"][0]
                dy = m["hover"]["c"][1] - m["idle"]["c"][1]
                m["hover"]["shape"] = a.get("shape_hover") or {"t": "move", "of": a["shape"],
                                                               "d": [round(dx, 2), round(dy, 2)]}
    return meas


def scene(meas, hover, style=None, only=None, parts=("pad", "button")):
    style = style or recipes.STYLE
    st = "hover" if hover else "idle"
    items = []
    for name, m in meas.items():
        if only and name not in only:
            continue
        cx, cy, r = m[st]["c"]
        if m["kind"] == "disc":
            items.append(recipes.disc(cx, cy, r, st, m[st].get("shape") or m[st]["glyph"], style=style["disc"]))
            continue
        oshape = None
        if "outer" in m:
            o, i = m[st]["outer"], m[st]["inner"]
            if "pad" in parts:
                items.append(recipes.pad(o, i, st, style=style["pad"]))
            oshape = {"t": "ellipse", "c": o[:2], "r": o[2:4], "rot": o[4]}
        if "button" in parts:
            items.append(recipes.bubble(cx, cy, r, "yellow" if hover else "blue", m[st].get("shape") or m[st]["glyph"],
                                        shadow=oshape,
                                        style=style["bubble"], body=m[st].get("body")))
    return items


def build(scale=3, out=None):
    out = out or os.path.join(fit.ROOT, "log", "roadmap", "P41", OUT)
    os.makedirs(out, exist_ok=True)
    base_idx, base_pal, un_idx, un_pal, mo_idx, mo_pal = load_art()
    meas = measure()
    with open(os.path.join(out, "title_meas.json"), "w") as f:
        json.dump(meas, f)
    with open(os.path.join(out, "title_scene.json"), "w") as f:
        json.dump({"idle": scene(meas, False), "hover": scene(meas, True)}, f)

    base_rgb = Image.fromarray(base_pal[base_idx])
    big = (800 * scale, 600 * scale)
    for tag, over_idx, over_pal, hover in (("", un_idx, un_pal, False), ("_hover", mo_idx, mo_pal, True)):
        art = composite(base_idx, base_pal, over_idx, over_pal)
        Image.fromarray(art).resize(big, Image.BILINEAR).save(os.path.join(out, f"title_art{tag}_x{scale}.png"))
        # the shapes go over what the game has on screen in that state: the backdrop, or the backdrop with its hover cells
        # (a hover cell repaints its surroundings too, a little apart from the idle picture baked into the backdrop)
        img = np.asarray((Image.fromarray(art) if hover else base_rgb).resize(big, Image.BILINEAR), float).copy()
        sdf.render(scene(meas, hover), img, scale)
        Image.fromarray(np.clip(img + 0.5, 0, 255).astype(np.uint8)).save(os.path.join(out, f"title_vec{tag}_x{scale}.png"))

    # the fits drawn over the art at 2x, for checking by eye
    art = composite(base_idx, base_pal, un_idx, un_pal)
    z = 2
    chk = Image.fromarray(art).resize((800 * z, 600 * z), Image.NEAREST)
    d = ImageDraw.Draw(chk)
    for name, m in meas.items():
        cx, cy, r = m["idle"]["c"]
        d.ellipse([(cx - r) * z, (cy - r) * z, (cx + r) * z, (cy + r) * z], outline=(255, 0, 0))
        if m["idle"].get("body"):
            bx, by, br = m["idle"]["body"]
            d.ellipse([(bx - br) * z, (by - br) * z, (bx + br) * z, (by + br) * z], outline=(0, 255, 255))
        for key, colr in (("outer", (0, 255, 0)), ("inner", (255, 0, 255))):
            if key in m:
                ex, ey, a, b, rot = m[key]
                t = np.linspace(0, 2 * np.pi, 200)
                c, s_ = np.cos(np.deg2rad(rot)), np.sin(np.deg2rad(rot))
                px = ex + a * np.cos(t) * c - b * np.sin(t) * s_
                py = ey + a * np.cos(t) * s_ + b * np.sin(t) * c
                d.line([(float(u) * z, float(v) * z) for u, v in zip(px, py)], fill=colr)
        for ring in m["idle"]["glyph"]:
            d.line([(u * z, v * z) for u, v in ring + ring[:1]], fill=(255, 255, 0))
    chk.save(os.path.join(out, "title_fits.png"))
    return meas


if __name__ == "__main__":
    a = [v for v in sys.argv[1:] if not v.startswith("--")]
    build(int(a[0]) if a else 3)
