"""P41 round 3 (2026-11-24): route-course-button, verdict fix, note "Arrow not consistent weight".

The arrow is the route round the pin: it starts at the hook top right of the left mark (295.6, 25.9), curves down to the
left point (284.5, 42.4), turns and sweeps up to the pole, passes behind it and runs down to the arrowhead block at the
lower right. Round 2 drew it as two tapered pieces (hook 1 -> 2.3 px, arc 1 -> 3.6 -> 5 px), so its weight changed along
the way. Now it is ONE stroke of constant width W with round caps and round joins: the three quadratics of round 2's
reading (same control points) sampled into one polyline. Everything else (pole, foot, pennant, block, horn, notch, gap
behind the pole, slit, core line, bubble) is round 2's, unchanged.
"""
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import common as C          # noqa: E402
import deliver2 as D2       # noqa: E402
import route_buttons as RB  # noqa: E402
import route_buttons2 as R2  # noqa: E402

W = float(os.environ.get("RC3_W", "2.8"))


def quad(p0, p1, p2, n=24):
    t = np.linspace(0.0, 1.0, n)[:, None]
    P = (1 - t) ** 2 * np.array(p0) + 2 * (1 - t) * t * np.array(p1) + t ** 2 * np.array(p2)
    return [[float(x), float(y)] for x, y in P]


def flag():
    pole = RB.R_(300.9, 18.4, 304.6, 60.2, 0.8)
    foot = RB.R_(298.8, 59.8, 307.3, 62.6, 1.4)
    pennant = R2.poly((304.4, 18.6), (306.8, 19.2), (310.6, 20.4), (315.4, 21.2), (319.4, 22.0), (322.4, 23.6), (324.4, 25.6),
                      (325.4, 27.4), (321.0, 27.8), (317.0, 28.0), (313.4, 29.2), (309.8, 30.4), (306.4, 31.4), (304.4, 31.4),
                      smooth=1)
    # the arrow: hook top -> left point -> pole -> arrowhead, one path, one width
    pts = (quad([295.6, 25.9], [283.4, 27.2], [284.9, 41.6], 22)
           + quad([284.9, 41.6], [291.45, 34.55], [300.8, 34.3], 20)[1:]
           + quad([300.8, 34.3], [316.25, 37.95], [320.0, 49.0], 26)[1:])
    arrow = {"t": "stroke", "pts": pts, "w": W, "cap": "round"}
    block = R2.poly((307.5, 48.2), (314.0, 48.2), (321.8, 48.6), (323.6, 50.4), (322.6, 51.8), (318.6, 52.6), (319.2, 55.0),
                    (320.8, 58.9), (307.5, 58.9))
    horn = RB.R_(307.6, 47.3, 310.2, 50.0, 0.8)
    gap = RB.R_(299.9, 31.0, 305.5, 38.6, 0.6)
    core = RB.R_(301.7, 19.6, 302.6, 60.2, 0.4)
    slit = RB.R_(304.75, 21.8, 305.6, 27.8, 0.3)
    notch = R2.poly((310.6, 47.0), (313.6, 47.0), (312.2, 49.6))
    arrow_cut = R2.sub(arrow, R2.sub(gap, RB.R_(299.0, 30.0, 300.4, 40.0)))
    return R2.sub(R2.U(pole, foot, pennant, arrow_cut, block, horn), R2.U(core, slit, notch))


OUT3 = os.path.join(C.ROOT, "log", "roadmap", "P41", "work3", "reports_maps")


def main():
    D2.OUT2 = OUT3
    D2.RES_JSON = os.path.join(OUT3, "results.json")
    D2.COLS = ["id", "status", "changed", "unsure", "how", "interior_mad"]
    R2.G2["course"] = flag
    R2.WHAT["course"] = ("the course glyph from its parts: a lit pole (dark, a light core line the whole height) on a rounded "
                         "foot; a pennant from its top curving out to a pointed tip, with the light slit by the pole; the ARROW "
                         "as one path of one width (%.1f px, round ends and joins): from the hook top down to the left point, "
                         "up to the pole, behind it (light gap at the pole) and down to the arrowhead block with its horn and "
                         "light V notch" % W)
    R2.CHANGED["course"] = ("the arrow now has one weight end to end: one stroke %.1f px wide along the same path (was a hook "
                            "tapering 1 -> 2.3 px and an arc tapering 1 -> 3.6 -> 5 px); the left point is now a rounded turn "
                            "of the same width instead of two tapers meeting; pole, pennant, block and bubble unchanged" % W)
    R2.run("course")


if __name__ == "__main__":
    main()
