"""P41 hud_panels fix round (2026-11-24): the five shot-shape pads redone (all five were "redo").

Re-read from luminance maps of every pad cell (J 0..300, 1..201; 80 x 49), cell-relative coordinates:
- Navy ring: 1.5 px all round (column 40: light lip row 1, ring rows 2-3 at the top and 38-39 at the bottom; row 20: x 7-8
  and 71-72). Its inner edge fitted per cell on rays from the inside (first pixel under luminance 70; disabled: 45):
  idle round (39.8, 20.1) 31.7 x 17.5; pressed cells (selected, hover) one pixel lower.
- Lip: above the ring a light lavender band whose outer edge is a dark one-pixel line, an ellipse round (40.0, 24.1)
  39.4 x 24.4 fitted to its dark pixels; seen at the top only (it meets the ring at rows 9-12 on both sides).
- Inside: a flat body colour (teal / green / dark) with a big light crescent hugging the upper left inside the ring: the
  inside minus an ellipse round (76.0, 24.3) 67.5 x 19.7 fitted to the crescent's lower edge (13 points read off the map).
  Round 1 painted the inside row by row (it banded); here it is two measured layers with a soft edge between them.
- Cast shadow below the ring (rows 40-47 fading 69 -> 104), soft.
- Shots: one white line each, ONE continuous path with round joins (round 1 drew the flat shot's kink as a separate
  stroke and the low shot's bounce as a second line), above its dark ground shadow line(s).
"""
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import common as C  # noqa: E402
import kit as K  # noqa: E402
import el_bubbles as EB  # noqa: E402
import el_pads as P1  # noqa: E402
import figs  # noqa: E402
import fit  # noqa: E402
import sdf  # noqa: E402
from kit import U, rect, circle, ellipse, poly, sub, inter, outline  # noqa: E402,F401

limb = figs.limb
W_SHOT = 1.5     # the white line: one and a half pixels (two-pixel diagonal steps, two rows where it runs level)
BALLS = {   # the ball on each shot: a white disc on the line and its brown shadow pixel (6b6352) below it (high: none)
    "low": ((32.0, 13.3), (33.2, 15.0)), "rising": ((35.8, 7.0), (36.6, 8.8)), "flat": ((35.2, 17.9), (36.3, 19.6)),
    "high": ((48.6, 8.0), None), "straight": ((34.2, 18.0), (35.3, 19.7)),
}

