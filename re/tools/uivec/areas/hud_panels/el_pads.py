"""P41 hud_panels: the Golfer action panel's five shot-shape pads and the Employee panel's pedestal.

Shot pad (J 0,1 .. 300,201; 80 x 49): a lavender block (rows 9.. of the cell, lit from the top: the art's column profile)
with an oval pad on it: a navy line (an ellipse fitted to the art's darkest ring), the inside lit from the upper left (a
light crescent along the upper left inside the line, the art's colours across the pad as a linear paint along the light's
direction), the pad's soft shadow on the block below it, and the shot: one white trajectory line (read per pad off a
character map, 2026-11-24) above its dark ground shadow line.
"""
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import common as C  # noqa: E402
import kit as K  # noqa: E402
import el_bubbles as EB  # noqa: E402
import fit  # noqa: E402
import sdf  # noqa: E402
from kit import U, line, curve, poly, rect, circle, ellipse, inter, sub, outline  # noqa: E402,F401

# per pad (row r of the sheet, y0 = 1 + 50 r): the white shot line(s) and the dark ground line(s), cell-relative (x, y - y0)
SHOTS = {
    "low": ([[(15, 23.5), (18, 21.5), (21, 19.5), (24, 17.2), (27, 15.4), (30, 13.6), (33, 12.2), (37, 11.8), (41, 12.2),
              (44, 12.8), (47, 14), (49, 15.5), (50, 17.4), (51.5, 19.5), (52.5, 21.5), (53, 22.6)],
             [(53, 22.6), (56, 21.6), (58, 21.4), (60, 22.4), (63, 23.0), (66, 23.6)]],
            [[(17, 23.5), (23.5, 23.4)], [(27, 22.5), (49.5, 22.5)]]),
    "rising": ([[(17, 23.0), (20, 20.0), (23, 17.0), (26, 14.0), (29, 11.5), (31, 9.5), (33, 7.6), (36, 5.6), (40, 3.6),
                 (44, 5.6), (47, 7.6), (48.6, 10), (49.6, 14), (50.6, 18), (52, 21), (54, 22.6), (57, 23.8), (59, 25),
                 (61, 27), (62.4, 28.4)]],
               [[(21, 21.6), (25, 19.2), (29, 16.6), (34, 14.6), (37, 14.3), (47, 14.6), (49, 16.0), (50, 17.4)]]),
    "flat": ([[(18, 21.0), (21, 20.0), (25, 19.2), (30, 18.2), (35, 17.4), (40, 17.2), (45, 17.6), (49, 18.6), (52, 20.0)],
              [(56, 20.6), (59.5, 18.4), (63.6, 16.0)]],
             [[(20, 22.4), (23, 23.4), (26, 24.4), (29, 25.4), (33, 26.4), (38, 27.3), (43, 28.4), (47, 27.6), (50, 26.6),
               (52.5, 25.0), (54, 23.4), (55, 22.4)]]),
    "high": ([[(22.5, 24.0), (24, 20.0), (26, 16.0), (28, 12.0), (30, 8.0), (32, 5.0), (34, 3.2), (37, 2.2), (40, 2.0),
               (43, 2.4), (45, 3.6), (47, 5.6), (49, 8.0), (50.4, 12), (51.4, 16), (52.4, 20), (53.6, 22.6)],
              [(53.6, 22.6), (50, 23.2), (47, 23.8), (44, 24.4)]],
             [[(24, 24.4), (29, 23.4), (35, 22.6), (49.6, 22.4)]]),
    "straight": ([[(20, 21.4), (22, 20.6), (25, 19.6), (29, 18.6), (31, 17.6), (34, 16.6), (36, 17.0), (38, 18.4), (42, 19.6),
                   (48, 20.2), (53, 20.6), (57, 21.4)]],
                 [[(25, 21.5), (41.5, 21.5)]]),
}
ORDER = ("low", "rising", "flat", "high", "straight")


