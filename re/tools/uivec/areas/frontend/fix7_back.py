"""P41 round 7, item 2 (log/roadmap/P41/FIX7_BRIEF.md): centre the Back arrow in its disc, idle and hover identical in position.

Mariano: "la flecha para volver hacia atras en el selector de partidas deberia estar centrada". The Load Game Back button's arrow
sat 2.23 px above the disc centre (and 0.75 px right of it) in both states. The same audit runs on every Back button of the
frontend area (difficulty, theme packs, world map / course action menu, Pick a Pro; the character editor has no Back button: its
exits are OK / Reset / Load / Save, and its backdrop scene holds no arrow).

What is measured, per button and state:
  * the disc: the circle of the approved scene (the arrow layer's clip circle), centre and radius;
  * the glyph: the arrow's bounding box (tip x .. tail x, line y +- head width / 2, every "move" above it applied), checked on
    the rendered glyph mask at 8x (pixels where the glyph's distance is < 0 inside the disc);
  * the art: the dark navy (idle) or yellow (hover) pixels inside the disc, bounding box centre against the same disc centre
    (for the record only; the fix centres on the disc, which is what he asked).
The fix moves the arrow (both end points, the shape otherwise untouched) so its box centre is the disc centre, the same arrow in
both states. The port draws a Back button's IDLE look from the screen's backdrop scene (load-backdrop, pro-backdrop,
diff-backdrop, theme-backdrop, world-backdrop / world-back's idle state) and the HOVER look from the button scene, so the same move
is applied to the arrow the backdrop carries, and those backdrops are delivered too (only the arrow's two points change).

    py -3.12 re/tools/uivec/areas/frontend/fix7_back.py [--measure]

Writes log/roadmap/P41/work7/frontend/: <id>.png (rows = states; art LEFT, previous MIDDLE, new RIGHT, 8x), <id>.json,
<id>_check.png (the same tiles with the disc centre cross and the glyph box drawn, and the idle and hover glyphs overlaid),
measured.tsv, results.tsv.
"""
import copy
import json
import os
import sys

import numpy as np
from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import common as c   # noqa: E402
import fit           # noqa: E402
import sdf           # noqa: E402

ROOT = c.ROOT
SCENES = os.path.join(ROOT, "re", "tools", "uivec", "scenes", "frontend")
OUT = os.path.join(ROOT, "log", "roadmap", "P41", "work7", "frontend")
os.makedirs(OUT, exist_ok=True)
Z = 8
HALF = 27          # comparison crop: the disc centre +- 27 px (54 px at 8x = 432 px per tile)


def load_scene(eid):
    with open(os.path.join(SCENES, eid + ".json")) as f:
        return json.load(f)


def glyph_layers(scene):
    """(layer, arrow dict, accumulated move (dx, dy), disc circle) for every layer of a scene list holding an arrow."""
    out = []
    for item in scene:
        for L in item["layers"]:
            found = []

            def walk(o, d):
                if isinstance(o, dict):
                    if o.get("t") == "arrow":
                        found.append((o, d))
                        return
                    if o.get("t") == "move":
                        walk(o["of"], (d[0] + o["d"][0], d[1] + o["d"][1]))
                        return
                    for k, v in o.items():
                        walk(v, d)
                elif isinstance(o, list):
                    for v in o:
                        walk(v, d)
            walk(L["shape"], (0.0, 0.0))
            for a, d in found:
                clip = L.get("clip")
                out.append((L, a, d, clip))
    return out


def arrow_box(a, d):
    """Bounding box of the arrow polygon sdf.py builds (any direction), moved by d."""
    ax, ay = a["a"]
    bx, by = a["b"]
    ux, uy = bx - ax, by - ay
    n = max(np.hypot(ux, uy), 1e-9)
    ux, uy = ux / n, uy / n
    px, py = -uy, ux
    hx, hy = bx - a["hl"] * ux, by - a["hl"] * uy
    w, hw = a["w"] / 2, a["hw"] / 2
    pts = [(ax + px * w, ay + py * w), (ax - px * w, ay - py * w), (hx + px * hw, hy + py * hw), (hx - px * hw, hy - py * hw),
           (bx, by)]
    xs = [p[0] + d[0] for p in pts]
    ys = [p[1] + d[1] for p in pts]
    return min(xs), min(ys), max(xs), max(ys)


