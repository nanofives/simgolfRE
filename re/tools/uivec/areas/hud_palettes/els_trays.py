"""The four palette trays (terrain, building, elevation, amenities): one lavender tray shape, its tab bubbles and bulldozer
measured on the tray itself, and each palette's content.

Read on the cuts (2026-11-24; all four share one silhouette, checked pixel for pixel): the tray's outline is the union of
two circles round the tab bubbles (fitted to the cut's edge: (271.5, 500.7) r 19.2 and (237.45, 527.5) r 19.0) and a body
whose top edge runs at y 494 and whose lower left edge curves down to x 216; a light rail along the top (y 494..497.5), a
darker edge down the right end (x 797..800). Sheet coordinates throughout (the scene is in the tray's sheet).
Content:
  terrain    a pale double oval on the back wall and a floor of two checker rows (squares 31 px wide; the top row 41 px tall
             starting at a diagonal edge, the bottom row from x 301.5 to the right end), light and grey squares measured;
  building   a recessed back panel (rounded top corners) and nine slot diamonds composed from building-slot's own scenes at
             their matched places (top row tall, bottom row short, the bottom row drawn over the top row's pillars);
  elevation  the four pedestals composed from the elevation-* scenes at their matched places (their kept pictures stay art);
  amenities  a deep recess with a curved left side and a rounded right end above a light ledge, the five amenity tab bubbles
             composed from their own scenes, and the three building thumbnails kept as art.
Embedded bubbles (tabs, bulldozer) are built with bel.py on a window of the tray (rim, body and glyph measured there).
"""
import json
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hp  # noqa: E402
import bel  # noqa: E402
import bubble as B  # noqa: E402
import geom as G  # noqa: E402
import el_bulldozer as BD  # noqa: E402
import els_tabs as T  # noqa: E402
from hp import sdf, fit  # noqa: E402

R1 = (271.5, 500.7, 19.2)
R2 = (237.45, 527.47, 19.0)
BODY = G.poly((252.5, 494.0), (800.0, 494.0), (800.0, 600.0), (216.0, 600.0), (216.0, 584.0), (217.0, 575.0), (218.0, 570.0),
              (220.0, 561.0), (222.0, 555.0), (224.0, 550.0), (226.5, 543.5), (240.0, 520.0), (252.5, 500.0))
SIL = G.U(G.circle(*R1), G.circle(*R2), BODY)
# Round 3 (building-palette, note "Cutoff on the edges"): BODY's left edge ran straight from (252.5, 500) to (240, 520), which
# put lavender over a green wedge the art has between the two tab rings (x 249..255, y 498..513; the rings do not touch:
# their centres are 43.3 apart for radii 19.2 + 19.0). BODY3 runs from the lower ring's centre through the narrow lavender
# neck the art has between them (read row by row: neck bottom at (251.5, 513.5), lavender at (253, 514.6) and (255.6, 512))
# to the upper ring's centre, and its foot keeps clear of the art's transparent pixels at x 216, y 580..583.
BODY3 = G.poly((271.5, 494.0), (800.0, 494.0), (800.0, 600.0), (216.0, 600.0), (216.0, 584.6), (217.2, 575.0), (218.0, 570.0),
               (220.0, 561.0), (222.0, 555.0), (224.0, 550.0), (226.5, 543.5), (237.45, 527.47), (250.5, 515.2), (253.0, 514.6),
               (255.6, 512.0), (271.5, 500.7))
SIL3 = G.U(G.circle(*R1), G.circle(*R2), BODY3)
# the light ring the art draws round the lower tab's socket (a circle fitted to its ridge: centre (237.0, 526.6), r 22.5,
# residuals within 1.5 px over y 518..551), from the neck under the upper ring round to the left edge, and the light rim
# that runs on down the tray's left outline (ridge read row by row: x 224 at y 552, 221 at 560..564, 216.5 at 590..595)
RING3 = (237.0, 526.6, 22.5, -40.0, 157.0)
LEFT_RIM3 = [(225.2, 549.0), (223.6, 554.0), (222.2, 559.0), (221.4, 564.0), (220.6, 569.0), (219.8, 574.0), (219.0, 579.0),
             (218.2, 584.0), (217.5, 589.0), (217.0, 594.0), (216.8, 600.5)]