def pad_ellipse(key, rct):
    """The navy line of the pad: an ellipse fitted to the outermost dark pixels (luminance < 70) seen from the centre."""
    rgb, m = C.sheet(key)
    x0, y0, w, h = rct
    a = rgb[y0:y0 + h, x0:x0 + w]
    dk = (fit.lum(a) < 70) & m[y0:y0 + h, x0:x0 + w]
    jj, ii = np.nonzero(dk)
    c0 = (40.5, 19.5)
    pts = fit.outermost(dk, c0, rays=160)
    pts = pts[pts[:, 1] < 37.5]          # the lower edge: the shadow below is dark too
    cx, cy, rx, ry, rot = fit.fit_ellipse(pts, rot=0.0)
    return cx + x0, cy + y0, rx + 0.4, ry + 0.4


def pad_layers(key, rct, e, shot):
    x0, y0, w, h = rct
    cx, cy, rx, ry = e
    L = []
    rgb, m = C.sheet(key)
    rows = [j for j in range(h) if m[y0 + j, x0 + 1]]
    top = y0 + rows[0]
    st = [[(r - top) / max(y0 + h - 1 - top, 1), K.sample(key, rect(x0 + 1, r, x0 + 2, r + 1), depth=0.0)] for r in range(top, y0 + h)]
    block = rect(x0, top, x0 + w, y0 + h)
    L.append(K.lay(block, {"linear": {"a": [0, top + 0.5], "b": [0, y0 + h - 0.5], "stops": st}}))
    shade = K.sample(key, rect(int(cx) - 1, int(cy + ry) + 3, int(cx) + 1, int(cy + ry) + 4), depth=0.0)
    L.append(K.lay(ellipse(cx + 0.5, cy + 3.0, rx + 1.5, ry + 1.0), shade, soft=2.5, clip=block, alpha=0.9))
    E = ellipse(cx, cy, rx, ry)
    L.append(K.lay(E, K.sample_dark(key, outline(ellipse(cx, cy, rx - 0.5, ry - 0.5), 1.0), q=0.4)))
    inner = ellipse(cx, cy, rx - 1.2, ry - 1.2)
    # the inside: the art's colour row by row (the median across each row of the pad, the shot's white and dark pixels
    # left out), which carries the light top rim and the darker foot; then the light crescent along the upper left
    rgb, m = C.sheet(key)
    jj, ii = np.mgrid[int(cy - ry):int(cy + ry) + 1, int(cx - rx):int(cx + rx) + 1]
    a_ = rgb[jj, ii]
    Lm = fit.lum(a_)
    ins = ((ii + 0.5 - cx) / (rx - 1.6)) ** 2 + ((jj + 0.5 - cy) / (ry - 1.6)) ** 2 < 1
    lines = U(*[curve(*[(x0 + x, y0 + y) for x, y in pts], w=1.3, smooth=1) for pts in shot[0] + shot[1]])
    keep = ins & (sdf.sd(lines, ii + 0.5, jj + 0.5) > 1.6)
    stops = []
    y_top, y_bot = cy - ry + 1.2, cy + ry - 1.2
    for r in range(jj.shape[0]):
        sel = keep[r]
        if sel.sum() >= 3:
            yy = jj[r, 0] + 0.5
            stops.append([float((yy - y_top) / (y_bot - y_top)), [round(float(v), 1) for v in np.median(a_[r][sel], axis=0)]])
    stops = [[min(max(t, 0.0), 1.0), c] for t, c in stops]
    L.append(K.lay(inner, {"linear": {"a": [0, y_top], "b": [0, y_bot], "stops": stops}}))
    cres = inter(inner, sub(ellipse(cx - 1.0, cy - 1.2, rx - 2.2, ry - 2.2), ellipse(cx + 1.6, cy + 1.6, rx - 2.4, ry - 2.2)))
    L.append(K.lay(cres, K.sample(key, rect(x0 + 14, y0 + 10, x0 + 16, y0 + 11), depth=0.0), alpha=0.6))
    whites, darks = shot
    for pts in darks:
        sh = curve(*[(x0 + x, y0 + y) for x, y in pts], w=1.1, cap="butt", smooth=1)
        L.append(K.lay(sh, K.sample_dark(key, sh, q=0.4), clip=inner))
    for pts in whites:
        wl = curve(*[(x0 + x, y0 + y) for x, y in pts], w=1.3, cap="round", smooth=2)
        L.append(K.lay(wl, _brightest(key, wl), clip=inner))
    return L, block


