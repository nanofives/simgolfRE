"""hud-mode-base, round 3 (2026-11-24): rejected twice with no note, so two clearly different versions (FIX3_BRIEF.md).

Five largest differences between round 2 and the art (read at 4x / 6x side by side, and as numbers):
  1. the RIDGE: above the groove the art has a raised tube lit from above (radial profile from the groove's centre: navy
     groove at dr 0..2, a dark underside at dr 4 (41..99), a bright top at dr 8..12 (170..180 between the necks), then a
     falloff outward); round 2 had a flat surface with a lit rim along the fan's outline instead;
  2. the NECKS: each mode bubble stands on a navy neck that crosses the whole ridge (course x 38.4..44.6, buildings
     102.2..109.4 tapering to 106.4, golfers a wedge under the rim at 150..158), and the ridge is SHADED to the left of each
     neck (lum 65..113 against 155..170 to its right); round 2 drew thin poles 3.6 px wide;
  3. the CABLES in the basin are raised wires lit from the right (a 1.5 px dark left edge 81..89, a light core 124..132, a
     2..3 px falloff 117..125 into the basin's 109); round 2 drew them as dark lines, 2..3 px left of where they are;
  4. a light LIP (117..132) runs just inside the groove all along; round 2 had it on the groove's outer side;
  5. the outer surface is LIGHT at the upper left (187..195 left of the course bubble) and falls to 55..73 toward the right
     end, where a 1 px dark line closes the outline; round 2 lit the whole outline edge evenly.
Version A (faithful): the surface is cut into 2-degree wedges round the groove's centre; each wedge is painted with the
radial profile measured on its own pixels (bins of 0.75 px across the lip, groove and underside, 1.5 px over the ridge,
3 px beyond; the outer bins smoothed along the arc), so every
light, mid and dark patch along the arc (the necks' shadows, the bright top-left, the dark right end) is its own band.
Version B (clean): one continuous tube with one measured profile, the outer surface as one left-to-right gradient, clean
navy necks with a lit right edge, each neck's and each bubble's cast shadow fitted as its own soft layer, raised cables.
Both: the necks, cables, the 1 px dark outline at the right end, and the bubbles and small buttons composed on top (the
mode bubbles without the tray pixels their cuts carry, the small buttons with their sockets). Sheet coordinates.
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

GX, GY, GR = MB.GROOVE
R_PAINT = GR + 140.0
MODE_IDS = ("hud-course-mode", "hud-buildings-mode", "hud-golfers-mode")
# necks (read row by row on the cut; their tops are under the bubbles' rims)
NECKS = [("under the course bubble", poly((38.4, 505.0), (44.6, 505.0), (44.6, 521.8), (38.4, 521.8)), -84.0),
         ("under the buildings bubble", poly((102.2, 524.0), (109.4, 524.0), (108.8, 529.0), (107.6, 533.0), (106.4, 537.0),
                                              (102.2, 537.0)), -65.7),
         ("under the golfers bubble", poly((150.5, 556.0), (157.6, 557.5), (158.0, 560.5), (156.3, 562.5), (155.0, 565.0),
                                            (154.0, 569.0), (149.5, 569.0), (150.0, 562.0)), -52.0)]
# cables: the light core's centre line, row by row (the dark edge lies 2.2 px to its left, the falloff 1.8 px to its right)
CABLES = [("from the info button", [(66.5, 571.0), (65.8, 574.0), (65.0, 576.0), (64.0, 578.0), (63.0, 580.0), (62.6, 582.0),
                                    (62.4, 584.0), (62.0, 586.0), (61.5, 588.0), (61.0, 590.0), (60.5, 592.0), (60.1, 594.0),
                                    (60.0, 596.0), (60.0, 600.6)]),
          ("from the pause button", [(99.5, 579.5), (98.2, 582.0), (97.2, 584.0), (96.2, 586.0), (95.2, 588.0), (94.2, 590.0),
                                     (93.2, 592.0), (92.2, 594.0), (91.2, 596.0), (90.6, 598.0), (90.2, 600.6)]),
          ("from the tools button", [(127.5, 594.6), (126.0, 596.0), (124.8, 597.8), (124.0, 600.6)])]


def ann(r0, r1):
    return G.sub(G.circle(GX, GY, r1), G.circle(GX, GY, r0))


def wedge(a0, a1):
    d0 = (np.cos(np.radians(a0)), np.sin(np.radians(a0)))
    d1 = (np.cos(np.radians(a1)), np.sin(np.radians(a1)))
    return G.inter(G.half((GX, GY), (d0[1], -d0[0])), G.half((GX, GY), (-d1[1], d1[0])))


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
    items, discs, wide = [], np.zeros(mask.shape, bool), np.zeros(mask.shape, bool)
    for pid, guess, reach in MB.PARTS:
        cut = hp.crops(pid)[0]
        _, _, ox, oy, crgb, cmask = cut
        best = TR.match(rgb, mask, x0, y0, crgb, cmask, guess, reach=reach)
        drop = ("the tray pixels",) if pid in MODE_IDS else ()
        items += TR.composed(pid, cut[0], (best[1], best[2]), drop=drop)
        ch, cw = cmask.shape
        cx, cy = best[1] + cw / 2, best[2] + ch / 2
        # what the composed part paints over (kept out of the fan's measurements): the mode bubbles' discs, the small
        # buttons' discs with their sockets
        # (a mode bubble's cut is wider than the bubble, so its disc comes from MB.RINGS below instead)
        if pid not in MODE_IDS:
            discs |= np.hypot(X - cx, Y - cy) < min(cw, ch) / 2 + 0.5
        else:
            # version A measures without the whole cut disc (r = half the cut less 1): the bubbles' dark outlines and
            # collars stay out of its wedge medians (they streaked the outer surface otherwise)
            wide |= np.hypot(X - cx, Y - cy) < min(cw, ch) / 2 - 1.0
    for cx, cy, r in MB.RINGS:
        discs |= np.hypot(X - cx, Y - cy) < r - 2.5
    necks_u = G.U(*[s for _, s, _ in NECKS])
    dneck = sdf.sd(necks_u, X, Y)
    cab_u = G.U(*[line(*shifted(p, 0.0), w=7.0, cap="round") for _, p in CABLES])
    dcab = sdf.sd(cab_u, X, Y)
    dg = np.hypot(X - GX, Y - GY)
    ang = np.degrees(np.arctan2(Y - GY, X - GX))
    plain = mask & ~discs & (dneck > 1.0)
    plain_a = plain & ~wide
    return dict(plain_a=plain_a, el=el, st=st, x0=x0, y0=y0, w=w, h=h, X=X, Y=Y, rgb=rgb, rgbf=rgbf, lum=lum, mask=mask, items=items,
                plain=plain, dneck=dneck, dcab=dcab, dr=dg - GR, ang=ang)


def common_layers(S, sil):
    """Necks, cables and the right end's outline line (both versions)."""
    rgb, X, Y, mask, plain, lum = S["rgb"], S["X"], S["Y"], S["mask"], S["plain"], S["lum"]
    L = []
    for nm, s, _ in NECKS:
        k = mask & (sdf.sd(s, X, Y) < -0.6) & (lum < 60)
        L.append({"shape": G.inter(s, sil), "fill": med(S["rgbf"], k) or [36, 38, 68],
                  "name": f"a navy neck {nm}, from the bubble's rim down across the ridge into the groove"})
    for nm, pts in CABLES:
        edge = shifted(pts, -2.2)
        fall = shifted(pts, 1.8)
        ke = plain & (sdf.sd(line(*edge, w=1.4), X, Y) < 0) & (lum < 102)
        kc = plain & (sdf.sd(line(*pts, w=1.6), X, Y) < 0) & (lum > 118)
        kf = plain & (sdf.sd(line(*fall, w=2.0), X, Y) < 0) & (lum > 111) & (lum < 128)
        L.append({"shape": G.inter(line(*fall, w=2.6, cap="round"), sil), "fill": med(S["rgbf"], kf) or [118, 118, 150],
                  "soft": 0.8, "name": f"the cable {nm}: its lit falloff on the right, into the basin"})
        L.append({"shape": G.inter(line(*edge, w=1.5, cap="round"), sil), "fill": med(S["rgbf"], ke) or [86, 86, 120],
                  "soft": 0.4, "name": f"the cable {nm}: its dark left edge (the wire's shadow side), one smooth path"})
        L.append({"shape": G.inter(line(*pts, w=1.8, cap="round"), sil), "fill": med(S["rgbf"], kc) or [128, 128, 160],
                  "soft": 0.4, "name": f"the cable {nm}: its light core, one smooth path"})
    # the 1 px dark line closing the outline at the right end (x 186..213: 16..40 at the edge)
    ol = G.inter(G.outlined(sil, 1.2), G.rect(150.0, 545.0, 212.4, 598.8))   # not along the cut's own bottom and right edges
    k = mask & (sdf.sd(G.outlined(sil, 2.4), X, Y) < 0) & (X > 150) & (Y > 545) & (lum < 60)
    if k.sum() > 3:
        L.append({"shape": ol, "fill": med(S["rgbf"], k), "name": "the thin dark line closing the outline at the right end"})
    return L