def rendered_box(layer, cx, cy, s=Z):
    """Bounding box (frame px) of the rendered glyph: grid cells at s x whose centre lies inside the glyph and the clip."""
    x0, y0 = int(cx - HALF), int(cy - HALF)
    n = 2 * HALF * s
    jj, ii = np.mgrid[0:n, 0:n]
    X = x0 + (ii + 0.5) / s
    Y = y0 + (jj + 0.5) / s
    m = sdf.sd(layer["shape"], X, Y) < 0
    if layer.get("clip"):
        m &= sdf.sd(layer["clip"], X, Y) < 0
    ys, xs = np.nonzero(m)
    # edges of the covered cells
    return (x0 + xs.min() / s, y0 + ys.min() / s, x0 + (xs.max() + 1) / s, y0 + (ys.max() + 1) / s), m


def art_box(art, cx, cy, r, state):
    """Bounding box of the art's glyph pixels inside the disc (r - 2): navy for idle, yellow for hover."""
    x0, y0 = int(cx - r), int(cy - r)
    x1, y1 = int(np.ceil(cx + r)), int(np.ceil(cy + r))
    sub = art[y0:y1, x0:x1]
    yy, xx = np.mgrid[y0:y1, x0:x1]
    inside = np.hypot(xx + 0.5 - cx, yy + 0.5 - cy) < r - 2.0
    if state == "idle":
        g = fit.lum(sub) < 70
    else:
        g = (sub[..., 0] > 180) & (sub[..., 1] > 140) & (sub[..., 2] < 120)
    g &= inside
    g = largest_component(g)
    ys, xs = np.nonzero(g)
    if len(xs) == 0:
        return None
    return (x0 + xs.min(), y0 + ys.min(), x0 + xs.max() + 1, y0 + ys.max() + 1)


def largest_component(m):
    """The largest 4-connected component of a boolean mask (the arrow; drops dark specks of the disc's lip)."""
    lab = np.zeros(m.shape, int)
    best, best_n, k = None, 0, 0
    for j0, i0 in zip(*np.nonzero(m)):
        if lab[j0, i0]:
            continue
        k += 1
        stack, n = [(j0, i0)], 0
        lab[j0, i0] = k
        while stack:
            j, i = stack.pop()
            n += 1
            for jj, ii in ((j + 1, i), (j - 1, i), (j, i + 1), (j, i - 1)):
                if 0 <= jj < m.shape[0] and 0 <= ii < m.shape[1] and m[jj, ii] and not lab[jj, ii]:
                    lab[jj, ii] = k
                    stack.append((jj, ii))
        if n > best_n:
            best, best_n = k, n
    return lab == best if best else m


def move_arrow(a, dx, dy):
    a["a"] = [round(a["a"][0] + dx, 3), round(a["a"][1] + dy, 3)]
    a["b"] = [round(a["b"][0] + dx, 3), round(a["b"][1] + dy, 3)]


# ---------------------------------------------------------------------------------------------------------------- arts
def arts():
    """{button id: {state: frame image}} of the art each Back button is judged against."""
    import load_screen
    out = {}
    base, hover, _, _ = load_screen.arts()
    out["load-back-button"] = {"idle": base, "hover": hover}
    D, DM = "Interface/TitleSelDiffUnSel.pcx", "Interface/TitleSelDiffMO.pcx"
    out["diff-back-button"] = {"idle": c.sheet(D)[0], "hover": c.over(D, DM, (0x2dc, 0x214, 0x44, 0x44))}
    T, TM = "Interface/Title_ThemePacks.pcx", "Interface/Title_ThemePacks_MO.pcx"
    out["theme-back-button"] = {"idle": c.sheet(T)[0], "hover": c.over(T, TM, (0x151, 1, 0x30, 0x31), (0x2d6, 0x215))}
    out["world-back"] = {"idle": c.sheet("Interface/WorldBase.pcx")[0], "hover": c.sheet("Interface/WorldButton.pcx")[0]}
    out["pro-backdrop"] = {"idle": c.sheet("Interface/Title_Pickapro.pcx")[0]}
    out["load-backdrop"] = {"idle": base}
    out["diff-backdrop"] = {"idle": out["diff-back-button"]["idle"]}
    out["theme-backdrop"] = {"idle": out["theme-back-button"]["idle"]}
    out["world-backdrop"] = {"idle": out["world-back"]["idle"]}
    return out