SHOTS = {   # white path, ground shadow paths (cell-relative)
    "low": ([(15.0, 23.4), (16.5, 22.3), (18.0, 20.9), (20.5, 19.3), (22.5, 18.1), (24.5, 16.8), (27.0, 15.4), (29.5, 14.0),
             (32.0, 12.7), (35.0, 12.1), (38.0, 11.9), (41.0, 12.0), (44.0, 12.5), (46.0, 13.6), (48.0, 14.9), (49.5, 16.3),
             (50.5, 17.7), (51.4, 19.4), (52.3, 21.0), (53.2, 22.6), (55.0, 22.2), (57.5, 21.9), (60.0, 22.5), (62.8, 23.4)],
            [[(17.0, 23.5), (27.5, 23.5)], [(26.5, 22.5), (50.5, 22.5)]]),
    "rising": ([(17.4, 23.4), (19.0, 21.0), (21.0, 18.7), (23.0, 16.4), (25.0, 14.2), (27.0, 12.2), (29.0, 10.6), (31.0, 9.0),
                (33.0, 7.8), (35.5, 6.4), (38.5, 5.8), (41.5, 5.9), (44.5, 6.6), (46.5, 7.6), (48.0, 9.0), (49.2, 10.8),
                (50.2, 12.6), (51.0, 14.6), (51.9, 16.8), (52.6, 18.8), (53.3, 20.8), (54.2, 22.4), (56.2, 23.3),
                (58.2, 24.6), (59.8, 26.0), (61.2, 27.4), (62.4, 28.4)],
               [[(20.6, 22.4), (22.6, 20.6), (25.2, 19.5), (27.4, 18.4), (30.0, 17.3), (33.0, 16.3), (36.5, 15.3), (38.0, 14.6),
                 (46.0, 14.6), (48.6, 15.6), (50.2, 16.6), (50.8, 17.5)]]),
    "flat": ([(18.2, 21.6), (20.5, 20.6), (24.0, 20.0), (28.0, 19.4), (31.5, 18.7), (35.0, 18.0), (40.0, 17.7), (45.0, 18.0),
              (48.5, 18.9), (51.0, 19.8), (53.0, 20.7), (54.6, 21.4), (55.8, 20.5), (57.2, 19.4), (59.5, 18.6), (61.6, 17.8),
              (63.4, 16.5)],
             [[(20.6, 22.5), (23.5, 23.6), (26.0, 24.5), (29.0, 25.5), (33.0, 26.5), (37.5, 27.5), (43.0, 28.4), (47.5, 27.8),
               (50.0, 26.9), (52.2, 25.6), (53.4, 24.4), (54.4, 23.3), (56.2, 21.8), (58.6, 20.6)]]),
    "high": ([(22.5, 24.6), (23.1, 22.5), (24.0, 20.0), (25.0, 17.6), (26.0, 15.5), (27.0, 13.6), (28.1, 12.0), (29.1, 10.5),
              (30.1, 9.1), (31.1, 8.0), (32.3, 6.6), (33.6, 5.4), (35.0, 4.3), (36.8, 3.3), (38.6, 2.6), (40.8, 2.4), (43.0, 2.6),
              (44.6, 3.3), (45.6, 4.4), (46.6, 5.4), (47.6, 6.7), (48.6, 8.2), (49.6, 10.0), (50.4, 11.8), (51.2, 13.6),
              (51.9, 15.4), (52.5, 17.2), (53.1, 19.0), (53.7, 20.8), (54.3, 22.4), (54.8, 23.4), (53.0, 23.5), (50.6, 23.7),
              (48.6, 24.4), (46.4, 24.6)],
             [[(24.0, 24.5), (27.5, 23.5), (31.0, 23.4), (35.5, 22.6), (42.0, 22.5), (50.6, 22.5)]]),
    "straight": ([(20.4, 21.6), (22.4, 20.6), (24.6, 19.9), (28.0, 19.4), (31.0, 18.8), (33.2, 17.8), (34.6, 17.2), (36.2, 17.9),
                  (38.0, 18.7), (40.6, 19.5), (42.8, 20.2), (44.6, 21.2), (46.6, 20.6), (49.5, 20.4), (52.4, 20.7),
                  (54.6, 21.5), (57.5, 21.6)],
                 [[(25.5, 21.5), (42.5, 21.5)], [(49.0, 21.5), (50.8, 21.5)]]),
}
ORDER = ("low", "rising", "flat", "high", "straight")
LIP = (40.0, 24.1, 39.4, 23.8)   # fitted 24.4 to the dark edge pixels' centres; the band is one pixel at the top
CRES = (76.0, 24.3, 67.5, 19.7)


def ring_inner(key, rct, st):
    rgb, m = C.sheet(key)
    L = fit.lum(rgb)
    x0, y0, w, h = rct
    cx, cy = x0 + 40.5, y0 + 20.5
    thr = 45 if st == "disabled" else 70
    pts = []
    for th in np.deg2rad(np.arange(0, 360, 3)):
        ex, ey = np.cos(th), np.sin(th)
        R = 1 / np.sqrt((ex / 30.5) ** 2 + (ey / 18.5) ** 2)
        for s in np.arange(0.85 * R, 1.2 * R, 0.2):
            x, y = cx + ex * s, cy + ey * s
            if L[int(y), int(x)] < thr:
                pts.append((x, y))
                break
    ex_, ey_, rx, ry, _ = fit.fit_ellipse(np.array(pts), rot=0.0)
    return ex_, ey_, rx, ry