def finish(S, scenes_name, fan_layers, how):
    x0, y0, w, h = S["x0"], S["y0"], S["w"], S["h"]
    fan = {"box": [x0 - 2, y0 - 2, x0 + w + 2, y0 + h + 2], "layers": fan_layers}
    scenes = {S["st"]: [fan] + S["items"]}
    sc = hp.score(S["el"], scenes, z=3)
    print(f"  {scenes_name}: mad {sc[S['st']][0]:.2f} edge {sc[S['st']][1]}")
    hp.compare(S["el"], scenes, z=3, out=os.path.join(hp.OUT, scenes_name + ".png"))
    hp.save_scene(S["el"], scenes, {"how": how, "version": scenes_name[-1]}, name=scenes_name)
    return sc[S["st"]]


def version_a(S):
    sil = G.inter(MB.SIL, G.rect(S["x0"], S["y0"], S["x0"] + S["w"], S["y0"] + S["h"]))
    plain, dr, ang, rgbf = S["plain_a"], S["dr"], S["ang"], S["rgbf"]
    clear = plain & (S["dcab"] > 0.0)
    L = [{"shape": sil, "fill": med(rgbf, clear & (dr < -12)) or [109, 109, 150], "name": "the basin inside the groove (flat, measured)"}]
    # radial bins: 0.75 px across the lip, groove and underside, 1.5 px over the ridge, 3 px over the outer surface
    edges = np.array(list(np.arange(-12.0, 6.0, 0.75)) + list(np.arange(6.0, 26.0, 1.5)) + list(np.arange(26.0, 134.0, 3.0)))
    mids = (edges[:-1] + edges[1:]) / 2
    step = 2.0
    angs = np.arange(-100.0, -25.0, step)
    M = np.full((len(angs), len(mids), 3), np.nan)
    for i, a in enumerate(angs):
        sel = clear & (ang >= a) & (ang < a + step)
        for j, (e0, e1) in enumerate(zip(edges[:-1], edges[1:])):
            k = sel & (dr >= e0) & (dr < e1)
            if k.sum() >= 3:
                M[i, j] = np.median(rgbf[k], 0)
    have = ~np.isnan(M[..., 0])
    # fill the gaps (pixels under a bubble, outside the outline) from the nearest measured bin of the same wedge, then from
    # the nearest wedge; the paint there is hidden anyway, it only keeps the radial interpolation from jumping
    for i in range(len(angs)):
        idx = np.nonzero(have[i])[0]
        if len(idx):
            for j in range(len(mids)):
                if not have[i, j]:
                    M[i, j] = M[i, idx[np.argmin(np.abs(idx - j))]]
    rows = np.nonzero(have.any(1))[0]
    for i in range(len(angs)):
        if not have[i].any() and len(rows):
            M[i] = M[rows[np.argmin(np.abs(rows - i))]]
    # smooth along the arc where the art changes slowly: the outer surface (dr > 14) three passes of [1, 2, 1], the ridge
    # top (dr 6..14) one pass, the lip / groove / underside not at all (the necks' shadows stay where they are)
    for passes, lo_, hi_ in ((3, 14.0, 999.0), (1, 6.0, 14.0)):
        cols = (mids > lo_) & (mids <= hi_)
        for _ in range(passes):
            P = M[:, cols].copy()
            Q = P.copy()
            Q[1:-1] = (P[:-2] + 2 * P[1:-1] + P[2:]) / 4
            M[:, cols] = Q
    for i, a in enumerate(angs):
        if not have[i].any():
            continue
        stops = [[round((GR + m) / R_PAINT, 5), [round(float(v), 1) for v in M[i, j]]] for j, m in enumerate(mids)]
        shp = G.inter(sil, ann(GR - 12.0, GR + 134.0), wedge(a - 0.5, a + step + 0.5))
        L.append({"shape": shp, "fill": {"radial": {"c": [GX, GY], "r": R_PAINT, "stops": stops}}, "soft": 1.0,
                  "name": f"surface wedge {a:.0f}..{a + step:.0f} deg round the groove's centre: the lip, groove, ridge and "
                          f"outer surface as measured on its own pixels"})
    L += common_layers(S, sil)
    return finish(S, "hud-mode-base--a", L, HOW_A)


