"""P41 popups round 3: shading a convex pad as nested authored bands, each fitted on the art.

A lit pad (a lens, a plate) is light along the edges that face the light and dark along the others. Its colour levels are
nested offset curves of its outline, so each level is drawn as one authored shape:
  - a level DARKER than the body: the region lighter than it is the outline shrunk by a and shifted by v ("ring"); painting
    the pad in its darkest colour and then each lighter ring on top leaves a band along the shadow side whose width varies
    smoothly round the outline (a + v . n);
  - a level LIGHTER than the body: the region lighter than it is a crescent along the lit side, the outline minus a copy of
    itself grown by g and shifted by v ("crescent").
fit_levels() finds (a, vx, vy) or (g, vx, vy) per level by intersection over union with the art's pixels lighter than the
midpoint between the two colours, over a mask the caller gives (the glyphs and their grooves left out). No pixel colour is
traced: each band is one shape with three numbers, painted in one art colour.
"""
import numpy as np

import common as c
from auto import mv


def ring(S, a, vx, vy):
    return mv({"t": "grow", "of": S, "d": -a}, vx, vy)


def crescent(S, g, vx, vy):
    return {"t": "sub", "a": S, "b": mv({"t": "grow", "of": S, "d": g}, vx, vy)}


def _lum(col):
    return float(c.fit.lum(np.array([col], float))[0])


def _iou(S, x, y, inside, target):
    m = (c.sdf.sd(S, x, y) < 0) & inside
    u = (m | target).sum()
    return (m & target).sum() / u if u else 0.0


def fit_levels(rel, box, S, levels, body_index, inside, grid=None, verbose=True, tag=""):
    """levels: colours darkest first; levels[body_index] is the body. Returns a list of (kind, p0, vx, vy, colour) bottom to
    top: the rings for levels[1..body_index] (darkest first), then the crescents for levels[body_index+1..] (lightest last)."""
    x0, y0, x1, y1 = box
    jj, ii = np.mgrid[y0:y1, x0:x1]
    x, y = ii + 0.5, jj + 0.5
    Lm = c.fit.lum(c.art(rel, box))
    g = grid or {}
    P = g.get("p", np.arange(-2.0, 12.1, 1.0))
    VX = g.get("vx", np.arange(-6.0, 6.1, 1.0))
    VY = g.get("vy", np.arange(-6.0, 6.1, 1.0))
    out = []
    for k in range(1, len(levels)):
        T = (_lum(levels[k - 1]) + _lum(levels[k])) / 2
        target = (Lm > T) & inside
        kind = "ring" if k <= body_index else "crescent"
        mk = ring if kind == "ring" else crescent
        best = (-1, 0, 0, 0)
        for p in P:
            for vx in VX:
                for vy in VY:
                    q = _iou(mk(S, p, vx, vy), x, y, inside, target)
                    if q > best[0]:
                        best = (q, p, vx, vy)
        _, p0, vx0, vy0 = best
        for p in np.arange(p0 - 0.75, p0 + 0.76, 0.25):
            for vx in np.arange(vx0 - 0.75, vx0 + 0.76, 0.25):
                for vy in np.arange(vy0 - 0.75, vy0 + 0.76, 0.25):
                    q = _iou(mk(S, p, vx, vy), x, y, inside, target)
                    if q > best[0]:
                        best = (q, p, vx, vy)
        if verbose:
            print(f"  {tag} {kind:8s} {k} T {T:5.1f} iou {best[0]:.3f} p {best[1]:.2f} v ({best[2]:.2f}, {best[3]:.2f}) "
                  f"px {int(target.sum())}")
        out.append((kind, float(best[1]), float(best[2]), float(best[3]), list(levels[k])))
    return out


def band_layers(S, fitted, soft=0.5):
    """The fitted levels as layers (to follow a layer that fills S with the darkest colour)."""
    out = []
    for kind, p, vx, vy, col in fitted:
        sh = ring(S, p, vx, vy) if kind == "ring" else crescent(S, p, vx, vy)
        out.append({"shape": {"t": "inter", "of": [sh, S]}, "fill": col, "soft": soft, "clip": S})
    return out
