"""P41 round 6 (log/roadmap/P41/FIX6_BRIEF.md): the Routing map and Buy Land backdrops as WIDE scenes (re/tools/uivec/wide.py).

Usage: py -3.12 wide_route.py [--deliver]

The problem: both screens are full-screen and fully opaque in the original (route screens_<mode>.pcx over route
screens_bottom.pcx; buy_land.pcx: no keyed pixel), but the approved scenes paint only the panels: the lilac field round the
slab, the map window's marbled floor (kept art) and, in wide mode, everything past x 0 / 800 are left transparent, so the
course and the HUD show through. (Member Roster is NOT affected: memberRoster.pcx keys every pixel outside its slab, the
original shows the course there too; checked, 864 opaque px uncovered, all slab-edge anti-aliasing.)

Read on the art (2026-11-26), frame coordinates:
- FIELD: one flat lilac, (148, 150, 198) on all four route tops and on the bottom sheet's sides (the G channel dithers
  148 / 150 / 156: mean 150.0 over the top rows of every route top, 150.7 at x 0..2 y 480..599), (148, 148, 198) on
  buy_land.pcx (every sample). No blotches, no gradient: a flat span rectangle, as theme-wide's navy field.
- ROUTE WINDOW OUTSIDE THE SLAB: the map window (approved route-bottom WINDOW: x 18.5..780.5, top y 408.5) runs on past the
  slab line to the screen edges: at x 0..2 and 797..799 the rows 409..473 are navy (mean (26, 34, 64), darker at the top:
  (18, 25, 48) at 409, (28, 36, 66) at 472), with a hard top edge between rows 408 and 409 and a dark lip above it (rows
  404..408 go 15 / 35 / 65 / 95 / 100 % from the field to (99, 99, 132): a soft edge at y 406.0, softness 2.5); below 473
  the disc's rim covers it. Drawn as two span items: the lip strip and the navy floor (a half plane under y 408.5, a linear
  paint from y 409.5 to 472.5, constant past both ends). Buy Land has no such strip: its window stays inside its slab
  (x 0..6 is field at every row).
- MARBLED FLOOR: kept art (route-marble kept_art round 1, rule 7: the giant dimpled golf ball). A picture entry cut from
  the bottom sheet (route: route screens_bottom.pcx x 18..782, y 268..600; Buy Land: buy_land.pcx x 18..782, y 269..600),
  matte "opaque", placed at its own frame position UNDER the approved scenes: everywhere in that rectangle except the
  window and the arch's slots the approved scenes paint opaque, so the cut's border never shows (checked: 4:3 render vs art
  below). The port must lift these two rectangles from the game files at run time (no art bytes in the repo).
- THE TWO DISCS (approved route-bottom items 3 and 4, buyland-backdrop items 2 and 3): circles centred ON the old frame
  edges (centres (4.54, 718.7) and (794.46, 718.7), r 243.88), so the art shows half of each. Their item boxes stop at
  x 0 / 800, which would cut them straight down the old frame edge on a wide window. They are drawn whole (the front end's
  rule for a ball the art's edge cuts): same shape, same bevel paint; only the two item boxes widen to the disc's extent.
  That is the only change to buyland-backdrop.json.
- ROUTE SLAB WALLS AT THE WINDOW ROWS (route-bottom.json, second change): inside the slab line the art's side walls
  (x 6..18 and 781..795) carry the window's lip (rows 404..408) and navy floor (rows 409..473), the same as outside the line;
  the approved slab fill painted lilac wall there (the window's navy then stopped at x 18.5 inside and restarted past the
  line outside). The slab fill's clip now also leaves out x < 18.5 and x > 780.5 from y 403.5 down (open_walls); the marble
  cut runs from x 5 to 795 so the art shows there; the slab line and the discs still draw over it.
- SLAB LINE SHADOW (both screens): right of the right slab line and below the bottom one the art has a 3 px soft shadow on
  the field; the approved scenes left those pixels to the art (transparent): shadow_entry.
Anchors: field, lip and navy floor are span; everything else centre (nothing in either screen is attached to an edge: the
discs carry the empty socket and the check button, which are composition).
"""
import json
import os
import sys
import time

import numpy as np
from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
UIVEC = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, UIVEC)
sys.path.insert(0, HERE)
import fit      # noqa: E402
import sdf      # noqa: E402
import wide     # noqa: E402
import parts    # noqa: E402,F401   (registers rm.smin)
import digits   # noqa: E402,F401   (registers rm.arc)

