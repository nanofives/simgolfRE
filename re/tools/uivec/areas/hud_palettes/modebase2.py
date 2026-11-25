"""hud-mode-base, fix round (2026-11-24, verdict "redo"): the lavender fan in the bottom left corner, read again.

Round 1 filled the cut's staircase extent with two gradients, drew the groove, and pasted the bubbles WITH the ragged "ground"
patches their own cuts carry (light jagged rings round the mode bubbles). Its own handoff listed what was missing: the stems
under the mode bubbles and the thin lines under the small buttons. Read on a 6x gridded view, coverage digits and a
line-detector map of the cut (sheet coordinates, 2026-11-24):
  outline   the union of three circles round the mode bubbles and a body (round 1's fit, kept: it is the fan's real edge),
            now drawn as that authored outline, not as the cut's staircase;
  surface   above the groove a raised lavender surface shaded by its distance from the groove (a radial paint measured on it:
            the light lip, the mid surface, the lighter ledge further out), with a thin rim lit along its top edges and dark
            along its right edge (a 2.6 px measured bevel; a deep bevel on this outline made ridges at its corners); inside the
            groove a darker gradient;
  groove    a 3.6 px navy arc round (20, 720), with a light lip along its outer side (round 1's fit, kept);
  stems     a navy STEM under the course bubble (x 41.4..45, from the bubble's foot at y 508 down into the groove) and one under
            the buildings bubble (3.8 px wide, x 104..108, y 519..534), each with its soft shadow on the surface to its left;
  cables    thin darker lines (1.5 px) running from the small buttons down out of the fan: under info ((63, 573) to (57.5, 600)),
            under pause ((97.5, 576.5) to (88.5, 600)), under tools ((126.5, 592) to (124, 600)) and from zoom in down to rotate
            right ((21.5, 545) to (7.5, 559)); each one smooth path with round ends;
  buttons   the three mode bubbles and the seven small buttons composed from their own scenes (this round's where they were
            redone) at the places where their cuts match the fan; the mode bubbles WITHOUT the ground their cuts carry (it is
            the fan, drawn here), the small buttons with their dark sockets (those are real: the halo round each button).
Layer order as the game draws it: fan, groove, stems and their shadows, cables, then the buttons on top.
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
from geom import rect, line  # noqa: E402
from hp import sdf  # noqa: E402

FAN_STOPS = [0.0, 1.0, 2.0, 3.5, 5.5, 8.0, 12.0, 18.0]
STEMS = [("under the course bubble", rect(41.4, 507.0, 45.0, 523.5)),
         ("under the buildings bubble", line((105.9, 518.5), (106.0, 535.0), w=3.8))]
CABLES = [("under the info button", [(63.0, 573.0), (61.5, 579.0), (60.0, 584.0), (58.8, 589.0), (58.2, 594.0), (57.5, 600.5)]),
          ("under the pause button", [(97.5, 576.5), (95.5, 582.0), (94.0, 586.0), (91.5, 590.0), (89.5, 595.0), (88.5, 600.5)]),
          ("under the tools button", [(126.6, 591.5), (125.2, 594.5), (124.5, 597.0), (124.0, 600.5)]),
          ("from zoom in down to rotate right", [(21.5, 545.0), (17.5, 547.5), (15.0, 551.0), (12.5, 554.5), (9.0, 557.5), (7.5, 559.0)])]
MODE_IDS = ("hud-course-mode", "hud-buildings-mode", "hud-golfers-mode")


def main():
    el = "hud-mode-base"
    st, src, x0, y0, rgb, mask = hp.crops(el)[0]
    h, w = mask.shape
    X, Y = B.grid(x0, y0, w, h)
    rgbf = rgb.astype(float)
    items, covered = [], np.zeros(mask.shape, bool)
    for pid, guess, reach in MB.PARTS:
        cut = hp.crops(pid)[0]
        _, _, ox, oy, crgb, cmask = cut
        best = TR.match(rgb, mask, x0, y0, crgb, cmask, guess, reach=reach)
        print(f"  {pid}: at {best[1]}, {best[2]} (mean error {best[0]:.1f})")
        drop = ("the tray pixels", "the light outer ring") if pid in MODE_IDS else ()
        items += TR.composed(pid, cut[0], (best[1], best[2]), drop=drop)
        jj, ii = best[2] - y0, best[1] - x0
        covered[max(jj, 0):jj + cmask.shape[0], max(ii, 0):ii + cmask.shape[1]] |= \
            cmask[max(-jj, 0):, max(-ii, 0):][:h - max(jj, 0), :w - max(ii, 0)]
    box = G.moved(rect(0, 0, w, h), x0, y0)
    sil = G.inter(MB.SIL, box)
    gx, gy, gr = MB.GROOVE
    dg = np.hypot(X - gx, Y - gy)
    plain = mask & ~covered
    stems_u = G.U(*[s for _, s in STEMS])
    cables_u = G.U(*[line(*p, w=2.5) for _, p in CABLES])
    clear = plain & (sdf.sd(stems_u, X, Y) > 4.0) & (sdf.sd(cables_u, X, Y) > 0.5)
    upper_px = clear & (dg > gr + 4) & (sdf.sd(sil, X, Y) < -0.05)
    # the surface's shading follows the distance from the groove (light lip, mid surface, lighter ledge further out): a
    # radial paint round the groove's centre, its stops the medians of the plain pixels binned by that distance
    knots = [gr + 2.0, gr + 6.0, gr + 12.0, gr + 20.0, gr + 32.0, gr + 48.0, gr + 70.0, gr + 100.0]
    stops, last = [], None
    for k0, k1 in zip([gr] + knots[:-1], knots):
        sel = upper_px & (dg >= k0) & (dg < k1) & (sdf.sd(sil, X, Y) < -2.5)
        if sel.sum() >= 5:
            last = [round(float(v), 1) for v in np.median(rgbf[sel], 0)]
        if last is not None:
            stops.append([round(((k0 + k1) / 2) / (gr + 100.0), 4), last])
    fp = {"radial": {"c": [gx, gy], "r": gr + 100.0, "stops": stops}}
    # the rim along the outline: a thin band lit from the upper left, dark along the right edge (a bevel only 3 px deep)
    rim = G.sub(sil, G.grow(sil, -2.6))
    rim_px = upper_px & (sdf.sd(sil, X, Y) > -2.6)
    rp = B.measure_bevel_shape(rgb, rim_px, x0, y0, sil, [0.0, 0.8, 1.6, 2.6])
    rp["bevel"]["of"] = sil
    inner = G.inter(sil, G.circle(gx, gy, gr))
    groove = G.inter(G.outlined(G.circle(gx, gy, gr), 3.6), sil)
    lip = G.inter(G.outlined(G.circle(gx, gy, gr + 3.0), 1.8), sil)

    def mode(k):
        return TR.mode_rgb(rgb[k]) if k.any() else [60, 60, 100]

    L = [{"shape": sil, "fill": fp, "name": "the fan: its authored outline (three circles round the mode bubbles and a body), "
                                           "its surface shaded by the distance from the groove (measured radial paint)"},
         {"shape": rim, "fill": rp, "soft": 0.5, "name": "the fan's rim: lit along its top edges, dark along its right edge "
                                                         "(measured, 2.6 px deep)"},
         {"shape": inner, "fill": TR.measured_linear(rgb, clear & (dg < gr - 3), x0, y0),
          "name": "the fan's lower part, inside the groove (measured gradient)"},
         {"shape": lip, "fill": mode(plain & (np.abs(dg - gr - 2.4) < 0.6)), "name": "a light lip along the groove's outer side"},
         {"shape": groove, "fill": mode(plain & (np.abs(dg - gr) < 0.8)), "name": "the groove: a 3.6 px navy arc round (20, 720)"}]
    base = [{"box": [x0, y0, x0 + w, y0 + h], "layers": list(L)}]
    meas = {}
    for nm, s in STEMS:
        ks = mask & (sdf.sd(s, X, Y) < -0.8)
        col = TR.mode_rgb(rgb[ks]) or [40, 40, 80]
        # its shadow on the surface to its left: the stem's shape moved, soft, strength fitted on the pixels beside it
        near = plain & (sdf.sd(s, X, Y) > 0.4) & (sdf.sd(s, X, Y) < 6.0) & (X < sdf_cx(s)) & (dg > gr + 2.5)
        fn = ped2.shadow_layer(s, f"the stem {nm}: its soft shadow on the surface to its left", clip=sil)
        p, _ = ped2.fit_cast(rgbf, near, x0, y0, base, fn, init=(-2.0, 0.0, 2.0, 0.4), steps=(1.0, 1.0, 1.0, 0.15),
                             lo=(-5, -2, 0.5, 0), hi=(1, 2, 4, 0.9))
        L.append(fn(p))
        L.append({"shape": G.inter(s, sil), "fill": col, "name": f"a navy stem {nm}, from the bubble's foot down into the groove"})
        meas[nm] = {"shadow": [round(float(v), 2) for v in p], "params": "dx, dy, soft, alpha"}
    for nm, pts in CABLES:
        path = line(*pts, w=1.5, cap="round")
        kc = plain & (sdf.sd(line(*pts, w=1.0), X, Y) < 0.0)
        L.append({"shape": G.inter(path, sil), "fill": mode(kc), "soft": 0.4,
                  "name": f"a thin darker cable {nm} (one smooth path, round ends)"})
    fan = {"box": [x0 - 2, y0 - 2, x0 + w + 2, y0 + h + 2], "layers": L}
    scenes = {st: [fan] + items}
    sc = hp.score(el, scenes, z=3)
    print(f"  {el}: mad {sc[st][0]:.2f} edge {sc[st][1]}")
    hp.compare(el, scenes, z=3)
    hp.save_scene(el, scenes, {"how": HOW, "measured": meas})


def sdf_cx(s):
    if s["t"] == "rrect":
        return s["c"][0]
    return float(np.mean([p[0] for p in s["pts"]]))


HOW = ("the lavender fan (new reading): its authored outline (three circles round the mode bubbles and a body), its surface "
       "shaded by the distance from the groove (measured radial paint) with a thin rim lit along its top edges and dark along "
       "its right edge, a darker gradient inside the 3.6 px navy groove arc with a light lip; "
       "a navy stem under the course and the buildings bubble, each with its soft shadow; thin darker cables from the info, "
       "pause and tools buttons down out of the fan and from zoom in to rotate right; the three mode bubbles (without the "
       "ground their cuts carry) and seven small buttons (with their sockets) composed on top from their own scenes")

if __name__ == "__main__":
    main()
