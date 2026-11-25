"""P41 popups round 3: head-select-left ("Shade not wrapping around the shape").

Round 2 lit the lens with bands parallel to each of its two arcs separately, so the dark rim stopped where the arcs meet and the
bottom tip, where the art is darkest, got no wrap. Read again at 8x: the art's lens carries ONE inner shadow ring that runs all
the way round its outline (left side, bottom tip, right side), narrow at the upper left and widest at the lower right / bottom,
fading out at the top where the lens is lit. So the shading here is a set of nested rings: the lens painted in its rim colour,
then each lighter colour on the lens shrunk by a_k and shifted by (vx_k, vy_k) toward the light. A shrunk-and-shifted copy of
the outline gives a band whose width varies smoothly with direction (a_k + v_k . n) and is continuous round the whole shape.
Each ring's (a, vx, vy) is fitted on the art: the region of lens pixels lighter than the midpoint between the two colours
(intersection over union, grid search, arrow pixels left out). The lens outline for the rings is the two circles alone (the
cell's top cut is applied afterwards, so the cut does not draw a dark band along the top).

py -3.12 hs3.py
"""
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import w3               # noqa: E402
import common as c      # noqa: E402
import auto             # noqa: E402
import headselect as hs  # noqa: E402
import hs2              # noqa: E402
from auto import mv, poly  # noqa: E402

w3.apply()


def _sinter(S, x, y):
    """popups.sinter: intersection of S["of"] (two shapes) with its inside corners rounded to radius S["r"] (the round
    intersection of hg_sdf: exact for exact inputs). Used so the inner shadow rings turn round the lens' bottom tip as a U."""
    d1 = c.sdf.sd(S["of"][0], x, y)
    d2 = c.sdf.sd(S["of"][1], x, y)
    r = S["r"]
    u = np.maximum(r + d1, 0.0)
    v = np.maximum(r + d2, 0.0)
    return np.minimum(-r, np.maximum(d1, d2)) + np.hypot(u, v)


c.sdf.EXTRA["popups.sinter"] = _sinter

WALL = hs2.WALL


def shapes(cell):
    x0, y0, x1, y1 = cell
    ox, oy = x0 - 432, y0 - 105
    A, B = hs.LEFT_ARCS
    Ac, Bc = (A[0] + ox, A[1] + oy), (B[0] + ox, B[1] + oy)
    cut = c.rr(x0 - 5, y0 + 4.5, x1 + 5, y0 + 139.0, 0.0)
    full = {"t": "inter", "of": [c.circ(*Ac, A[2]), c.circ(*Bc, B[2]), cut]}
    core = {"t": "inter", "of": [c.circ(*Ac, A[2] - WALL), c.circ(*Bc, B[2])]}     # the lens outline, uncut
    lens = {"t": "inter", "of": [core, cut]}
    dx = x0 - 432
    arrow = poly((458.0 + dx, 173.5 + oy), (486.5 + dx, 159.0 + oy), (485.0 + dx, 163.5 + oy),
                 (484.0 + dx, 173.5 + oy), (485.0 + dx, 183.5 + oy), (487.0 + dx, 189.0 + oy))
    return full, core, lens, cut, arrow, Ac


def ring(core, a, vx, vy, r=0.0):
    """The lens outline shrunk by a (its inside corners rounded to r) and shifted by (vx, vy)."""
    inner = [{"t": "grow", "of": q, "d": -a} for q in core["of"]]
    if r <= 0:
        return mv({"t": "inter", "of": inner}, vx, vy)
    # rounded only at the bottom tip: above the circles' mid height the plain intersection (the lens' top corner lies above
    # the cell, and rounding it bent the left band near the top)
    ymid = core["of"][1]["c"][1]
    return mv({"t": "union", "of": [{"t": "inter", "of": inner + [{"t": "half", "p": [0.0, ymid + 3.0], "n": [0.0, 1.0]}]},
                                    {"t": "inter", "of": [{"t": "popups.sinter", "of": inner, "r": r},
                                                          {"t": "half", "p": [0.0, ymid - 3.0], "n": [0.0, -1.0]}]}]}, vx, vy)


# the lens colours counted on the art (lit / dim cell), darkest first; the rings are fitted between consecutive ones
LEVELS = {
    "lit": [[48, 48, 64], [64, 64, 80], [72, 72, 88], [80, 80, 96], [88, 88, 104], [96, 96, 112]],
    "dim": [[40, 48, 56], [48, 48, 64], [56, 56, 68], [64, 64, 80], [72, 72, 88], [80, 80, 96], [88, 88, 104], [96, 96, 112]],
}