GLYPH = {s["id"]: s for s in T.SPECS}


def tab_glyph(tab_id, cx, cy, wx0, wy0, local_c=(17.5, 16.55)):
    return [(n, G.moved(s, cx - wx0 - local_c[0], cy - wy0 - local_c[1]), k) for n, s, k in GLYPH[tab_id]["glyph"]]


def window(rgb, mask, x0, y0, cx, cy, half=19):
    wx0, wy0 = int(cx - half), int(cy - half)
    i0, j0 = wx0 - x0, wy0 - y0
    i1, j1 = i0 + 2 * half + 1, j0 + 2 * half + 1
    i0c, j0c = max(i0, 0), max(j0, 0)
    sub = rgb[j0c:j1, i0c:i1]
    m = mask[j0c:j1, i0c:i1]
    return wx0 + (i0c - i0), wy0 + (j0c - j0), sub, m


def embedded_bubble(el, rgb, mask, x0, y0, cx, cy, r, glyph_fn, name, kdir=1.0):
    """A bubble built on a window of the tray: its rim fitted first, then the glyph placed at the fitted centre."""
    wx0, wy0, sub, m = window(rgb, mask, x0, y0, cx, cy)
    c = B.fit_rim(sub, m, wx0, wy0, dark=70, guess=(cx, cy, r), arc=(110.0, 350.0))
    cx, cy = c[0], c[1]
    spec = dict(id=el, ref="w", circle=(cx - wx0, cy - wy0, r), ground=(), shadow=False, body_kdir=kdir,
                glyph=glyph_fn(wx0, wy0, cx, cy), _crops={"w": ("w", "", wx0, wy0, sub, m)})
    scenes, _, info = bel.build(spec, write=False, verbose=False)
    item = scenes["w"][0]
    for L in item["layers"]:
        L["name"] = name + ": " + L.get("name", "rim / body")
    return item, info["w"]


def measured_linear(rgb, keep, x0, y0):
    return B.measure_linear(rgb, keep, x0, y0) if keep.sum() > 3 else [128, 128, 160]


def mode_rgb(v):
    u, n = np.unique(v.astype(int), axis=0, return_counts=True)
    return [float(c) for c in u[int(np.argmax(n))]]


def match(rgb, mask, x0, y0, crop_rgb, sel, guess, reach=6):
    """The top left (sheet) where a cut's selected pixels best match the tray (mean abs RGB error)."""
    h, w = sel.shape
    best = None
    ys, xs = np.nonzero(sel)
    v = crop_rgb[ys, xs].astype(float)
    for dy in range(-reach, reach + 1):
        for dx in range(-reach, reach + 1):
            tx, ty = guess[0] + dx, guess[1] + dy
            jj, ii = ys + ty - y0, xs + tx - x0
            ok = (jj >= 0) & (jj < rgb.shape[0]) & (ii >= 0) & (ii < rgb.shape[1])
            if ok.mean() < 0.6:
                continue
            e = np.abs(rgb[jj[ok], ii[ok]].astype(float) - v[ok]).mean()
            if best is None or e < best[0]:
                best = (e, tx, ty)
    return best


def composed(el_id, state, at, drop=("ground", "back wall", "upper wall", "ledge", "floor", "a dark line", "recess",
                                     "the bubble's cast shadow", "the cylinder's soft shadow")):
    """An element's built scene (its json), moved so its cut's top left sits at `at`, without its ground layers."""
    d = json.load(open(hp.scene_json(el_id)))
    s = d["states"][state]
    cx0, cy0 = s["cut"][0], s["cut"][1]
    out = []
    for it in s["scene"]:
        it = G.moved(it, at[0] - cx0, at[1] - cy0)
        if "layers" in it:
            it["layers"] = [L for L in it["layers"] if not any(str(L.get("name", "")).startswith(p) for p in drop)]
        out.append(it)
    return out