def version_b(S):
    sil = G.inter(MB.SIL, G.rect(S["x0"], S["y0"], S["x0"] + S["w"], S["y0"] + S["h"]))
    plain, dr, ang, rgbf, X, Y, x0, y0 = S["plain"], S["dr"], S["ang"], S["rgbf"], S["X"], S["Y"], S["x0"], S["y0"]
    clear = plain & (S["dcab"] > 0.0)
    # the tube's one profile: measured between the necks, away from their shadows (angles -80..-70 and -63..-56)
    lit = clear & (((ang > -80) & (ang < -70)) | ((ang > -63) & (ang < -56)))
    edges = list(np.arange(-12.0, 6.0, 0.75)) + list(np.arange(6.0, 36.0, 1.5))
    tstops = radial_stops(rgbf, lit, dr, edges)
    tube = {"radial": {"c": [GX, GY], "r": R_PAINT, "stops": tstops}}
    top = max(tstops, key=lambda t: sum(t[1]))[1]
    # the outer surface: one gradient from the light upper left to the dark right end, measured beyond the tube (dr > 22)
    outer_k = clear & (dr > 22)
    outer = TR.measured_linear(S["rgb"], outer_k, x0, y0)
    L = [{"shape": sil, "fill": med(rgbf, clear & (dr < -12)) or [109, 109, 150], "name": "the basin inside the groove (flat, measured)"},
         {"shape": G.inter(sil, ann(GR + 18.0, GR + 140.0)), "fill": outer, "name": "the outer surface: one gradient, light at the "
                                                                                    "upper left, dark toward the right end"},
         {"shape": G.inter(sil, ann(GR - 12.0, GR + 34.0)), "fill": tube, "soft": 6.0,
          "name": "the ridge: one continuous tube round the groove, lit from above (one measured profile: lip, groove, dark "
                  "underside, bright top, falloff; its outer edge fades over 6 px into the outer surface)"},
         {"shape": G.inter(sil, ann(GR + 8.6, GR + 11.4)), "fill": top, "soft": 1.0,
          "name": "the ridge's highlight: a crisp band along its top in the tube's brightest measured tone"},
         {"shape": G.inter(sil, ann(GR + 2.6, GR + 4.2)), "fill": med(rgbf, lit & (dr > 2.8) & (dr < 4.2)) or [60, 62, 96],
          "soft": 0.5, "name": "the ridge's underside: a dark line where it turns down into the groove"}]
    base = [{"box": [x0, y0, x0 + S["w"], y0 + S["h"]], "layers": list(L)}]
    meas = {}
    # each bubble's soft cast shadow on the surface (the bubble's disc moved down and to the left), then each neck's
    for (cx, cy, r), nm in zip(MB.RINGS, ("course", "buildings", "golfers")):
        disc = G.circle(cx, cy, r - 3.0)
        near = clear & (sdf.sd(disc, X, Y) > 0.5) & (sdf.sd(disc, X, Y) < 9.0) & (Y > cy) & (dr > 2.5)
        fn = ped2.shadow_layer(disc, f"the {nm} bubble's soft cast shadow on the ridge", clip=G.inter(sil, ann(GR + 2.5, GR + 140)))
        if near.sum() < 20:
            print(f"  {nm} bubble: no receiving pixels, shadow skipped")
            continue
        p, _ = ped2.fit_cast(rgbf, near, x0, y0, base, fn, init=(-2.0, 2.0, 3.0, 0.35), steps=(1.5, 1.5, 1.5, 0.15),
                             lo=(-6, -1, 0.8, 0), hi=(2, 6, 7, 0.85))
        L.append(fn(p))
        base[0]["layers"].append(fn(p))
        meas[f"{nm} bubble shadow"] = [round(float(v), 2) for v in p]
    for nm, s, _ in NECKS:
        near = clear & (S["dneck"] > 0.4) & (S["dneck"] < 9.0) & (dr > 2.5)
        fn = ped2.shadow_layer(s, f"the neck {nm}: its soft shadow on the ridge to its left", clip=G.inter(sil, ann(GR + 2.5, GR + 140)))
        p, _ = ped2.fit_cast(rgbf, near, x0, y0, base, fn, init=(-3.0, 0.0, 2.5, 0.45), steps=(1.0, 1.0, 1.0, 0.15),
                             lo=(-7, -2, 0.5, 0), hi=(1, 3, 6, 0.9))
        L.append(fn(p))
        base[0]["layers"].append(fn(p))
        meas[f"neck {nm} shadow"] = [round(float(v), 2) for v in p]
    L += common_layers(S, sil)
    # the necks' lit right edges (a 1 px light line where the ridge's light catches them)
    for nm, s, _ in NECKS:
        pts = s["rings"][0]
        right = [p for p in pts if p[0] >= np.mean([q[0] for q in pts])]
        right.sort(key=lambda p: p[1])
        L.append({"shape": G.inter(line(*[(x - 0.6, y) for x, y in right], w=0.9), sil), "fill": [78, 80, 118], "soft": 0.3,
                  "name": f"the neck {nm}: a faint lit right edge"})
    return finish(S, "hud-mode-base--b", L, HOW_B), meas


HOW_A = ("version A, faithful: the lavender fan (authored outline: three circles round the mode bubbles and a body) cut into "
         "2-degree wedges round the groove's centre, each painted with the radial profile measured on its own pixels (the "
         "light lip inside the groove, the navy groove, the ridge's dark underside, its bright top and the outer surface: every "
         "light and dark patch along the arc, the necks' shadows included); navy necks under the three mode bubbles; three "
         "raised cables (dark left edge, light core, lit falloff); the dark line closing the right end; the bubbles and small "
         "buttons composed on top")
HOW_B = ("version B, clean: the lavender fan as one continuous ridge (a tube lit from above with one measured profile: light lip, "
         "navy groove, dark underside, bright top, falloff) over an outer surface that runs from light at the upper left to "
         "dark at the right end; clean navy necks with a faint lit right edge; each bubble's and each neck's soft cast shadow on "
         "the ridge fitted as its own layer; three raised cables; the dark line closing the right end; the bubbles and small "
         "buttons composed on top")

if __name__ == "__main__":
    S = setup()
    want = sys.argv[1:] or ["a", "b"]
    if "a" in want:
        version_a(S)
    if "b" in want:
        print(version_b(S)[1])