ROOT = fit.ROOT
SCENES = os.path.join(UIVEC, "scenes", "reports_maps")
OUT = os.path.join(ROOT, "log", "roadmap", "P41", "work6", "reports_maps")
IS = "Interface/infoscreens/"

ROUTE_FIELD = [148.0, 150.0, 198.0]
BUY_FIELD = [148.0, 148.0, 198.0]
LIP = {"edge": 406.0, "soft": 2.5, "rgb": [99.0, 99.0, 132.0]}
BUY_LIP_EDGE = 266.0
FLOOR = {"top": 408.5, "a": 409.5, "b": 472.5, "rgb_a": [18.0, 25.0, 48.0], "rgb_b": [28.0, 36.0, 66.0]}
DISC_BOX = {"left": [-240.0, 474.0, 249.0, 600.0], "right": [550.0, 474.0, 1039.0, 600.0]}
MODES = ("course", "employ", "aura", "value")


# ------------------------------------------------------------------------------------------------------------ scenes
def approved(eid, state=None):
    d = json.load(open(os.path.join(SCENES, eid + ".json")))["states"]
    return (d[state] if state else list(d.values())[0])["scene"]


def widened(eid, disc_items):
    """The approved scene with only the disc items' boxes widened (shape and paint untouched)."""
    doc = json.load(open(os.path.join(SCENES, eid + ".json")))
    st = list(doc["states"].values())[0]
    for i in disc_items:
        it = st["scene"][i]
        assert it["name"] == "disc" or it["layers"][0]["shape"]["t"] == "inter", (eid, i)
        cx = it["layers"][0]["shape"]["of"][0]["c"][0]
        it["box"] = list(DISC_BOX["left" if cx < 400 else "right"])
    return doc


WALL_CUT_Y = 403.5      # route-bottom: the slab's side walls stop here (the window's lip starts on row 404)


def open_walls(doc):
    """route-bottom only: the slab fill (item 0) no longer covers its two side walls (x < 18.5, x > 780.5) from y 403.5 down,
    so the map window's lip and navy floor show there as on the art (rows 404..473 at x 5..18 and 781..795 are the window's,
    the same as outside the slab line); the slab line (item 5) and the discs still draw over it."""
    st = list(doc["states"].values())[0]
    L = st["scene"][0]["layers"][0]
    assert st["scene"][0]["name"] == "slab" and L["clip"]["t"] == "sub"
    h = (660.0 - WALL_CUT_Y) / 2
    # x -10..24 and 775..810: they overlap the window's own cut (x 18.5..780.5) by 5.5 px, so the shared edge leaves no
    # half-covered column (touching cuts gave a 50 % lilac hairline at x 18.5 and 780.5)
    cuts = [{"t": "rrect", "c": [7.0, WALL_CUT_Y + h], "half": [17.0, h], "r": 0.0, "rot": 0.0},
            {"t": "rrect", "c": [792.5, WALL_CUT_Y + h], "half": [17.5, h], "r": 0.0, "rot": 0.0}]
    L["clip"]["b"] = {"t": "union", "of": [L["clip"]["b"]] + cuts}
    return doc


SHADOW = {"d": [1.5, 1.0], "soft": 3.0, "alpha": 0.5, "rgb": [24.0, 26.0, 70.0]}
ROUTE_BOTTOM_SLAB = {"t": "rrect", "c": [400.15, 368.9], "half": [395.35, 168.9], "r": 8.0, "rot": 0.0}   # route-bottom SLAB


def route_line(mode):
    """The Routing map's slab line for one mode: the union of the top scene's slab outline (course / aura / value: y 91.8..330;
    employ: y 246.8..480) and route-bottom's slab outline below y 253 (the bottom sheet's first row), w 1.7 as both draw it."""
    doc = json.load(open(os.path.join(SCENES, f"route-top-{mode}.json")))["states"]
    top = [it for it in list(doc.values())[0]["scene"] if it.get("name") == "slab"][0]
    rr = top["layers"][1]["shape"]["of"]
    bot = {"t": "inter", "of": [{"t": "outline", "of": ROUTE_BOTTOM_SLAB, "w": 1.7}, {"t": "half", "p": [0.0, 253.0], "n": [0.0, -1.0]}]}
    return {"t": "union", "of": [{"t": "outline", "of": rr, "w": 1.7}, bot]}


