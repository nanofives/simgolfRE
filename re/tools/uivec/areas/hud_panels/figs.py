"""P41 hud_panels fix round (2026-11-24): shapes for figures of people and other small pictures with volume.

hud_panels.limb: a limb, a club shaft, a mouth line: a polyline through points with a width AT EACH POINT, round joins and
round ends; each segment is an exact uneven capsule (a round cone), so a bent leg is one continuous shape whose thigh can be
thicker than its shin, and a curve never breaks at its joins. "smooth": n rounds of corner cutting (ends kept, widths
interpolated). "cap": "butt" cuts the two end segments flat at their own ends (the hook of a club head stays).

Usage from a builder: figs.limb([(x, y, w), ...], smooth=0) -> shape.
"""
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import common as C  # noqa: E402,F401
import sdf  # noqa: E402


def _smooth(pts, n):
    for _ in range(int(n)):
        out = [pts[0]]
        for a, b in zip(pts, pts[1:]):
            for f in (0.25, 0.75):
                out.append([a[k] * (1 - f) + b[k] * f for k in range(3)])
        out.append(pts[-1])
        pts = out
    return pts


def _cone(x, y, a, b, r1, r2):
    """Signed distance to the round cone from a (radius r1) to b (radius r2)."""
    ex, ey = b[0] - a[0], b[1] - a[1]
    h = max(np.hypot(ex, ey), 1e-9)
    ux, uy = ex / h, ey / h
    px, py = x - a[0], y - a[1]
    v = px * ux + py * uy          # along
    u = np.abs(-px * uy + py * ux)  # across
    if abs(r1 - r2) >= h:          # one end swallows the other
        return np.minimum(np.hypot(px, py) - r1, np.hypot(x - b[0], y - b[1]) - r2)
    s = (r1 - r2) / h
    c = np.sqrt(1 - s * s)
    # iq's sdUnevenCapsule in the frame (across, along): k = dot(q, (-s, c))
    k = -s * u + c * v
    d_mid = u * c + v * s - r1
    d_a = np.hypot(u, v) - r1
    d_b = np.hypot(u, v - h) - r2
    return np.where(k < 0, d_a, np.where(k > c * h, d_b, d_mid))


def _limb(S, x, y):
    pts = _smooth([list(map(float, p)) for p in S["pts"]], S.get("smooth", 0))
    d = np.full(x.shape, 1e18)
    for k, (a, b) in enumerate(zip(pts, pts[1:])):
        dk = _cone(x, y, a, b, a[2] / 2, b[2] / 2)
        if S.get("cap") == "butt":
            for end, nxt, last in ((pts[0], pts[1], k == 0), (pts[-1], pts[-2], k == len(pts) - 2)):
                if last:
                    ux, uy = end[0] - nxt[0], end[1] - nxt[1]
                    n = max(np.hypot(ux, uy), 1e-9)
                    dk = np.maximum(dk, ((x - end[0]) * ux + (y - end[1]) * uy) / n)
        d = np.minimum(d, dk)
    if len(pts) == 1:
        d = np.hypot(x - pts[0][0], y - pts[0][1]) - pts[0][2] / 2
    return d


sdf.EXTRA["hud_panels.limb"] = _limb


def limb(pts, smooth=0, cap="round"):
    S = {"t": "hud_panels.limb", "pts": [[float(p[0]), float(p[1]), float(p[2])] for p in pts], "smooth": smooth}
    if cap != "round":
        S["cap"] = cap
    return S


def _rrect4(S, x, y):
    """hud_panels.rrect4: a rounded rectangle with its own radius at each corner ("r": [tl, tr, br, bl]), an exact
    distance (iq's rounded box with the radius picked per quadrant), so a lit surface on it has no seam."""
    x0, y0, x1, y1 = S["box"]
    cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
    hx, hy = (x1 - x0) / 2, (y1 - y0) / 2
    tl, tr, br, bl = S["r"]
    px, py = x - cx, y - cy
    r = np.where(px > 0, np.where(py > 0, br, tr), np.where(py > 0, bl, tl))
    qx = np.abs(px) - hx + r
    qy = np.abs(py) - hy + r
    return np.minimum(np.maximum(qx, qy), 0) + np.hypot(np.maximum(qx, 0), np.maximum(qy, 0)) - r


sdf.EXTRA["hud_panels.rrect4"] = _rrect4


def rrect4(x0, y0, x1, y1, tl, tr, br, bl):
    return {"t": "hud_panels.rrect4", "box": [float(x0), float(y0), float(x1), float(y1)], "r": [tl, tr, br, bl]}
