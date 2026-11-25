"""hud-mode-base, round 4 (2026-11-25), built on round 3's version B (modebase3.version_b). FIX4_BRIEF.md.

His notes on B: "lines reaching the 3 buttons is not there" and "the background behind the 3 buttons should be a shade
that's not following the 3 big buttons". What the art shows and B did not (read on the cut's luminance, row by row):
  1. LINES THAT REACH. (a) The three cables in the basin start right at the rims of the info, pause and tools buttons
     (info: under its rim at (67..69, 567); pause: (98..101, 578..579); tools: (124..128, 593)); B started them 3..5 px
     lower and the small buttons' composed "tray pixels" disc laid a light halo over that gap, so no cable touched its
     button. (b) Each mode bubble's navy neck runs from under the bubble's rim down INTO the groove with a flared foot
     (course: x 37.3..44.6, the foot spreading to x 33 / 46 at y 519..520; buildings: x 102.6..108.3, the foot spreading
     left to x 96 at y 535; golfers: right edge slanting (157, 559) -> (153.3, 567)); B's buildings neck stopped 1.6 px
     above the groove and its golfers neck began 1.6 px below the bubble's rim.
  2. ONE PLATE. The fan's outline is ONE circle, centre (21.78, 680.22) r 208.1 (least squares on every boundary pixel
     that no bubble covers: the left edge, both gaps and the right end; residuals under 0.5 px); B drew it as three circles
     round the bubbles plus a polygon, so the plate's edge and its tones ringed each big bubble. Here the plate is that
     disc, lit by its own light: one linear gradient over the outer surface measured only on pixels at least 6 px from
     every bubble, the ridge's lit top as its own gradient band, the groove and lip as one radial profile, and a 1 px dark
     outline along the whole arc. The three bubbles' shadows are one separate soft layer under ONE light (offset, softness
     and strength fitted on the pixels below all three together, strength capped at 0.42 so the plate keeps its own
     shading); the necks' shadows the same way.
  3. The small buttons (info, pause, tools, zoom, rotate) are composed WITHOUT the tray disc their sprites carry (the
     halo), keeping their own measured cast shadow, so the plate runs continuously under and between them.
Sheet coordinates.
"""
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hp  # noqa: E402
import bubble as B  # noqa: E402
import geom as G  # noqa: E402
import els_trays as TR  # noqa: E402
import els_modebase as MB  # noqa: E402
import ped2  # noqa: E402
from geom import line, poly  # noqa: E402
from hp import sdf  # noqa: E402

OUT4 = os.path.join(hp.ROOT, "log", "roadmap", "P41", "work4", "hud_palettes")
GX, GY, GR = MB.GROOVE
R_PAINT = GR + 140.0
PLATE_C, PLATE_R = (21.78, 680.22), 208.6        # fitted r 208.12 to boundary pixel centres + 0.5 px to the pixel edge
MODE_IDS = ("hud-course-mode", "hud-buildings-mode", "hud-golfers-mode")
# necks: from under each bubble's rim down into the groove, with the art's flared foot (read row by row, see the docstring)
NECKS = [("under the course bubble",
          poly((37.3, 505.0), (44.6, 505.0), (44.6, 516.5), (45.3, 519.0), (46.4, 521.0), (46.4, 523.2),
               (34.4, 523.2), (34.4, 521.2), (35.8, 519.2), (37.3, 516.5), smooth=1)),
         ("under the buildings bubble",
          poly((102.6, 524.0), (108.3, 524.0), (108.3, 536.0), (109.0, 538.0), (109.4, 540.4),
               (97.5, 540.4), (97.5, 537.6), (99.4, 535.6), (101.4, 534.4), (102.6, 532.6), smooth=1)),
         ("under the golfers bubble",
          poly((151.6, 550.0), (157.6, 550.0), (157.2, 559.4), (156.4, 561.3), (155.0, 563.5), (153.8, 565.5),
               (153.3, 567.6), (153.4, 572.0), (145.5, 572.0), (145.5, 566.5), (148.4, 563.2), (150.2, 561.2),
               (151.6, 558.0)))]