BUY_LINE = {"t": "outline", "of": {"t": "rrect", "c": [399.8, 403.75], "half": [392.0, 134.05], "r": 8.0, "rot": 0.0}, "w": 1.7}


def shadow_entry(line):
    """The slab line's soft shadow, down and to the right (light from the upper left). Read on both screens: right of the
    right line the art goes 119.7 / 134.9 / 145.6 lum on the 153.7 field over three px, below the bottom line 119 / 137 / 150,
    at every height; inside the slab the approved scenes paint their own. Fitted (grid search on x 797..799, rows 360, 400 and
    150 of the routing art): the line's outline moved (1.5, 1.0), softness 3.0, alpha 0.5 -> 112 / 127 / 140 against the art's
    115 / 132 / 140. Drawn under the panels, so only the part on the field shows."""
    return {"name": "slab line shadow", "anchor": "centre", "box": [-10.0, 80.0, 810.0, 560.0],
            "layers": [{"shape": {"t": "move", "of": line, "d": SHADOW["d"]}, "soft": SHADOW["soft"], "fill": SHADOW["rgb"],
                        "alpha": SHADOW["alpha"]}]}


def field_entry(rgb):
    return {"name": "lilac field", "anchor": "span", "box": [wide.SPAN_X[0], -20.0, wide.SPAN_X[1], 620.0],
            "layers": [{"shape": {"t": "rrect", "c": [400.0, 300.0], "half": [3000.0, 400.0], "r": 0.0}, "fill": list(rgb)}]}


def lip_layer(edge, clip=None):
    L = {"shape": {"t": "half", "p": [0.0, edge], "n": [0.0, -1.0]}, "soft": LIP["soft"], "fill": LIP["rgb"]}
    if clip:
        L["clip"] = clip
    return L


def buy_lip_entry():
    """Buy Land: the map window's lip above the slab's top line (y 269.7): rows 264..267 go 15 / 38 / 66 / 100 % from the field
    to (99, 99, 132) at x 20..480 (the Routing map's lip one row higher: edge 266.0, softness 2.5); it fades in over x 15..19
    where the slab's top-left corner (r 8) ends, and runs under the two pills; the approved backdrop leaves it transparent.
    One soft rectangle: x 17.0..782.6 (the straight top run, mirrored about the slab centre 399.8; its soft ends give the fade
    at the corners), top edge 266.0, bottom under the window (the marble and the wall paint over it)."""
    L = {"shape": {"t": "rrect", "c": [399.8, BUY_LIP_EDGE + 17.0], "half": [382.8, 17.0], "r": 0.0, "rot": 0.0},
         "soft": LIP["soft"], "fill": LIP["rgb"]}
    return {"name": "map window lip", "anchor": "centre", "box": [10.0, 258.0, 790.0, 290.0], "layers": [L]}


def window_entries():
    lip = lip_layer(LIP["edge"])
    floor = {"shape": {"t": "half", "p": [0.0, FLOOR["top"]], "n": [0.0, -1.0]},
             "fill": {"linear": {"a": [0.0, FLOOR["a"]], "b": [0.0, FLOOR["b"]], "stops": [[0.0, FLOOR["rgb_a"]], [1.0, FLOOR["rgb_b"]]]}}}
    return [{"name": "map window lip (outside the slab)", "anchor": "span", "box": [wide.SPAN_X[0], 400.0, wide.SPAN_X[1], 620.0], "layers": [lip]},
            {"name": "map window floor (outside the slab)", "anchor": "span", "box": [wide.SPAN_X[0], 405.0, wide.SPAN_X[1], 620.0], "layers": [floor]}]


def marble(sheet, rect):
    return {"name": "marbled floor (kept art)", "anchor": "centre",
            "picture": {"sheet": IS + sheet, "rect": list(rect), "at": list(rect[:2]), "matte": {"kind": "opaque"}}}