def chrome(rgb, mask, x0, y0, content_masks, round3=False):
    """The common tray: silhouette, rail, right edge, the two tab sockets. content_masks: pixels the body paint skips.
    round3: the building tray's corrected silhouette (SIL3) with the lower tab's light ring and the left rim."""
    h, w = mask.shape
    X, Y = B.grid(x0, y0, w, h)
    discs = (np.hypot(X - R1[0], Y - R1[1]) < R1[2]) | (np.hypot(X - R2[0], Y - R2[1]) < R2[2])
    plain = mask & ~discs & ~content_masks & (Y > 499) & (X < 795)
    sil = SIL3 if round3 else SIL
    L = [{"shape": sil, "fill": measured_linear(rgb, plain, x0, y0), "name": "tray: lavender body and the two tab shoulders"}]
    if round3:
        lum = rgb.astype(float) @ np.array([0.3, 0.59, 0.11])
        ring = G.arc(*RING3[:3], RING3[3], RING3[4], 3.0)
        k = mask & (sdf.sd(ring, X, Y) < -0.4) & ~discs & (lum > 165)
        L.append({"shape": G.inter(ring, sil), "fill": mode_rgb(rgb[k]), "soft": 0.5,
                  "name": "tray: the light ring round the lower tab's socket (r 22.5, from the neck round to the left edge)"})
        rim = G.line(*LEFT_RIM3, w=2.6, cap="round")
        k = mask & (sdf.sd(rim, X, Y) < -0.4) & (lum > 150)
        L.append({"shape": G.inter(rim, sil), "fill": measured_linear(rgb, k, x0, y0), "soft": 0.5,
                  "name": "tray: the light rim down the left outline"})
        # the art's outline is one dark pixel on the staircase steps (16..57 grey-navy); drawn as a 0.9 px line in their median
        edge = G.inter(G.outlined(sil, 0.9), G.rect(214.0, 545.0, 226.0, 588.0))
        k = mask & (sdf.sd(G.outlined(sil, 2.0), X, Y) < 0.0) & (lum < 90) & (X < 226) & (Y > 545) & (Y < 588)
        if k.sum() > 3:
            L.append({"shape": edge, "fill": [float(c) for c in np.median(rgb[k], axis=0)],
                      "name": "tray: the thin dark line along the left outline"})
    for R, nm in ((R1, "upper"), (R2, "lower")):
        ring = G.circle(*R)
        k = mask & (np.hypot(X - R[0], Y - R[1]) < R[2]) & (np.hypot(X - R[0], Y - R[1]) > R[2] - 5.5)
        p = B.measure_bevel_shape(rgb, k, x0, y0, ring, [0.0, 1.0, 2.0, 3.5, 5.0])
        p["bevel"]["of"] = ring
        L.append({"shape": ring, "fill": p, "name": f"tray: the {nm} tab's socket (lit as measured)"})
    rail = G.rect(254.0, 494.0, 797.0, 497.5)
    k = mask & (sdf.sd(rail, X, Y) < -0.2)
    L.append({"shape": rail, "fill": measured_linear(rgb, k, x0, y0), "name": "tray: light rail along the top"})
    edge = G.rect(797.0, 494.0, 800.0, 600.0)
    k = mask & (sdf.sd(edge, X, Y) < -0.2)
    L.append({"shape": edge, "fill": measured_linear(rgb, k, x0, y0), "name": "tray: darker edge down the right end"})
    return L