# cables: the light core's centre line, row by row, each starting under its button's rim (the dark edge 2.2 px to its left,
# the lit falloff 1.8 px to its right)
CABLES = [("from the info button", [(69.4, 565.6), (68.6, 568.0), (67.5, 570.0), (66.5, 572.0), (65.8, 574.0), (65.0, 576.0),
                                    (64.0, 578.0), (63.0, 580.0), (62.6, 582.0), (62.4, 584.0), (62.0, 586.0), (61.5, 588.0),
                                    (61.0, 590.0), (60.5, 592.0), (60.1, 594.0), (60.0, 596.0), (60.0, 600.6)]),
          ("from the pause button", [(101.8, 576.6), (100.8, 578.4), (99.8, 580.0), (98.8, 582.0), (97.8, 584.0), (96.8, 586.0),
                                     (95.8, 588.0), (94.8, 590.0), (93.8, 592.0), (92.8, 594.0), (91.8, 596.0), (91.0, 598.0),
                                     (90.4, 600.6)]),
          ("from the tools button", [(127.2, 591.6), (126.2, 593.2), (125.4, 594.6), (124.9, 596.0), (124.2, 597.5), (123.7, 599.0),
                                     (123.5, 600.6)])]
SMALL_IDS = ("hud-zoom-in", "hud-zoom-out", "hud-rotate-right", "hud-rotate-left", "hud-info", "hud-pause", "hud-tools")


def ann(r0, r1):
    return G.sub(G.circle(GX, GY, r1), G.circle(GX, GY, r0))


def shifted(pts, dx):
    return [(x + dx, y) for x, y in pts]


def med(rgb, k):
    return [round(float(v), 1) for v in np.median(rgb[k], 0)] if k.sum() else None


def radial_stops(rgbf, sel, dr, edges):
    stops, last = [], None
    for e0, e1 in zip(edges[:-1], edges[1:]):
        k = sel & (dr >= e0) & (dr < e1)
        if k.sum() >= 3:
            last = med(rgbf, k)
        if last is not None:
            stops.append([round((GR + (e0 + e1) / 2) / R_PAINT, 5), last])
    return stops


def setup():
    el = "hud-mode-base"
    st, src, x0, y0, rgb, mask = hp.crops(el)[0]
    h, w = mask.shape
    X, Y = B.grid(x0, y0, w, h)
    rgbf = rgb.astype(float)
    lum = rgbf @ np.array([0.3, 0.59, 0.11])
    items, discs = [], np.zeros(mask.shape, bool)
    for pid, guess, reach in MB.PARTS:
        cut = hp.crops(pid)[0]
        _, _, ox, oy, crgb, cmask = cut
        best = TR.match(rgb, mask, x0, y0, crgb, cmask, guess, reach=reach)
        # no part keeps the tray disc its sprite carries: the plate is drawn continuously under all of them
        items += TR.composed(pid, cut[0], (best[1], best[2]), drop=("the tray pixels",))
        ch, cw = cmask.shape
        cx, cy = best[1] + cw / 2, best[2] + ch / 2
        if pid not in MODE_IDS:
            discs |= np.hypot(X - cx, Y - cy) < min(cw, ch) / 2 + 0.5
    for cx, cy, r in MB.RINGS:
        discs |= np.hypot(X - cx, Y - cy) < r - 2.5
    far = np.ones(mask.shape, bool)              # at least 6 px from every mode bubble: the plate's own light
    for cx, cy, r in MB.RINGS:
        far &= np.hypot(X - cx, Y - cy) > r + 6.0
    necks_u = G.U(*[s for _, s in NECKS])
    dneck = sdf.sd(necks_u, X, Y)
    cab_u = G.U(*[line(*p, w=7.0, cap="round") for _, p in CABLES])
    dcab = sdf.sd(cab_u, X, Y)
    dg = np.hypot(X - GX, Y - GY)
    ang = np.degrees(np.arctan2(Y - GY, X - GX))
    dplate = np.hypot(X - PLATE_C[0], Y - PLATE_C[1]) - PLATE_R
    plain = mask & ~discs & (dneck > 1.0)
    return dict(el=el, st=st, x0=x0, y0=y0, w=w, h=h, X=X, Y=Y, rgb=rgb, rgbf=rgbf, lum=lum, mask=mask, items=items,
                plain=plain, far=far, dneck=dneck, dcab=dcab, dr=dg - GR, ang=ang, dplate=dplate)


