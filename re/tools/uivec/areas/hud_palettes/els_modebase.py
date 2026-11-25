"""hud-mode-base: the lavender fan in the bottom left corner holding the three mode bubbles and the seven small buttons.

Read on the cut (3mainLowerLeft.pcx (0, 438), 213 x 162; 2026-11-24): its outline is the union of three circles round the
mode bubbles (fitted to the cut's edge: course (43.78, 473.76) r 36.41, buildings (117.5, 496.83) r 33.15, golfers
(175.61, 536.38) r 29.48) and a body whose right edge steps in at y 559 and runs out again to the cut's lower right corner.
The buttons are composed from their own elements' idle scenes at the places where their cuts match the fan's pixels (every
one is an exact copy there), with the socket each small button's sprite carries. Sheet coordinates.
"""
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hp  # noqa: E402
import bubble as B  # noqa: E402
import geom as G  # noqa: E402
import els_trays as TR  # noqa: E402
from hp import sdf  # noqa: E402

GROOVE = (19.97, 720.08, 198.32)      # the navy groove's centre line, fitted to its darkest pixels column by column
RINGS = [(43.78, 473.76, 36.41), (117.5, 496.83, 33.15), (175.61, 536.38, 29.48)]
BODY = G.poly((0.0, 472.0), (44.0, 474.0), (117.5, 497.0), (175.6, 536.0), (205.0, 545.0), (205.0, 551.0), (194.0, 559.0),
              (213.0, 598.0), (213.0, 600.0), (0.0, 600.0))
SIL = G.U(*[G.circle(*r) for r in RINGS], BODY)
PARTS = [("hud-course-mode", (7, 437), 3), ("hud-buildings-mode", (83, 463), 3), ("hud-golfers-mode", (145, 505), 3),
         ("hud-zoom-in", (20, 530), 40), ("hud-zoom-out", (20, 560), 40), ("hud-rotate-right", (10, 545), 40),
         ("hud-rotate-left", (40, 545), 40), ("hud-info", (60, 530), 40), ("hud-pause", (95, 555), 40), ("hud-tools", (120, 575), 40)]


def main():
    el = "hud-mode-base"
    st, src, x0, y0, rgb, mask = hp.crops(el)[0]
    h, w = mask.shape
    X, Y = B.grid(x0, y0, w, h)
    items, covered = [], np.zeros(mask.shape, bool)
    for pid, guess, reach in PARTS:
        cut = hp.crops(pid)[0]
        _, _, ox, oy, crgb, cmask = cut
        best = TR.match(rgb, mask, x0, y0, crgb, cmask, guess, reach=reach)
        print(f"  {pid}: at {best[1]}, {best[2]} (mean error {best[0]:.1f})")
        items += TR.composed(pid, cut[0], (best[1], best[2]), drop=())
        jj, ii = best[2] - y0, best[1] - x0
        covered[max(jj, 0):jj + cmask.shape[0], max(ii, 0):ii + cmask.shape[1]] |= cmask[max(-jj, 0):, max(-ii, 0):][:h - max(jj, 0), :w - max(ii, 0)]
    import bel
    sil = G.moved(bel.extent_poly(mask), x0, y0)
    plain = mask & ~covered
    dg = np.hypot(X - GROOVE[0], Y - GROOVE[1])
    inner = G.inter(sil, G.circle(GROOVE[0], GROOVE[1], GROOVE[2]))
    groove = G.inter(G.outlined(G.circle(*GROOVE), 3.6), sil)
    lip = G.inter(G.outlined(G.circle(GROOVE[0], GROOVE[1], GROOVE[2] + 3.0), 1.8), sil)
    def mode(k):
        return TR.mode_rgb(rgb[k]) if k.any() else [60, 60, 100]
    fan = {"box": [x0 - 2, y0 - 2, x0 + w + 2, y0 + h + 2],
           "layers": [{"shape": sil, "fill": TR.measured_linear(rgb, plain & (dg > GROOVE[2] + 4), x0, y0),
                       "name": "the lavender fan (outline: the cut's extent), its upper part"},
                      {"shape": inner, "fill": TR.measured_linear(rgb, plain & (dg < GROOVE[2] - 3), x0, y0),
                       "name": "the fan's lower part, inside the groove"},
                      {"shape": lip, "fill": mode(plain & (np.abs(dg - GROOVE[2] - 2.4) < 0.6)), "name": "a light lip along the groove's outer side"},
                      {"shape": groove, "fill": mode(plain & (np.abs(dg - GROOVE[2]) < 0.8)), "name": "the groove: a 3.6 px navy arc round (20, 720)"}]}
    scenes = {st: [fan] + items}
    sc = hp.score(el, scenes, z=3)
    print(f"  {el}: mad {sc[st][0]:.2f} edge {sc[st][1]}")
    hp.compare(el, scenes, z=3)
    hp.save_scene(el, scenes, {"how": "lavender fan (outline: the cut's extent, simplified), lighter above and darker below a "
                                      "3.6 px navy groove arc (centre (20, 720), r 198) with a light lip on its outer side; the "
                                      "three mode bubbles and seven small buttons composed from their own scenes where their cuts "
                                      "match the fan's pixels"})


if __name__ == "__main__":
    main()