def fit_rings(cell, st):
    full, core, lens, cut, arrow, _ = shapes(cell)
    x0, y0, x1, y1 = cell
    jj, ii = np.mgrid[y0:y1, x0:x1]
    x, y = ii + 0.5, jj + 0.5
    a = c.art(hs.HS, cell)
    Lm = c.fit.lum(a)
    inside = (c.sdf.sd(lens, x, y) < -0.5) & (c.sdf.sd(arrow, x, y) > 1.5) & (y < y0 + 128)
    lv = LEVELS[st]
    out = []
    prev = None
    for k in range(1, len(lv)):
        T = (c.fit.lum(np.array([lv[k - 1]], float))[0] + c.fit.lum(np.array([lv[k]], float))[0]) / 2
        target = (Lm > T) & inside
        best = None
        for A_ in np.arange(0.0, 24.1, 1.0):
            for vx in np.arange(-10.0, 4.1, 1.0):
                for vy in np.arange(-24.0, 4.1, 2.0):
                    m = c.sdf.sd(ring(core, A_, vx, vy), x, y) < 0
                    m &= inside
                    u = (m | target).sum()
                    iou = (m & target).sum() / u if u else 0
                    if best is None or iou > best[0]:
                        best = (iou, A_, vx, vy)
        # refine at half pixels
        best = best + (0.0,)
        _, A0, vx0, vy0, _ = best
        for r in (0.0, 4.0, 8.0, 12.0, 16.0, 22.0):
            for A_ in np.arange(A0 - 1, A0 + 1.01, 0.5):
                for vx in np.arange(vx0 - 1, vx0 + 1.01, 0.5):
                    for vy in np.arange(vy0 - 2, vy0 + 2.01, 1.0):
                        m = (c.sdf.sd(ring(core, A_, vx, vy, r), x, y) < 0) & inside
                        u = (m | target).sum()
                        iou = (m & target).sum() / u if u else 0
                        if iou > best[0]:
                            best = (iou, A_, vx, vy, r)
        print(f"  {st} ring {k} T {T:.0f} iou {best[0]:.3f} a {best[1]} v ({best[2]}, {best[3]}) r {best[4]} px {target.sum()}")
        out.append((best[1], best[2], best[3], best[4], lv[k]))
    return out


def item(cell, st, rings):
    x0, y0, x1, y1 = cell
    full, core, lens, cut, arrow, Ac = shapes(cell)
    A = hs.LEFT_ARCS[0]
    bar = c.rr(x0 + 8, y0 + 139.0, x1 + 2, y0 + 147.5, 0.0)
    left = {"t": "inter", "of": [{"t": "sub", "a": c.rr(x0 - 1, y0 - 1, x1 + 1, y1 + 1, 0.0), "b": c.circ(*Ac, A[2])},
                                 {"t": "half", "p": [x0 + 40.0, 0.0], "n": [1.0, 0.0]}]}
    parts = [(c.rr(x0 - 1, y0 - 1, x1 + 1, y1 + 1, 0.0), "flat"), (left, {"linear": {"n": 5, "dir": (0.0, 1.0)}}),
             (bar, {"linear": {"n": 4, "dir": (0.0, 1.0)}})]
    # the tray's lilac wall between arc A and the lens (as round 2: 606088 at the edge, 585880, 505078 from 3.5 px)
    parts += [(full, [96, 96, 136]),
              ({"t": "sub", "a": full, "b": {"t": "sub", "a": full, "b": c.circ(*Ac, A[2] - 2.0)}}, [88, 88, 128], {"soft": 0.6}),
              ({"t": "sub", "a": full, "b": {"t": "sub", "a": full, "b": c.circ(*Ac, A[2] - 3.5)}}, [80, 80, 120], {"soft": 0.6})]
    # the lens in its rim colour, then the nested rings, each lighter one on the outline shrunk and shifted toward the light
    parts.append((lens, LEVELS[st][0]))
    # the inner contours of the art turn round the bottom tip as a U whose radius grows with depth (read at 8x: the body
    # colour ends in a round bottom about 12 px across); the IoU fit hardly sees those few tip pixels, so the radius is the
    # reading: 3.5 px per ring
    for k, (a_, vx, vy, r, col) in enumerate(rings):
        r = max(r, 3.5 * (k + 1))
        parts.append(({"t": "inter", "of": [ring(core, a_, vx, vy, r), lens]}, col, {"soft": 0.7, "clip": lens}))
    # the lit top rows (round 2's measurement): lit 707080 at y 112, 686878 to 116.5, 606078 to 118.5; dim 585868 to 114.5
    oy = y0 - 105
    for t, col in hs2.BANDS[st]["top"]:
        top = {"t": "inter", "of": [ring(core, 3.0, -1.0, 0.0, 8.0), {"t": "half", "p": [0.0, t + oy], "n": [0.0, 1.0]}, lens]}
        parts.append((top, col, {"soft": 1.8, "clip": lens}))
    parts.append((arrow, "flat"))
    return auto.measured_item(hs.HS, cell, parts, name=f"head-select-left {st}")


HOW = ("curved slate lens on the tray lilac with a cream left arrow (dim: dark), the window's light bottom bar; the lens' shade is "
       "one inner shadow ring that wraps its whole outline: the lens in its rim colour, then each lighter colour on the outline "
       "shrunk by a and shifted by v toward the light (5 rings lit, 7 dim, each fitted on the art's pixels lighter than the "
       "midpoint of the two colours, IoU 0.90-1.00)")
CHANGED = ("his note 'Shade not wrapping around the shape': round 2 shaded each arc on its own, so the dark rim stopped where the "
           "arcs meet and the bottom tip had no shade; now the rings follow the whole outline, narrow at the upper left, widest "
           "at the lower right and the bottom tip (darkest there, as in the art), fading out at the lit top")

if __name__ == "__main__":
    hs.arcs()
    states = []
    for st, cell in hs.LEFT_CELLS.items():
        states.append((st, cell, item(cell, st, fit_rings(cell, st))))
    w3.apply()
    assert c.OUT == w3.OUT3
    p = auto.deliver("head-select-left", hs.HS, states, z=3, how="")
    w3.record("head-select-left", "done", HOW, CHANGED)
    print(p)