def build(S):
    x0, y0, w, h = S["x0"], S["y0"], S["w"], S["h"]
    X, Y, rgbf, lum, mask, dr, ang = S["X"], S["Y"], S["rgbf"], S["lum"], S["mask"], S["dr"], S["ang"]
    sil = G.inter(G.circle(PLATE_C[0], PLATE_C[1], PLATE_R), G.rect(x0, y0, x0 + w, y0 + h))
    clear = S["plain"] & (S["dcab"] > 0.0) & (S["dplate"] < -2.0)
    own = clear & S["far"] & (S["dneck"] > 8.0)          # the plate's own light: away from bubbles, necks and cables
    meas = {}
    # 1. basin (inside the lip): one gradient across it
    basin_k = clear & (dr < -12)
    basin = TR.measured_linear(S["rgb"], basin_k, x0, y0)
    # 2. lip + groove + underside: one radial profile (these do not change with the light), measured all along the arc
    lit = clear & (S["dneck"] > 8.0)
    edges = list(np.arange(-12.0, 7.5, 0.75))
    gstops = radial_stops(rgbf, lit, dr, edges)
    groove = {"radial": {"c": [GX, GY], "r": R_PAINT, "stops": gstops}}
    # 3. outer surface (beyond the ridge's top) and 4. the ridge's top band: each one gradient across the plate
    outer_k = own & (dr > 18)
    outer = TR.measured_linear(S["rgb"], outer_k, x0, y0)
    hi_k = lit & (dr > 7.5) & (dr < 11.5)
    hi = TR.measured_linear(S["rgb"], hi_k, x0, y0)
    edge_rgb = med(rgbf, mask & (S["dplate"] > -1.8) & S["far"] & (lum < 75)) or [32, 34, 66]
    L = [{"shape": sil, "fill": basin, "name": "the plate: ONE disc (centre (21.78, 680.22), r 208.6); the basin inside the "
                                              "groove, one gradient across it"},
         {"shape": G.inter(sil, ann(GR + 10.5, GR + 140.0)), "fill": outer, "soft": 3.0, "clip": sil,
          "name": "the plate's outer surface beyond the ridge: one gradient across the whole plate (light at the upper left, "
                  "dark at the right end), measured only on pixels 6 px or more from every bubble"},
         {"shape": G.inter(sil, ann(GR + 6.0, GR + 11.5)), "fill": hi, "soft": 1.2, "clip": sil,
          "name": "the ridge's lit top: a band along the groove, one gradient across the plate (its own light), falling "
                  "softly into the outer surface"},
         {"shape": G.inter(sil, ann(GR - 12.0, GR + 6.5)), "fill": groove, "soft": 1.0, "clip": sil,
          "name": "the light lip, the navy groove and the ridge's dark underside: one radial profile all along the arc"},
         {"shape": G.inter(G.outlined(G.circle(PLATE_C[0], PLATE_C[1], PLATE_R - 0.6), 1.2), G.rect(x0 + 0.6, y0, x0 + w - 0.4, y0 + h - 0.6)),
          "fill": edge_rgb, "name": "the plate's 1 px dark outline along its whole arc"}]
    base = [{"box": [x0, y0, x0 + w, y0 + h], "layers": list(L)}]
    clip = G.inter(sil, ann(GR + 2.5, GR + 140))
    # cast shadows, separate soft layers on the plate, one light for all three bubbles (one offset, softness and strength
    # fitted on the pixels just below the three bubbles together; strength capped so the plate keeps its own shading)
    discs = [G.circle(cx, cy, r - 3.0) for cx, cy, r in MB.RINGS]
    near = np.zeros(mask.shape, bool)
    for (cx, cy, r), d in zip(MB.RINGS, discs):
        sd = sdf.sd(d, X, Y)
        near |= clear & (sd > 0.5) & (sd < 10.0) & (Y > cy) & (dr > 2.5)
    fn = ped2.shadow_layer(G.U(*discs), "the three bubbles' soft cast shadows on the plate (one light, a separate layer)", clip=clip)
    p, _ = ped2.fit_cast(rgbf, near, x0, y0, base, fn, init=(-2.0, 3.0, 3.5, 0.3), steps=(1.0, 1.0, 1.0, 0.08),
                         lo=(-5, 0, 3.0, 0), hi=(1, 5, 6, 0.42))
    L.append(fn(p))
    base[0]["layers"].append(fn(p))
    meas["bubble shadows (dx dy soft alpha)"] = [round(float(v), 2) for v in p]
    nk = U_necks = G.U(*[sh for _, sh in NECKS])
    near = clear & (S["dneck"] > 0.4) & (S["dneck"] < 9.0) & (dr > 2.5)
    fn = ped2.shadow_layer(nk, "the three necks' soft shadows on the ridge to their left (one light, a separate layer)", clip=clip)
    p, _ = ped2.fit_cast(rgbf, near, x0, y0, base, fn, init=(-3.0, 0.0, 2.5, 0.45), steps=(1.0, 1.0, 1.0, 0.15),
                         lo=(-7, -2, 1.0, 0), hi=(1, 3, 6, 0.8))
    L.append(fn(p))
    meas["neck shadows (dx dy soft alpha)"] = [round(float(v), 2) for v in p]
    # necks (navy, into the groove), their faint lit right edge
    for nm, s in NECKS:
        k = mask & (sdf.sd(s, X, Y) < -0.6) & (lum < 60)
        L.append({"shape": G.inter(s, sil), "fill": med(rgbf, k) or [36, 38, 68], "soft": 0.4,
                  "name": f"a navy neck {nm}: from under the bubble's rim down into the groove, its foot flaring into it"})
    for nm, s in NECKS:
        pts = s["rings"][0]
        mx = np.mean([q[0] for q in pts])
        right = sorted([p for p in pts if p[0] >= mx and p[1] < max(q[1] for q in pts) - 1.0], key=lambda p: p[1])
        L.append({"shape": G.inter(line(*[(x - 0.6, y) for x, y in right], w=0.9), sil), "fill": [78, 80, 118], "soft": 0.3,
                  "name": f"the neck {nm}: a faint lit right edge"})
    # cables: raised wires (dark left edge, light core, lit falloff), each from its button's rim to the plate's bottom
    plain = S["plain"]
    for nm, pts in CABLES:
        edge = shifted(pts, -2.2)
        fall = shifted(pts, 1.8)
        ke = plain & (sdf.sd(line(*edge, w=1.4), X, Y) < 0) & (lum < 102)
        kc = plain & (sdf.sd(line(*pts, w=1.6), X, Y) < 0) & (lum > 118)
        kf = plain & (sdf.sd(line(*fall, w=2.0), X, Y) < 0) & (lum > 111) & (lum < 128)
        L.append({"shape": G.inter(line(*fall, w=2.6, cap="round"), sil), "fill": med(rgbf, kf) or [118, 118, 150],
                  "soft": 0.8, "name": f"the cable {nm}: its lit falloff on the right, into the basin"})
        L.append({"shape": G.inter(line(*edge, w=1.5, cap="round"), sil), "fill": med(rgbf, ke) or [86, 86, 120],
                  "soft": 0.4, "name": f"the cable {nm}: its dark left edge, one smooth path from the button's rim"})
        L.append({"shape": G.inter(line(*pts, w=1.8, cap="round"), sil), "fill": med(rgbf, kc) or [128, 128, 160],
                  "soft": 0.4, "name": f"the cable {nm}: its light core, one smooth path from the button's rim"})
    fan = {"box": [x0 - 2, y0 - 2, x0 + w + 2, y0 + h + 2], "layers": L}
    return {S["st"]: [fan] + S["items"]}, meas