BUTTONS = ["load-back-button", "diff-back-button", "theme-back-button", "world-back"]
BACKDROPS = {"load-backdrop": "load-back-button", "pro-backdrop": "load-back-button", "diff-backdrop": "diff-back-button",
             "theme-backdrop": "theme-back-button", "world-backdrop": "world-back"}
SCREEN = {"load-back-button": "Load Game", "diff-back-button": "Select difficulty", "theme-back-button": "Theme Packs",
          "world-back": "Course action menu (world map)", "load-backdrop": "Load Game", "pro-backdrop": "Pick a Pro",
          "diff-backdrop": "Select difficulty", "theme-backdrop": "Theme Packs", "world-backdrop": "Course action menu"}


def measure(eid, doc, art_states):
    """Rows of measurements for each state's arrow layer that sits in a Back disc (bottom right of the screen)."""
    rows = []
    for st in doc["states"]:
        for L, a, d, clip in glyph_layers(st["scene"]):
            if clip is None or clip.get("t") != "circle" or clip["c"][0] < 700 or clip["c"][1] < 500:
                continue
            (cx, cy), r = clip["c"], clip["r"]
            bx0, by0, bx1, by1 = arrow_box(a, d)
            (rx0, ry0, rx1, ry1), _ = rendered_box(L, cx, cy)
            art = art_states.get(st["name"]) if art_states else None
            ab = art_box(art, cx, cy, r, "idle" if st["name"] == "idle" else "hover") if art is not None else None
            rows.append({"id": eid, "state": st["name"], "disc_cx": cx, "disc_cy": cy, "disc_r": r,
                         "glyph_cx": (bx0 + bx1) / 2, "glyph_cy": (by0 + by1) / 2,
                         "render_cx": (rx0 + rx1) / 2, "render_cy": (ry0 + ry1) / 2,
                         "art_cx": None if ab is None else (ab[0] + ab[2]) / 2, "art_cy": None if ab is None else (ab[1] + ab[3]) / 2,
                         "layer": L, "arrow": a, "move": d})
    return rows


# ---------------------------------------------------------------------------------------------------------------- images
def tile(bg, scene, cx, cy):
    rect = (int(round(cx)) - HALF, int(round(cy)) - HALF, 2 * HALF, 2 * HALF)
    return c.render_crop(bg, scene, rect, Z), rect


def wiped_bg(art, scenes, rect):
    x, y, w, h = rect
    m = np.zeros((h, w), bool)
    for sc in scenes:
        m |= c.shape_mask(sc, x, y, w, h, 1.5)
    bg = art.copy()
    bg[y:y + h, x:x + w] = c.inpaint(art[y:y + h, x:x + w], m)
    return bg


def only_button(scene, cx, cy):
    """The items of a (backdrop) scene that overlap the comparison crop (rendering 3,500 backdrop items for one crop is slow)."""
    x0, y0, x1, y1 = cx - HALF - 2, cy - HALF - 2, cx + HALF + 2, cy + HALF + 2
    return [it for it in scene if not (it["box"][2] < x0 or it["box"][0] > x1 or it["box"][3] < y0 or it["box"][1] > y1)]


def to8(a):
    return Image.fromarray(np.clip(a + 0.5, 0, 255).astype(np.uint8))


def marks(im, rect, cx, cy, box, col):
    """Disc centre cross (red) and glyph box (col) on an 8x tile."""
    d = ImageDraw.Draw(im)
    X = lambda v: (v - rect[0]) * Z
    Y = lambda v: (v - rect[1]) * Z
    d.line([(X(cx) - 14, Y(cy)), (X(cx) + 14, Y(cy))], fill=(255, 0, 0), width=1)
    d.line([(X(cx), Y(cy) - 14), (X(cx), Y(cy) + 14)], fill=(255, 0, 0), width=1)
    d.rectangle([X(box[0]), Y(box[1]), X(box[2]), Y(box[3])], outline=col)
    bx, by = (box[0] + box[2]) / 2, (box[1] + box[3]) / 2
    d.ellipse([X(bx) - 3, Y(by) - 3, X(bx) + 3, Y(by) + 3], outline=col)
    return im


