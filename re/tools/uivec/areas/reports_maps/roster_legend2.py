"""P41 fix round (2026-11-24): roster-legend (redo), re-read from memberRoster.pcx rows 528..552.

What the legend IS (luminance dumps, every slot):
  - four BALLS, each inside a RING: a circle line (centre x 27 / 149 / 271 / 393, y 540.9; line centre r 6.7, 2 px, the
    pills' colour (92, 98, 141)); the ball fills the ring's inside and is lit from the top left (a highlight about 3 px up-left
    of the centre fading to the colour's dark side at the bottom right: silver 229 -> 147, gold 210 -> 121, red 206 -> 108,
    navy 108 -> 59 in luminance). Round one drew a bigger sphere with no ring.
  - the camera and the heart: the roster cell marks (roster_marks2), moved to their legend places; no ring.
  - a WIRE from each mark to its pill: a 2 px line (y 540..542) in the same colour.
  - six PILLS: a 2 px outline, line centre y 532.0 .. 550.0, the ends NOT half circles: corners r 7 with a 4 px straight
    vertical piece between (rows 539..542 sit on one column); left / right line centres read off row 541 below.
Ball paint: a radial gradient per ball, its centre, reach and colour stops solved by least squares on the ball's own pixels.
"""
import json
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import common as C          # noqa: E402
import parts as P           # noqa: E402
import roster_marks as RM   # noqa: E402
import roster_marks2 as RM2  # noqa: E402

SHEET = "memberRoster.pcx"
LINE = [92, 98, 141]
LAV = [148, 150, 198]
CY = 540.9
RING_R, RING_W = 6.7, 2.0
BALLS = [("navy", 27.0), ("silver", 149.0), ("gold", 271.0), ("red", 393.0)]
STOPS = [0.0, 0.2, 0.4, 0.6, 0.8, 1.0]
OVER = ("navy",)      # the navy ball's dark side covers its ring at the lower right (lum 72..81 where the ring is 101):
                      # drawn over the ring to the ring's outer edge; its light top-left (lum ~105) is the ring's tone there
FILE = os.path.join(RM2.OUT2, "read", "legend_balls.json")


def pill_ends():
    """Each pill's left / right line centre on row 541 (the dark runs' middles), from the art."""
    rgb, _ = C.sheet(SHEET)
    L = C.fit.lum(rgb)[541]
    out = []
    for x0, x1 in RM.LEG_PILLS:
        def centre(lo, hi):
            xs = np.arange(lo, hi)
            w = np.clip(154 - L[lo:hi], 0, None)
            return float((w * (xs + 0.5)).sum() / w.sum())
        out.append((round(centre(x0 - 3, x0 + 1), 2), round(centre(x1 - 1, x1 + 4), 2)))
    return out


def hat(t):
    t = np.clip(t, STOPS[0], STOPS[-1])
    out = []
    for k, s in enumerate(STOPS):
        w = np.zeros_like(t)
        if k > 0:
            a = STOPS[k - 1]
            w = np.where((t >= a) & (t <= s), (t - a) / (s - a), w)
        if k < len(STOPS) - 1:
            b = STOPS[k + 1]
            w = np.where((t > s) & (t <= b), (b - t) / (b - s), w)
        if k == len(STOPS) - 1:
            w = np.where(t >= s, 1.0, w)
        out.append(w)
    return np.stack(out, 1)


def solve():
    rgb, _ = C.sheet(SHEET)
    res = {}
    for name, cx in BALLS:
        jj, ii = np.mgrid[530:552, int(cx) - 10:int(cx) + 11]
        x, y = ii + 0.5, jj + 0.5
        rm = RING_R + RING_W / 2 - 0.6 if name in OVER else RING_R - RING_W / 2 - 0.6
        m = np.hypot(x - cx, y - CY) < rm
        Y = rgb[jj[m], ii[m]]
        best = None
        for hx in np.arange(-7.0, 0.51, 0.5):
            for hy in np.arange(-7.0, 0.51, 0.5):
                for R in (8.0, 9.0, 10.0, 11.0, 12.0, 14.0):
                    t = np.hypot(x[m] - cx - hx, y[m] - CY - hy) / R
                    A = hat(t)
                    X, *_ = np.linalg.lstsq(A, Y, rcond=None)
                    e = float(((A @ X - Y) ** 2).mean())
                    if best is None or e < best[0]:
                        best = (e, hx, hy, R, X)
        e, hx, hy, R, X = best
        used = hat(np.hypot(x[m] - cx - hx, y[m] - CY - hy) / R).sum(0) > 0.5
        cols = [[round(float(np.clip(v, 0, 255)), 1) for v in X[k]] for k in range(len(STOPS))]
        for k in range(len(STOPS)):          # a stop no pixel reaches takes its neighbour's colour
            if not used[k]:
                cols[k] = cols[k - 1] if k else cols[k + 1]
        res[name] = {"c": [round(cx + hx, 2), round(CY + hy, 2)], "r": R, "stops": [[s, c] for s, c in zip(STOPS, cols)],
                     "rms": round(e ** 0.5, 2)}
        print(name, res[name]["c"], R, "rms", res[name]["rms"], flush=True)
    os.makedirs(os.path.dirname(FILE), exist_ok=True)
    json.dump(res, open(FILE, "w"))
    return res


def scene(balls=None):
    balls = balls or json.load(open(FILE))
    ends = pill_ends()
    out = []
    marks_right = [cx + RING_R for _, cx in BALLS] + [142.9 + RM.CAM_OFF[0], 163.6 + RM.HEART_OFF[0]]
    for (pl, pr), xr in zip(ends, marks_right):
        out.append({"name": "wire", "box": [xr - 1, 537, pl + 1, 545],
                    "layers": [{"shape": P.rr(xr - 0.5, 540.0, pl, 542.0), "fill": LINE}]})
        out.append({"name": "legend pill", "box": [pl - 3, 528, pr + 3, 554],
                    "layers": [{"shape": {"t": "outline", "of": P.rr(pl, 532.0, pr, 550.0, 7.0), "w": 2.0}, "fill": LINE}]})
    for name, cx in BALLS:
        b = balls[name]
        ball = {"shape": {"t": "circle", "c": [cx, CY], "r": RING_R + (RING_W / 2 if name in OVER else 0)},
                "fill": {"radial": {"c": b["c"], "r": b["r"], "stops": b["stops"]}}}
        ring = {"shape": {"t": "outline", "of": {"t": "circle", "c": [cx, CY], "r": RING_R}, "w": RING_W}, "fill": LINE}
        out.append({"name": f"{name} ball", "box": [cx - 9, 531, cx + 9, 551],
                    "layers": [ring, ball] if name in OVER else [ball, ring]})
    out.append({"name": "camera", "box": [500, 528, 530, 552], "layers": RM2.camera(*RM.CAM_OFF)})
    out.append({"name": "heart", "box": [622, 528, 650, 552], "layers": RM2.heart(*RM.HEART_OFF)})
    return out


if __name__ == "__main__":
    print(pill_ends())
    solve()