def build(el_id, tab1, tab2, content_fn, how):
    st, src, x0, y0, rgb, mask = hp.crops(el_id)[0]
    h, w = mask.shape
    X, Y = B.grid(x0, y0, w, h)
    content_layers, content_mask, extra_items = content_fn(rgb, mask, x0, y0, X, Y)
    items = [{"box": [x0 - 2, y0 - 2, x0 + w + 2, y0 + h + 2],
              "layers": chrome(rgb, mask, x0, y0, content_mask, round3=(el_id == "building-palette")) + content_layers}]
    items += extra_items
    # the tabs and the bulldozer, measured on the tray
    for R, tid, nm in ((R1, tab1, "upper tab"), (R2, tab2, "lower tab")):
        it, inf = embedded_bubble(el_id, rgb, mask, x0, y0, R[0], R[1], 14.5,
                                  lambda wx0, wy0, cx, cy, tid=tid: tab_glyph(tid, cx, cy, wx0, wy0), nm + " (" + tid + ")")
        items.append(it)
    bx, by = BULL[el_id]
    it, inf = embedded_bubble(el_id, rgb, mask, x0, y0, bx, by, 14.0,
                              lambda wx0, wy0, cx, cy: BD.glyph(cx - wx0 - 15.5, cy - wy0 - 15.8), "bulldozer")
    items.append(it)
    scenes = {st: items}
    sc = hp.score(el_id, scenes, z=3)
    print(f"  {el_id}: mad {sc[st][0]:.2f} edge {sc[st][1]}")
    hp.compare(el_id, scenes, z=2)
    hp.save_scene(el_id, scenes, {"how": how})
    return sc


# the bulldozer bubble's centre on each tray (read on the cut; the fit inside bel refines it)
BULL = {"terrain-palette": (261.5, 578.0), "building-palette": (274.5, 569.0), "elevation-palette": (266.5, 563.0),
        "amenities-palette": (249.0, 566.0)}


def terrain_content(rgb, mask, x0, y0, X, Y):
    L, cm = [], np.zeros(mask.shape, bool)
    shelf = G.U(G.oval(330.5, 516.5, 37.0, 9.0), G.oval(338.0, 525.0, 15.0, 6.0))
    k = mask & (sdf.sd(shelf, X, Y) < -0.5)
    L.append({"shape": shelf, "fill": mode_rgb(rgb[k]), "name": "a pale double oval on the back wall"})
    cm |= sdf.sd(shelf, X, Y) < 1
    top_y0, top_y1, bot_y1 = 531.0, 572.0, 600.0
    diag = G.poly((254.5, top_y0), (301.5, top_y0), (301.5, top_y1))
    xs_top = [270.5 + 31.0 * k for k in range(17)]
    xs_bot = [301.5 + 31.0 * k for k in range(17)]
    light, grey = [], []
    for k in range(16):
        a, b = xs_top[k], min(xs_top[k + 1], 766.5)
        sq = G.rect(max(a, 254.5), top_y0, b, top_y1)
        if k == 0:
            sq = G.U(diag, G.rect(270.5, top_y0, 301.5, top_y1))
            sq = G.inter(sq, G.half((254.5, top_y0), (-41.0 / 62.7, 47.0 / 62.7)))
        (light if k % 2 == 0 else grey).append(sq)
    for k in range(16):
        a, b = xs_bot[k], min(xs_bot[k + 1], 797.0)
        (light if k % 2 == 0 else grey).append(G.rect(a, top_y1, b, bot_y1))
    for nm, sqs in (("light", light), ("grey", grey)):
        u = G.U(*sqs)
        k = mask & (sdf.sd(u, X, Y) < -1.0)
        L.append({"shape": u, "fill": mode_rgb(rgb[k]), "name": f"checker floor: the {nm} squares"})
        cm |= sdf.sd(u, X, Y) < 1
    edge = G.line((254.5, top_y0 + 1.0), (300.5, top_y1 + 0.5), w=1.6)
    k = mask & (sdf.sd(edge, X, Y) < -0.2)
    L.append({"shape": edge, "fill": mode_rgb(rgb[k]) if k.any() else [100, 100, 130], "name": "the floor's diagonal left edge (a dark line)"})
    return L, cm, []