def pad_layers(key, rct, st, shot):
    x0, y0, w, h = rct
    rgb, m = C.sheet(key)
    cx, cy, rx, ry = ring_inner(key, rct, st)
    if st == "disabled":       # the dark inside defeats the ray test: the ring is the idle one's (same place in the art)
        cx, cy, rx, ry = ring_inner(key, (0, y0, w, h), "idle")
        cx += x0
    dyp = cy - (y0 + 20.13)
    L = []
    # block: the art's column profile (round 1)
    rows = [j for j in range(h) if m[y0 + j, x0 + 1]]
    top = y0 + rows[0]
    stp = [[(r - top) / max(y0 + h - 1 - top, 1), K.sample(key, rect(x0 + 1, r, x0 + 2, r + 1), depth=0.0)] for r in range(top, y0 + h)]
    block = rect(x0, top, x0 + w, y0 + h)
    L.append(K.lay(block, {"linear": {"a": [0, top + 0.5], "b": [0, y0 + h - 0.5], "stops": stp}}))
    # cast shadow
    shc = K.sample(key, rect(int(cx) - 2, int(cy + ry) + 2, int(cx) + 2, int(cy + ry) + 3), depth=0.0)
    L.append(K.lay(ellipse(cx + 1.0, cy + 2.6, rx + 1.6, ry + 1.6), shc, soft=3.0, clip=block, alpha=0.95))
    # lip: light band with a dark outer line, top part only
    lip = ellipse(x0 + LIP[0], y0 + LIP[1] + dyp, LIP[2], LIP[3])
    top_only = {"t": "half", "p": [0, y0 + 10.0 + dyp], "n": [0, 1]}     # the lip's dark edge meets the ring at row 9-10
    lipc = K.sample(key, U(rect(x0 + 19, y0 + 4 + round(dyp), x0 + 21, y0 + 5 + round(dyp)),
                            rect(x0 + 38, y0 + 1 + round(dyp), x0 + 43, y0 + 2 + round(dyp))), depth=0.0)
    L.append(K.lay(inter(lip, top_only), lipc))
    dots = [(x0 + 24, y0 + 2), (x0 + 18, y0 + 4), (x0 + 12, y0 + 7), (x0 + 55, y0 + 2), (x0 + 61, y0 + 4), (x0 + 67, y0 + 7)]
    lipd = [round(float(v), 1) for v in np.median(np.array([rgb[y + round(dyp), x] for x, y in dots]), axis=0)]
    L.append(K.lay(inter(outline(lip, 0.9), top_only), lipd, alpha=0.75))    # the art's edge is a one-pixel staircase
    # navy ring, 1.5 px
    outer = ellipse(cx, cy, rx + 1.0, ry + 1.0)
    inner = ellipse(cx, cy, rx - 0.5, ry - 0.5)
    ringc = K.sample_dark(key, sub(outer, inner), q=0.5, grow=0.0)
    L.append(K.lay(outer, ringc))
    # inside: body colour + the light crescent
    lines = U(limb([(x0 + x, y0 + y, 2.2) for x, y in shot[0]]), *[limb([(x0 + x, y0 + y, 2.0) for x, y in p]) for p in shot[1]])
    cres_cut = ellipse(x0 + CRES[0], y0 + CRES[1] + dyp, CRES[2], CRES[3])
    cres = sub(inner, cres_cut)
    body = sub(sub(ellipse(cx, cy, rx - 1.8, ry - 1.8), {"t": "grow", "of": cres, "d": 1.2}), lines)
    bodyc = K.sample(key, body, depth=0.5)
    # the body darkens a little toward the bottom on the disabled pad: a vertical linear paint through 3 bands
    bands = []
    for t, (ya, yb) in zip((0.2, 0.55, 0.9), ((cy - 4, cy), (cy, cy + 6), (cy + 9, cy + ry - 2))):
        try:
            bands.append([t, K.sample(key, inter(body, rect(cx - rx, ya, cx + rx, yb)), depth=0.5)])
        except Exception:
            pass
    paint = {"linear": {"a": [0, cy - ry], "b": [0, cy + ry], "stops": bands}} if len(bands) == 3 else bodyc
    L.append(K.lay(inner, paint))
    cresc = K.sample(key, sub({"t": "grow", "of": cres, "d": -1.0}, lines), depth=0.3)
    L.append(K.lay(cres, cresc, soft=0.9, clip=inner))
    # shot
    for p in shot[1]:
        g = limb([(x0 + x, y0 + y, 1.05) for x, y in p])
        L.append(K.lay(g, K.sample_dark(key, g, q=0.35), clip=inner))
    wl = limb([(x0 + x, y0 + y, W_SHOT) for x, y in shot[0]], smooth=1)
    white = P1._brightest(key, wl)
    L.append(K.lay(wl, white, clip=inner))
    (bx, by), sh = shot[2]
    if sh is not None:
        L.append(K.lay(circle(x0 + sh[0], y0 + sh[1], 0.7), [round(float(v), 1) for v in rgb[int(y0 + sh[1]), int(x0 + sh[0])]],
                       clip=inner))
    L.append(K.lay(circle(x0 + bx, y0 + by, 1.3), white, clip=inner))
    return L


def build_pad(el):
    k = el.split("shot-shape-")[1]
    scenes = {}
    for st, key, rct in C.CELLS[el]:
        x0, y0, w, h = rct
        shot = SHOTS[k] + (BALLS[k],)
        if st in ("selected", "hover"):     # pressed: the pad and its shot one pixel lower in the art
            b_, sh_ = BALLS[k]
            shot = ([(x, y + 1.0) for x, y in shot[0]], [[(x, y + 1.0) for x, y in p] for p in shot[1]],
                    ((b_[0], b_[1] + 1.0), None if sh_ is None else (sh_[0], sh_[1] + 1.0)))
        scenes[st] = [K.item(pad_layers(key, rct, st, shot), box=[x0, y0, x0 + w, y0 + h])]
    return scenes


EB.BUILD.update({f"shot-shape-{k}": build_pad for k in ORDER})