def _brightest(key, shape):
    rgb, m = C.sheet(key)
    b = K.bbox(shape)
    x0, y0, x1, y1, x, y = K.grid(b)
    d = sdf.sd({"t": "grow", "of": shape, "d": 0.6}, x, y)
    a = rgb[y0:y1, x0:x1][d < 0]
    Lm = fit.lum(a)
    return [round(float(v), 1) for v in np.median(a[Lm >= np.quantile(Lm, 0.8)], axis=0)]


def build_pad(el):
    k = el.split("shot-shape-")[1]
    scenes = {}
    for st, key, rct in C.CELLS[el]:
        e = pad_ellipse(key, rct)
        L, block = pad_layers(key, rct, e, SHOTS[k])
        x0, y0, w, h = rct
        scenes[st] = [K.item(L, box=[x0, y0, x0 + w, y0 + h])]
    return scenes


# ------------------------------------------------------------------------------------------------------------ pedestal
def build_pedestal(el):
    """A cylinder pedestal (E 0,0; 63 x 43): its top an ellipse with a navy line (fitted to the darkest ring), filled with
    the art's colours down its middle (lilac idle, green selected, dark disabled); its side running down to the cell's
    foot (x 6..57) in the side's own colour, a darker band at its right (its shadow side), on the lavender block."""
    scenes = {}
    for st, key, rct in C.CELLS[el]:
        x0, y0, w, h = rct
        rgb, m = C.sheet(key)
        a = rgb[y0:y0 + h, x0:x0 + w]
        dk = (fit.lum(a) < 70) & m[y0:y0 + h, x0:x0 + w]
        pts = fit.outermost(dk, (31.5, 20.5), rays=160)
        pts = pts[(pts[:, 0] < 56.5) & (pts[:, 1] < 37.5)]     # the side's dark right band and foot are not the rim
        cx, cy, rx, ry, _ = fit.fit_ellipse(pts, rot=0.0)
        cx, cy, rx, ry = cx + x0, cy + y0, rx + 0.4, ry + 0.4
        lav = K.sample(key, rect(x0 + 1, y0 + 2, x0 + 3, y0 + 6), depth=0.0)
        # the second row's cells show the side of the pedestal above running down behind this one's top
        above = K.sample(key, rect(int(cx) - 10, y0, int(cx) - 9, y0 + 2), depth=0.0)
        ys = y0 if np.abs(np.subtract(above, lav)).max() > 12 else cy
        side = K.sample(key, rect(int(cx) - 2, y0 + h - 3, int(cx) + 2, y0 + h - 1), depth=0.0)
        sh = K.sample(key, rect(min(int(cx + rx) + 1, x0 + w - 2), y0 + h - 4, min(int(cx + rx) + 3, x0 + w), y0 + h - 1), depth=0.0)
        L = [K.lay(rect(x0, y0, x0 + w, y0 + h), lav),
             K.lay(rect(cx - rx + 0.4, ys, cx + rx + 3.0, y0 + h), sh),
             K.lay(rect(cx - rx + 0.4, ys, cx + rx - 0.4, y0 + h), {"linear": {"a": [cx - rx, 0], "b": [cx + rx, 0], "stops": [
                 [t, K.sample(key, rect(int(cx - rx + 1 + t * (2 * rx - 3)), y0 + h - 3, int(cx - rx + 2 + t * (2 * rx - 3)), y0 + h - 1), depth=0.0)]
                 for t in np.linspace(0, 1, 6)]}}),
             K.lay(ellipse(cx, cy, rx, ry), K.sample_dark(key, outline(ellipse(cx, cy, rx - 0.5, ry - 0.5), 1.0), q=0.12))]
        st_ = []
        for t in np.linspace(0, 1, 8):
            yy = cy - ry + 1.5 + t * (2 * ry - 3)
            st_.append([float(t), K.sample(key, rect(int(cx) - 1, int(yy), int(cx) + 1, int(yy) + 1), depth=0.0)])
        L.append(K.lay(ellipse(cx, cy, rx - 1.2, ry - 1.2), {"linear": {"a": [0, cy - ry + 1.5], "b": [0, cy + ry - 1.5], "stops": st_}}))
        scenes[st] = [K.item(L, box=[x0, y0, x0 + w, y0 + h])]
    return scenes


EB.BUILD.update({f"shot-shape-{k}": build_pad for k in ORDER})
EB.BUILD["employee-pedestal"] = build_pedestal