def building_content(rgb, mask, x0, y0, X, Y):
    """Fix round: the slots are this round's building-slot boxes; their back rim is left out (in the tray the wall and the dark
    back panel are what shows behind the tiles); the panel's paint is measured on its own pixels only (round 1 measured it over
    the slots too and came out light); the boxes' faces run on down to the tray's bottom edge."""
    L, cm = [], np.zeros(mask.shape, bool)
    lum = rgb.astype(float) @ np.array([0.3, 0.59, 0.11])
    # round 3: the recess read again column by column (rows 560..599, x 316..770) and row by row (x 326..340 and 744..758):
    # it spans x 319..761 from y 520 (round 2 had 321..763 from 521 with r 8 corners); its LEFT wall is in shadow (a soft
    # step 137, 128, 112, 103 over x 319..322, then a dark band 86 at x 323..325), its TOP wall steps down 137 .. 85 to a dark
    # line (69 / 58) at y 524 over a band of 88 at y 525..527, and its RIGHT wall is lit (a light lip 158..178 at x 761..765)
    panel = G.rect(319.0, 520.0, 761.0, 606.0, 4.0)
    items, slot_px = [], np.zeros(mask.shape, bool)
    tall = [c for c in hp.crops("building-slot") if c[0] == "idle"][0]
    short = [c for c in hp.crops("building-slot") if c[0] == "idle short"][0]
    for (cx, cy), cut, st in ([((381 + 80 * i, 528), tall, "idle") for i in range(5)] +
                              [((421 + 80 * i, 568), short, "idle short") for i in range(4)]):
        _, _, ox, oy, crgb, cmask = cut
        hh, ww = cmask.shape
        jj, ii = np.mgrid[0:hh, 0:ww]
        sel = cmask & (jj < 30)
        best = match(rgb, mask, x0, y0, crgb, sel, (int(cx - ww / 2), int(cy - 26)), reach=8)
        its = composed("building-slot", st, (best[1], best[2]), drop=("back rim",))
        for it in its:
            # round 3 (note "Cutoff on the edges"): a composed slot keeps its cut's render box, which ended at the slot cut's
            # bottom (y 568 for the tall row), so the faces extended below stopped there in a straight line. The box now runs
            # to the tray's bottom edge like the faces do.
            if "box" in it:
                it["box"] = [it["box"][0] - 1, it["box"][1], it["box"][2] + 1, 612]
            lays = it.get("layers", [])
            for Ly in lays:
                nm = Ly.get("name", "")
                if nm.startswith("the box's lit left face") or nm.startswith("the box's shaded right face"):
                    left = "left face" in nm
                    bx = best[1] + 38.5
                    x_a, x_b = (best[1] - 1, bx) if left else (bx, best[1] + 78)
                    Ly["shape"] = G.U(Ly["shape"], G.rect(x_a, best[2] + 40.0, x_b, 610.0))
                    # the box's left edge where the art has it (its lit face starts at x + 1 against the panel, column read)
                    Ly["clip"] = G.inter(G.half((bx, 0.0), (1.0, 0.0)) if left else G.half((bx, 0.0), (-1.0, 0.0)),
                                         G.rect(best[1] + 0.6, best[2] - 2, best[1] + 79, 612.0))
            # round 3: the box's right edge, a dark vertical line from the tile's right corner to the tray's bottom (columns:
            # 93, 85, 85 at x + 76..79 over the next box's face, 59, 58 then a soft 77, 87 onto the panel for the last tall box)
            if lays:
                ln = G.line((best[1] + 78.0, best[2] + 27.0), (best[1] + 78.0, 612.0), w=2.4)
                k = mask & (sdf.sd(ln, X, Y) < -0.3) & (lum < 100) & (Y > best[2] + 31)
                fill = mode_rgb(rgb[k]) if k.sum() > 3 else ([58, 58, 82] if st == "idle" else [74, 76, 104])
                at = next((i for i, Ly in enumerate(lays) if str(Ly.get("name", "")).startswith("tile:")), len(lays))
                lays.insert(at, {"shape": ln, "fill": fill, "soft": 0.5,
                                 "name": "the box's right edge: a dark line from the tile's right corner down"})
        items += its
        j0, i0 = best[2] - y0, best[1] - x0
        slot_px[max(j0, 0):, max(i0, 0):i0 + ww] |= True
    k = mask & (sdf.sd(panel, X, Y) < -1.0) & ~slot_px & (lum > 95) & (lum < 112)
    L.append({"shape": panel, "fill": measured_linear(rgb, k, x0, y0), "name": "recessed back panel (measured on its own pixels)"})

    def profile(axis, lo, hi, a0, a1, name, shape, clip):
        # a measured profile across a wall: one stop per art pixel column (axis "x") or row (axis "y"), mean over a0..a1
        stops = []
        for v in range(lo, hi):
            if axis == "x":
                sel = rgb[a0 - y0:a1 - y0, v - x0]
            else:
                sel = np.concatenate([rgb[v - y0, b0 - x0:b1 - x0] for b0, b1 in a0])
            stops.append([(v - lo + 0.5) / (hi - lo), [float(c) for c in np.median(sel, axis=0)]])
        a, b = ([lo, 0.0], [hi, 0.0]) if axis == "x" else ([0.0, lo], [0.0, hi])
        return {"shape": shape, "fill": {"linear": {"a": a, "b": b, "stops": stops}}, "clip": clip, "name": name}

    walls = G.rect(317.0, 519.5, 766.0, 606.0, 5.0)
    L.append(profile("y", 519, 531, [(326, 340), (744, 758)], None, "recess: its top wall stepping down to a dark line at y 524",
                     G.rect(317.0, 519.0, 761.5, 531.0), G.rect(317.0, 519.5, 761.5, 606.0, 5.0)))
    L.append(profile("x", 317, 328, 560, 600, "recess: its left wall in shadow (soft step, then a dark band)",
                     G.rect(317.0, 519.0, 328.0, 606.0), G.inter(walls, G.half((0.0, 524.0), (0.0, -1.0)))))
    L.append(profile("x", 759, 768, 560, 600, "recess: its right wall lit (a light lip, its top rounded where the top wall turns down)",
                     G.rect(759.0, 524.5, 766.5, 606.0, 3.0), walls))
    cm |= sdf.sd(walls, X, Y) < 2
    return L, cm, items