def route_scene(mode="course"):
    return {"id": "route-wide" if mode == "course" else f"route-wide-{mode}", "screen": "Routing map", "mode": mode,
            "design_x": list(wide.DESIGN_X), "frame": [800, 600], "ground": {"flat": ROUTE_FIELD},
            "changed_refs": {"route-bottom": "log/roadmap/P41/work6/reports_maps/route-bottom.json (disc item boxes; side walls open at the window rows)"},
            "draw": [field_entry(ROUTE_FIELD), shadow_entry(route_line(mode))] + window_entries() + [
                {"name": f"top: banner, list, pills, title plate ({mode})", "anchor": "centre", "ref": f"route-top-{mode}"},
                marble("route screens_bottom.pcx", (5, 268, 790, 332)),
                {"name": "bottom: slab, arch, mode bubbles, board frame, discs, socket, check", "anchor": "centre", "ref": "route-bottom"}]}


def buy_scene():
    return {"id": "buyland-wide", "screen": "Buy Land", "design_x": list(wide.DESIGN_X), "frame": [800, 600],
            "ground": {"flat": BUY_FIELD},
            "changed_refs": {"buyland-backdrop": "log/roadmap/P41/work6/reports_maps/buyland-backdrop.json (disc item boxes only)"},
            "draw": [field_entry(BUY_FIELD), shadow_entry(BUY_LINE), buy_lip_entry(), marble("buy_land.pcx", (18, 269, 764, 331)),
                     {"name": "slab, window rim, pills, discs, capsules, balls, sockets, check, lot board", "anchor": "centre",
                      "ref": "buyland-backdrop"}]}


def refs(route_bottom, buy_backdrop):
    r = {f"route-top-{m}": approved(f"route-top-{m}") for m in MODES}
    r["route-bottom"] = list(route_bottom["states"].values())[0]["scene"]
    r["buyland-backdrop"] = list(buy_backdrop["states"].values())[0]["scene"]
    return r


# ------------------------------------------------------------------------------------------------------------ art
def sheet(name):
    i, p = fit.load_sheet(IS + name)
    return p[i].astype(float), i


def route_art(mode="course"):
    top, _ = sheet(f"route screens_{mode}.pcx")
    bot, _ = sheet("route screens_bottom.pcx")
    a = bot.copy()
    a[:253] = top[:253]
    return a


def buy_art():
    return sheet("buy_land.pcx")[0]