def strip(rows, gap=8):
    """rows: list of lists of PIL images -> one image (white gaps)."""
    W = max(sum(i.width for i in r) + gap * (len(r) - 1) for r in rows)
    H = sum(max(i.height for i in r) for r in rows) + gap * (len(rows) - 1)
    out = Image.new("RGB", (W, H), (255, 255, 255))
    y = 0
    for r in rows:
        x = 0
        for i in r:
            out.paste(i, (x, y))
            x += i.width + gap
        y += max(i.height for i in r) + gap
    return out


def label(im, text):
    d = ImageDraw.Draw(im)
    d.rectangle([0, 0, 7 * len(text) + 6, 13], fill=(255, 255, 255))
    d.text((3, 1), text, fill=(0, 0, 0))
    return im


# ---------------------------------------------------------------------------------------------------------------- main
def unify_disc(new, moves):
    """world-back: the hover state's disc was fitted on its own cell (centre 0.9 px up, radius 0.76 px larger), but the art
    draws the same disc in both cells (outline rows 537 / 536 at the top, 581 at the bottom, columns 729 / 729 on the left in
    the dumps; the hover cell only has a lighter outer band). One structure: the hover disc takes the idle circle; its bevel
    paint keeps its measured profile, scaled to the idle radius (the paint is a function of depth / L)."""
    idle = new["states"][0]["scene"][0]["layers"][0]
    c0, r0 = idle["shape"]["c"], idle["shape"]["r"]
    for st in new["states"][1:]:
        it = st["scene"][0]
        L0 = it["layers"][0]
        old = (L0["shape"]["c"], L0["shape"]["r"])
        L0["shape"] = {"t": "circle", "c": list(c0), "r": r0}
        if isinstance(L0["fill"], dict) and "bevel" in L0["fill"]:
            L0["fill"]["bevel"]["L"] = idle["fill"]["bevel"]["L"]
        for L in it["layers"][1:]:
            if L["shape"].get("t") == "move":     # the hover arrow's own 2 px lift goes too: one glyph place in both states
                L["shape"]["d"] = [0.0, 0.0]
            if L.get("clip", {}).get("t") == "circle":
                L["clip"] = {"t": "circle", "c": list(c0), "r": r0}
        moves.append((st["name"] + " disc", round(c0[0] - old[0][0], 3), round(c0[1] - old[0][1], 3)))
        new["hover_circle"] = new.get("idle_circle")
        new["hover_shift"] = [0, 0]


UNIFY = {"world-back"}

# states whose disc paint was fitted with the art's arrow left in the art: the hover cells draw the yellow arrow 1 to 2 px higher
# than the scene's arrow (dumps: theme hover shaft rows 550-559 against the idle's 551-560 and the head from row 545; difficulty
# hover from row 553), so yellow pixels outside the old glyph went into the lit paint as a yellow halo above the head, hidden
# while the arrow sat high and exposed once it is centred. These are refitted (same circle, same stops) on the art with its own
# arrow inpainted away, as world-back's paint already was in round 2 (fix2_bubbles.disc_state).
REFIT = {("theme-back-button", "hover"), ("diff-back-button", "hover")}