def elevation_content(rgb, mask, x0, y0, X, Y):
    L, cm = [], np.zeros(mask.shape, bool)
    items = []
    for el, guess in (("elevation-point", (312, 500)), ("elevation-square", (429, 500)), ("elevation-area", (546, 500)),
                      ("elevation-smooth", (689, 500))):
        cut = [c for c in hp.crops(el) if c[0] == "idle"][0]
        _, _, ox, oy, crgb, cmask = cut
        hh, ww = cmask.shape
        jj, ii = np.mgrid[0:hh, 0:ww]
        sel = cmask & (jj > 60)
        best = match(rgb, mask, x0, y0, crgb, sel, guess, reach=12)
        items += composed(el, "idle", (best[1], best[2]))
        cm[max(0, best[2] - y0):, max(0, best[1] - x0):best[1] - x0 + ww] = True
    return L, cm, items


def amenities_content(rgb, mask, x0, y0, X, Y):
    L, cm = [], np.zeros(mask.shape, bool)
    # fix round ("beware of which elements are rendered over the textures"): the recess's curved left side read again on a
    # 6x grid; it stays right of the building thumbnails (round 1's ran over the statue thumbnail at x 393..402)
    recess = G.poly((371.0, 497.6), (770.0, 497.6), (786.0, 503.0), (794.0, 512.0), (797.0, 530.0), (797.0, 577.0), (425.0, 577.0),
                    (424.5, 566.0), (423.5, 558.0), (421.0, 549.0), (417.0, 540.0), (412.7, 532.0), (406.0, 522.0), (399.0, 515.0),
                    (390.0, 508.5), (380.0, 503.0), smooth=1)
    k = mask & (sdf.sd(recess, X, Y) < -1.5) & (Y < 562)      # above the tab bubbles: the recess's own shading (dark at the top)
    L.append({"shape": recess, "fill": measured_linear(rgb, k, x0, y0), "name": "deep recess: curved left side, rounded right end"})
    cm |= sdf.sd(recess, X, Y) < 2
    ledge = G.rect(425.0, 577.0, 797.0, 579.6)
    k = mask & (sdf.sd(ledge, X, Y) < -0.4)
    L.append({"shape": ledge, "fill": measured_linear(rgb, k, x0, y0), "name": "light ledge under the recess"})
    cm |= sdf.sd(ledge, X, Y) < 1
    items = []
    for i, tid in enumerate(("amenities-bench-tab", "amenities-bridge-tab", "amenities-obelisk-tab", "amenities-flowers-tab",
                             "amenities-tree-tab")):
        cut = [c for c in hp.crops(tid) if c[0] == "idle"][0]
        _, _, ox, oy, crgb, cmask = cut
        hh, ww = cmask.shape
        jj, ii = np.mgrid[0:hh, 0:ww]
        disc = np.hypot(ii + 0.5 - 21.7, jj + 0.5 - 22.6) < 16.0
        best = match(rgb, mask, x0, y0, crgb, disc & cmask, (int(471 + 61 * i - 21.7), int(577 - 22.6)), reach=8)
        items += composed(tid, "idle", (best[1], best[2]))
    # the three thumbnails (path tile at the back, sign tile at the left, statue tile at the right) on their pillars: tiles read
    # on the 6x grid (sign 278.5..327.7, path 309.7..358.5, statue 340..389 across); round 1's region stopped at x 372 and cut
    # the statue tile. Kept art is drawn last, so the region keeps clear of the tabs and the bulldozer (x < 264)
    keep = G.poly((309.0, 528.0), (334.0, 510.5), (359.5, 525.0), (366.0, 538.0), (380.0, 547.0), (391.5, 561.0), (392.0, 600.0),
                  (276.5, 600.0), (276.5, 562.0), (297.0, 547.0), (304.0, 537.0))
    items.append({"keep_art": keep, "name": "the three building thumbnails on their pillars, kept as art (only them)"})
    return L, cm, items