# ------------------------------------------------------------------------------------------------------------ images
def checker(h, w, s=8):
    jj, ii = np.mgrid[0:h, 0:w]
    c = (((ii // s) + (jj // s)) % 2).astype(float)
    return np.dstack([c * 40 + 150] * 3)


def old_composite(items_lists, s=1.0, x0=0.0, w=800, h=600, y0=0.0):
    """The approved scenes alone over a grey checker (what the port showed: anything behind them shows through)."""
    img = checker(h, w)
    for items in items_lists:
        sdf.render(items, img, s, origin=(x0, y0))
    return np.clip(img + 0.5, 0, 255).astype(np.uint8)


def u8(a):
    return np.clip(np.asarray(a, float) + 0.5, 0, 255).astype(np.uint8)


def label(img, text):
    im = Image.fromarray(img)
    d = ImageDraw.Draw(im)
    d.rectangle([0, 0, 7 * len(text) + 8, 14], fill=(255, 255, 255))
    d.text((4, 2), text, fill=(200, 0, 0))
    return np.asarray(im)


def three(art, old, new, labels=("art", "approved (port today)", "new")):
    gap = np.full((art.shape[0], 6, 3), 255, np.uint8)
    return np.concatenate([label(u8(art), labels[0]), gap, label(old, labels[1]), gap, label(new, labels[2])], 1)


def zoom(img, x, y, w, h, k):
    return np.asarray(Image.fromarray(img[y:y + h, x:x + w]).resize((w * k, h * k), Image.NEAREST))


def stats(new, art, mask):
    d = np.abs(new.astype(float) - art)[mask].mean(-1)
    return float(d.mean()), float(np.percentile(d, 99)) if d.size else 0.0


# ------------------------------------------------------------------------------------------------------------ deliver
def deliver_screen(sid, scene, art, rf, old_lists, crops, log=print):
    t = time.time()
    new = wide.render(scene, 4 / 3, 600, rf)
    old = old_composite(old_lists)
    Image.fromarray(three(art, old, new)).save(os.path.join(OUT, f"{sid}.png"))
    # 3x crops of the backdrop regions: art | approved | new per row
    rows = []
    for (x, y, w, h) in crops:
        a, o, n = (zoom(u8(art), x, y, w, h, 3), zoom(old, x, y, w, h, 3), zoom(new, x, y, w, h, 3))
        r = three(a, o, n, (f"art x{x} y{y}", "approved", "new"))
        rows.append(r)
    W = max(r.shape[1] for r in rows)
    rows = [np.pad(r, ((0, 6), (0, W - r.shape[1]), (0, 0)), constant_values=255) for r in rows]
    Image.fromarray(np.concatenate(rows, 0)).save(os.path.join(OUT, f"{sid}_zoom3.png"))
    for tag, a, hgt in (("169", 16 / 9, 1080), ("219", 21 / 9, 1080), ("329", 32 / 9, 540)):
        img = wide.render(scene, a, hgt, rf)
        wide.save_png(wide.ticks(img, a, hgt / 600.0), os.path.join(OUT, f"{sid}_{tag}.png"))
    # 8x across the old frame edges (seams) and far out
    cw, ch = 50, 50
    tiles = []
    for (cx, cy, lab) in [(0, 440, "x=0"), (800, 440, "x=800"), (0, 520, "x=0"), (800, 520, "x=800"), (-300, 470, "far left"), (1100, 470, "far right"),
                          (0, 60, "x=0"), (800, 60, "x=800")]:
        img = wide.render(scene, 32 / 9, 600, rf, origin_x=cx - cw / 2, width=cw * 8, s=8.0, y0=cy - ch / 2, h_px=ch * 8)
        im = Image.fromarray(img)
        d = ImageDraw.Draw(im)
        if lab.startswith("x="):
            d.line([(cw * 4, 0), (cw * 4, 12)], fill=(255, 0, 0), width=2)
        d.text((4, ch * 8 - 14), f"{lab} y={cy}", fill=(255, 0, 0))
        tiles.append(np.asarray(im))
    g = np.full((400, 6, 3), 255, np.uint8)
    r1 = np.concatenate([tiles[0], g, tiles[1], g, tiles[4], g, tiles[6]], 1)
    r2 = np.concatenate([tiles[2], g, tiles[3], g, tiles[5], g, tiles[7]], 1)
    Image.fromarray(np.concatenate([r1, np.full((6, r1.shape[1], 3), 255, np.uint8), r2], 0)).save(os.path.join(OUT, f"{sid}_zoom8.png"))
    log(f"  {sid}: {time.time() - t:.0f}s")
    return new


def check_43(name, new, art, cover_lists):
    """How the drawn 4:3 screen compares with the art: on the pixels the approved scenes left uncovered (the backdrop this
    round adds) and on all pixels."""
    a0 = np.zeros((600, 800, 3))
    a1 = np.full((600, 800, 3), 255.0)
    for items in cover_lists:
        sdf.render(items, a0, 1.0)
        sdf.render(items, a1, 1.0)
    A = 1 - (a1 - a0).max(-1) / 255.0
    hole = A < 0.98
    m_h, p_h = stats(new, art, hole)
    m_a, p_a = stats(new, art, np.ones(hole.shape, bool))
    full = A < 0.02
    m_f, p_f = stats(new, art, full)
    d = np.abs(new.astype(float) - art).mean(-1)
    print(f"  {name}: uncovered px {int(hole.sum())}: mean |new - art| {m_h:.2f}, p99 {p_h:.1f}; fully uncovered {int(full.sum())}: "
          f"mean {m_f:.2f}, p99 {p_f:.1f}, over 15: {int((full & (d > 15)).sum())}; whole screen mean {m_a:.2f}, p99 {p_a:.1f}")
    return {"uncovered_px": int(hole.sum()), "uncovered_mad": round(m_h, 2), "uncovered_p99": round(p_h, 1),
            "fully_uncovered_px": int(full.sum()), "fully_uncovered_mad": round(m_f, 2), "fully_uncovered_p99": round(p_f, 1),
            "fully_uncovered_over15": int((full & (d > 15)).sum()), "all_mad": round(m_a, 2)}


def main(args):
    os.makedirs(OUT, exist_ok=True)
    rb = open_walls(widened("route-bottom", (3, 4)))
    bb = widened("buyland-backdrop", (2, 3))
    json.dump(rb, open(os.path.join(OUT, "route-bottom.json"), "w"))
    json.dump(bb, open(os.path.join(OUT, "buyland-backdrop.json"), "w"))
    rf = refs(rb, bb)
    res = {}
    for mode in MODES:
        sc = route_scene(mode)
        json.dump(sc, open(os.path.join(OUT, sc["id"] + ".json"), "w"), indent=1)
        art = route_art(mode)
        lists = [approved(f"route-top-{mode}"), approved("route-bottom")]
        if mode == "course" or "--all" in args:
            new = deliver_screen(sc["id"], sc, art, rf, lists, [(0, 0, 200, 130), (600, 0, 200, 130), (0, 380, 200, 220), (600, 380, 200, 220)])
        else:
            new = wide.render(sc, 4 / 3, 600, rf)
            Image.fromarray(three(art, old_composite(lists), new)).save(os.path.join(OUT, sc["id"] + ".png"))
        res[sc["id"]] = check_43(sc["id"], new, art, lists)
    sc = buy_scene()
    json.dump(sc, open(os.path.join(OUT, "buyland-wide.json"), "w"), indent=1)
    art = buy_art()
    lists = [approved("buyland-backdrop")]
    new = deliver_screen("buyland-wide", sc, art, rf, lists, [(0, 0, 200, 130), (600, 0, 200, 130), (0, 250, 200, 350), (600, 250, 200, 350)])
    res["buyland-wide"] = check_43("buyland-wide", new, art, lists)
    # the changed element scenes: what changed inside the frame, and the element on a wide strip over the new backdrop
    for eid, doc, sc, art in (("route-bottom", rb, route_scene("course"), route_art("course")), ("buyland-backdrop", bb, buy_scene(), buy_art())):
        a = np.zeros((600, 800, 3))
        b = np.zeros((600, 800, 3))
        new_items = list(doc["states"].values())[0]["scene"]
        sdf.render(approved(eid), a, 1.0)
        sdf.render(new_items, b, 1.0)
        ch = np.abs(a - b).max(-1) > 0.5
        ys, xs = np.nonzero(ch)
        res[eid] = {"changed_px_in_frame": int(ch.sum()), "changed_bbox": [int(xs.min()), int(ys.min()), int(xs.max()), int(ys.max())] if ch.any() else None}
        print(f"  {eid}: {res[eid]}")
        under = dict(sc, draw=[e for e in sc["draw"] if e.get("ref") != eid])
        x0, w, y0, h = -260.0, 1320, 240.0, 360
        base = wide.render(under, 32 / 9, 600, rf, origin_x=x0, width=w, s=1.0, y0=y0, h_px=h).astype(float)
        o, n = base.copy(), base.copy()
        sdf.render(approved(eid), o, 1.0, origin=(x0, y0))
        sdf.render(new_items, n, 1.0, origin=(x0, y0))
        ar = checker(h, w)
        ar[:, int(-x0):int(-x0) + 800] = art[int(y0):int(y0) + h]
        panels = []
        for im, lab in ((ar, "art (frame only)"), (o, "approved element over the new backdrop"), (n, "new element over the new backdrop")):
            im = u8(im)
            for fx in (0, 800):
                im[:12, int(fx - x0)] = [255, 0, 0]
            panels += [label(im, lab), np.full((6, w, 3), 255, np.uint8)]
        if eid == "route-bottom":
            # 4x across both walls at the window rows: art | approved | new
            full_o = wide.render(under, 4 / 3, 600, rf).astype(float)
            full_n = full_o.copy()
            sdf.render(approved(eid), full_o, 1.0)
            sdf.render(new_items, full_n, 1.0)
            row = []
            for (cx, cy) in ((0, 392), (760, 392)):
                t = [zoom(u8(im), cx, cy, 40, 90, 4) for im in (art, full_o, full_n)]
                row += [t[0], np.full((360, 4, 3), 255, np.uint8), t[1], np.full((360, 4, 3), 255, np.uint8), t[2], np.full((360, 16, 3), 255, np.uint8)]
            row = np.concatenate(row, 1)
            panels.append(np.pad(label(row, "4x walls x0..40 / x760..800, y392..482: art | approved | new"), ((0, 0), (0, w - row.shape[1]), (0, 0)), constant_values=255))
        Image.fromarray(np.concatenate(panels, 0)).save(os.path.join(OUT, f"{eid}.png"))
    json.dump(res, open(os.path.join(OUT, "check.json"), "w"), indent=1)
    return res


if __name__ == "__main__":
    t0 = time.time()
    main(sys.argv[1:])
    print(f"{time.time() - t0:.0f}s")