HOW = ("round 4, built on version B: the plate is ONE lavender disc (its outline one circle, centre (21.78, 680.22) r 208.6, "
       "fitted to every boundary pixel no bubble covers) lit by its own light: the basin, the lip, the navy groove and the "
       "ridge's dark underside as one radial profile, the ridge's lit top and the outer surface each one gradient across the "
       "plate measured away from the bubbles, a 1 px dark outline along the whole arc; the three bubbles' soft cast "
       "shadows as one separate layer under one light (offset, softness and strength fitted together, strength capped at "
       "0.42), the necks' shadows likewise; navy necks from under each bubble's rim down into the groove with flared feet; three "
       "raised cables, each starting under the rim of its button (info, pause, tools); the bubbles and small buttons composed "
       "on top without the tray discs their sprites carry")
CHANGED = ("notes 'lines reaching the 3 buttons is not there' and 'the background behind the 3 buttons should be a shade that's "
           "not following the 3 big buttons': the three cables now start under the rims of the info, pause and tools buttons "
           "(B began them 3..5 px lower, under a light halo disc each small button carried; the halos are gone); the three "
           "navy necks now run from under each mode bubble's rim into the groove with the art's flared feet (B's buildings "
           "neck stopped 1.6 px short of the groove, its golfers neck 1.6 px under the rim); the plate's outline is one "
           "circle instead of three circles round the bubbles, and its surface is one plate lit by its own light (gradients "
           "measured only away from the bubbles), with the bubbles' shadows a separate soft layer cast by one light")