SPECS = [
    ("terrain-palette", "tab-grass", "tab-shovel", terrain_content,
     "lavender tray (two round tab shoulders, a light top rail, a darker right end) with the yellow grass tab, the blue "
     "shovel tab and the bulldozer measured on it; a pale double oval on the back wall; a two-row checker floor, the top row "
     "starting at a diagonal edge"),
    ("building-palette", "tab-house", "tab-elevation", building_content,
     "the lavender tray (round 3: its left outline through the narrow neck between the two tab rings, the lower tab's outer "
     "light ring and the light rim down the left outline) with the yellow house tab, the blue up-down tab and the bulldozer; a "
     "recessed back panel with a shadowed left wall, a dark line along its top and a lit lip down its right side; the nine "
     "slot boxes composed from building-slot (tall top row, short bottom row) at their matched places, their faces running to "
     "the tray's bottom edge, each with its dark right-edge line"),
    ("elevation-palette", "tab-house", "tab-elevation", elevation_content,
     "the lavender tray with the blue house tab, the yellow up-down tab and the bulldozer; the four pedestals composed from "
     "their own scenes at their matched places (their pictures kept as art)"),
    ("amenities-palette", "tab-grass", "tab-shovel", amenities_content,
     "the lavender tray with the blue grass tab, the yellow shovel tab and the bulldozer; a deep recess with a curved left "
     "side and a rounded right end above a light ledge; the five amenity tabs composed from their own scenes; the three "
     "building thumbnails kept as art"),
]

if __name__ == "__main__":
    want = sys.argv[1:]
    for el, t1, t2, fn, how in SPECS:
        if not want or el in want:
            build(el, t1, t2, fn, how)