def refit_paint(doc_new, doc_old, eid, art_states):
    import parts
    info = []
    for st_new, st_old in zip(doc_new["states"], doc_old["states"]):
        if (eid, st_new["name"]) not in REFIT:
            continue
        art = art_states[st_new["name"]]
        it = st_new["scene"][0]
        L0 = it["layers"][0]
        disc = L0["shape"]
        cx, cy = disc["c"]
        r = disc["r"]
        x0, y0 = int(cx - r - 3), int(cy - r - 3)
        x1, y1 = int(np.ceil(cx + r + 4)), int(np.ceil(cy + r + 4))
        sub = art[y0:y1, x0:x1]
        X, Y = parts.grid(x0, y0, x1, y1)
        # the art's arrow: the scene's old arrow grown 1.6 px plus the art's own glyph pixels grown 1 px
        old_l = glyph_layers(st_old["scene"])[0][0]
        am = sdf.sd(old_l["shape"], X, Y) < 1.6
        yy, xx = np.mgrid[y0:y1, x0:x1]
        inside = np.hypot(xx + 0.5 - cx, yy + 0.5 - cy) < r - 2.0
        g = (sub[..., 0] > 160) & (sub[..., 2] < 140) & inside
        g = largest_component(g)
        for _ in range(1):
            g = g | np.roll(g, 1, 0) | np.roll(g, -1, 0) | np.roll(g, 1, 1) | np.roll(g, -1, 1)
        am |= g
        clean = c.inpaint(sub, am)
        dm = sdf.sd(disc, X, Y) < -0.6
        before = L0["fill"]
        L0["fill"] = parts.bevel_fit(clean, disc, dm, x0, y0, before["bevel"]["L"], before["bevel"]["stops"])
        # what the old and new paints leave on the disc, outside both arrows, against the cleaned art (mean |diff|)
        newg = sdf.sd(glyph_layers(st_new["scene"])[0][0]["shape"], X, Y) < 1.0
        m = dm & ~am & ~newg & (sdf.sd(disc, X, Y) < -2.0)
        res = []
        for fill in (before, L0["fill"]):
            img = clean.copy()
            sdf.render([{"box": [x0, y0, x1, y1], "layers": [{"shape": disc, "fill": fill, "soft": L0.get("soft", 0.6)}]}],
                       img, 1, origin=(x0, y0))
            res.append(float(np.abs(img - clean).mean(axis=2)[m].mean()))
            # and over the area the old arrow uncovered (where a halo would show)
        un = dm & am & ~newg
        hal = []
        for fill in (before, L0["fill"]):
            img = clean.copy()
            sdf.render([{"box": [x0, y0, x1, y1], "layers": [{"shape": disc, "fill": fill, "soft": L0.get("soft", 0.6)}]}],
                       img, 1, origin=(x0, y0))
            hal.append(float(np.abs(img - clean).mean(axis=2)[un].mean()))
        info.append((st_new["name"], round(res[0], 2), round(res[1], 2), round(hal[0], 2), round(hal[1], 2)))
        print(eid, st_new["name"], "disc paint refit: MAD outside arrows", round(res[0], 2), "->", round(res[1], 2),
              "| over the uncovered strip", round(hal[0], 2), "->", round(hal[1], 2))
    return info


def fix_doc(doc, eid=""):
    """A copy of the scene document with every Back arrow moved so its box centre is its disc centre; returns (new, moves)."""
    new = copy.deepcopy(doc)
    moves = []
    if eid in UNIFY:
        unify_disc(new, moves)
    for st in new["states"]:
        for L, a, d, clip in glyph_layers(st["scene"]):
            if clip is None or clip.get("t") != "circle" or clip["c"][0] < 700 or clip["c"][1] < 500:
                continue
            bx0, by0, bx1, by1 = arrow_box(a, d)
            dx = clip["c"][0] - (bx0 + bx1) / 2
            dy = clip["c"][1] - (by0 + by1) / 2
            move_arrow(a, dx, dy)
            moves.append((st["name"], round(dx, 3), round(dy, 3)))
    return new, moves