def save(S, scenes, extra):
    """The scene json in OUT4 (hp.save_scene writes to round 3's folder)."""
    import json
    st = S["st"]
    doc = {"id": S["el"], "coords": "source sheet pixels",
           "states": {st: {"sheet": hp.crops(S["el"])[0][1], "cut": [S["x0"], S["y0"], S["w"], S["h"]], "scene": scenes[st]}}}
    doc.update(extra)
    with open(os.path.join(OUT4, "hud-mode-base.json"), "w") as f:
        json.dump(doc, f, default=lambda o: float(o) if isinstance(o, np.floating) else int(o))


def main():
    os.makedirs(OUT4, exist_ok=True)
    S = setup()
    scenes, meas = build(S)
    sc = hp.score(S["el"], scenes, z=3)
    print(f"  hud-mode-base round 4: mad {sc[S['st']][0]:.2f} edge {sc[S['st']][1]}")
    print(meas)
    hp.compare(S["el"], scenes, z=3, out=os.path.join(OUT4, "hud-mode-base.png"))
    save(S, scenes, {"how": HOW, "changed": CHANGED, "measured": meas, "round": 4})
    with open(os.path.join(OUT4, "results.tsv"), "w", encoding="utf-8") as f:
        f.write("id\tstatus\tchanged\tunsure\tinterior_mad\tedge_px\thow\n")
        f.write(f"hud-mode-base\tdone\t{CHANGED}\t{UNSURE}\t{sc[S['st']][0]:.2f}\t{sc[S['st']][1]}\t{HOW}\n")


UNSURE = ("'the 3 buttons' in the first note read two ways (the cables to the three small yellow buttons, or the necks to the "
          "three mode bubbles): both are fixed")

if __name__ == "__main__":
    main()