def main():
    measure_only = "--measure" in sys.argv
    A = arts()
    mt = [["id", "screen", "state", "version", "disc_cx", "disc_cy", "disc_r", "glyph_box_cx", "glyph_box_cy", "dx", "dy",
           "rendered_cx", "rendered_cy", "art_glyph_cx", "art_glyph_cy", "art_dx", "art_dy"]]
    results = [["id", "status", "changed", "unsure"]]
    ids = BUTTONS + list(BACKDROPS)
    only = [v for v in os.environ.get("FE_ONLY", "").split(",") if v]
    for eid in ids:
        if only and eid not in only:
            continue
        doc = load_scene(eid)
        new, moves = fix_doc(doc, eid)
        refits = refit_paint(new, doc, eid, A[eid]) if not measure_only else []
        before = measure(eid, doc, A.get(eid))
        after = measure(eid, new, A.get(eid))
        for ver, rows in (("previous", before), ("new", after)):
            for r in rows:
                f = lambda v: "" if v is None else f"{v:.2f}"
                mt.append([eid, SCREEN[eid], r["state"], ver, f(r["disc_cx"]), f(r["disc_cy"]), f(r["disc_r"]), f(r["glyph_cx"]),
                           f(r["glyph_cy"]), f(r["glyph_cx"] - r["disc_cx"]), f(r["glyph_cy"] - r["disc_cy"]), f(r["render_cx"]),
                           f(r["render_cy"]), f(r["art_cx"]), f(r["art_cy"]),
                           "" if r["art_cx"] is None else f(r["art_cx"] - r["disc_cx"]),
                           "" if r["art_cy"] is None else f(r["art_cy"] - r["disc_cy"])])
                print(eid, r["state"], ver, "disc", round(r["disc_cx"], 2), round(r["disc_cy"], 2), "glyph",
                      round(r["glyph_cx"], 2), round(r["glyph_cy"], 2), "render", round(r["render_cx"], 2), round(r["render_cy"], 2),
                      "art", r["art_cx"], r["art_cy"])
        if measure_only:
            continue
        # ---- the real check: rendered glyph box centre within 1/8 px of the disc centre, and the states' glyphs identical
        # relative to their discs
        for r in after:
            assert abs(r["render_cx"] - r["disc_cx"]) <= 1.0 / Z + 1e-6 and abs(r["render_cy"] - r["disc_cy"]) <= 1.0 / Z + 1e-6, \
                (eid, r["state"], r["render_cx"], r["render_cy"], r["disc_cx"], r["disc_cy"])
        if len(after) > 1:
            g0 = after[0]
            for r in after[1:]:
                for k in ("w", "hl", "hw"):
                    assert r["arrow"][k] == g0["arrow"][k], (eid, k)
                rel0 = (g0["arrow"]["a"][0] + g0["move"][0] - g0["disc_cx"], g0["arrow"]["a"][1] + g0["move"][1] - g0["disc_cy"])
                rel = (r["arrow"]["a"][0] + r["move"][0] - r["disc_cx"], r["arrow"]["a"][1] + r["move"][1] - r["disc_cy"])
                assert abs(rel[0] - rel0[0]) < 1e-3 and abs(rel[1] - rel0[1]) < 1e-3, (eid, rel0, rel)
        # ---- images
        rows_img, rows_chk = [], []
        ov = None
        for (st_old, st_new), rb, ra in zip(zip(doc["states"], new["states"]), before, after):
            name = st_old["name"]
            art = A[eid][name] if name in A[eid] else A[eid]["idle"]
            cx, cy = ra["disc_cx"], ra["disc_cy"]
            so = only_button(st_old["scene"], cx, cy)
            sn = only_button(st_new["scene"], cx, cy)
            rect = (int(round(cx)) - HALF, int(round(cy)) - HALF, 2 * HALF, 2 * HALF)
            bg = wiped_bg(art, [so, sn], rect)
            x, y, w, h = rect
            a8 = to8(np.kron(art[y:y + h, x:x + w], np.ones((Z, Z, 1))))
            p8 = to8(c.render_crop(bg, so, rect, Z))
            n8 = to8(c.render_crop(bg, sn, rect, Z))
            rows_img.append([label(a8.copy(), f"{name}: art"), label(p8.copy(), "previous"), label(n8.copy(), "new (centred)")])
            ab = None
            if ra["art_cx"] is not None:
                ab = art_box(art, cx, cy, ra["disc_r"], "idle" if name == "idle" else "hover")
            pb = arrow_box(rb["arrow"], rb["move"])
            nb = arrow_box(ra["arrow"], ra["move"])
            ca = marks(a8.copy(), rect, cx, cy, ab, (0, 200, 255)) if ab else a8.copy()
            rows_chk.append([label(ca, f"{name}: art glyph box"), label(marks(p8.copy(), rect, cx, cy, pb, (0, 255, 0)), "previous"),
                             label(marks(n8.copy(), rect, cx, cy, nb, (0, 255, 0)), "new")])
            # the glyph mask relative to the disc centre, for the idle / hover overlay
            _, m = rendered_box(ra["layer"], cx, cy)
            ov = m.astype(np.uint8) if ov is None else ov + 2 * m.astype(np.uint8)
        to_png = strip(rows_img)
        to_png.save(os.path.join(OUT, eid + ".png"))
        chk = strip(rows_chk)
        if len(after) > 1:
            # overlay: idle glyph red, hover glyph green, both = yellow; each relative to its own disc centre
            col = np.zeros(ov.shape + (3,), np.uint8) + 255
            col[ov == 1] = (230, 40, 40)
            col[ov == 2] = (40, 180, 40)
            col[ov == 3] = (230, 200, 0)
            im = Image.fromarray(col)
            d = ImageDraw.Draw(im)
            n = ov.shape[0]
            d.line([(n / 2 - 20, n / 2), (n / 2 + 20, n / 2)], fill=(0, 0, 0))
            d.line([(n / 2, n / 2 - 20), (n / 2, n / 2 + 20)], fill=(0, 0, 0))
            diff = int((ov == 1).sum() + (ov == 2).sum())
            label(im, f"idle red / hover green / both yellow: {diff} cells differ")
            chk = strip([[chk], [im]])
            print(eid, "idle vs hover glyph cells differing at 8x:", diff)
        chk.save(os.path.join(OUT, eid + "_check.png"))
        with open(os.path.join(OUT, eid + ".json"), "w") as f:
            json.dump(new, f)
        mv = "; ".join(f"{s}: moved ({dx:+.2f}, {dy:+.2f}) px" for s, dx, dy in moves)
        pre = "; ".join(f"{r['state']} box centre {r['glyph_cx'] - r['disc_cx']:+.2f}, {r['glyph_cy'] - r['disc_cy']:+.2f} px off the disc"
                        for r in before)
        if eid in BACKDROPS:
            ch = (f"the idle Back button this backdrop carries (what the port shows when the button is not hovered): the arrow "
                  f"was {pre}; {mv}, the same arrow and place as {BACKDROPS[eid]}'s idle and hover; nothing else in the "
                  f"backdrop changed (PNG shows the button crop)")
        else:
            ch = f"arrow was {pre}; {mv}; shape unchanged (shaft, head length and width), idle and hover now the same glyph at the same place in the disc"
        for name, m0, m1, h0, h1 in refits:
            ch += (f"; {name} disc paint refitted on the art with its arrow inpainted away (the old fit held a yellow halo above "
                   f"the head that the centred arrow uncovered): mean |error| on the uncovered strip {h0} -> {h1} levels, on the rest "
                   f"of the disc {m0} -> {m1}")
        results.append([eid, "done", ch, UNSURE.get(eid, "")])
    if not measure_only:
        with open(os.path.join(OUT, "results.tsv"), "w", encoding="utf-8") as f:
            for r in results:
                f.write("\t".join(r) + "\n")
    with open(os.path.join(OUT, "measured.tsv" if not measure_only else "measured_preview.tsv"), "w", encoding="utf-8") as f:
        for r in mt:
            f.write("\t".join(r) + "\n")


UNSURE = {
    "world-back": "the hover disc now uses the idle circle (the previous hover fit sat 0.9 px higher and 0.76 px larger); "
                  "the art's yellow hover arrow is drawn 2 px higher than its navy idle arrow, now both are centred",
    "theme-back-button": "the art's yellow hover arrow is drawn 2 px higher than its navy idle arrow; both now centred in the disc",
    "diff-back-button": "the art's yellow hover arrow is drawn 1 px higher than its navy idle arrow; both now centred in the disc",
    "load-back-button": "centred on the bounding box: the head is the heavier end, so the arrow's mass sits left of centre by design; "
                        "the pixelated ring outside the disc in the PNG is the art's outer lip, which the approved scene leaves to "
                        "the picture (unchanged here)",
}

if __name__ == "__main__":
    main()
