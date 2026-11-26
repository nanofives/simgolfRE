"""P41 round 6 (FIX6_BRIEF.md), amenities-palette: the sunken well redrawn to match the art's well. Everything else in the
approved scene (log/roadmap/P41/work2/hud_palettes/amenities-palette.json) is kept as it is: only the two layers of the
tray item named "deep recess..." and "light ledge under the recess" are replaced by the layers built here.

Read on the art (AmenitiesPanel.pcx, sheet coordinates, every edge measured row by row in this script):
  * the well's interior is a pure vertical gradient (the dither pattern and the row means are the same at every x):
    lum 120 at y 498 down to 78 at y 576;
  * its left wall is a soft ramp about 4 px wide (lum ~145 on the tray to the well's row value), its 50 % line runs from
    x 369.8 at y 498 to x 423.8 at y 576 (round 2's outline ran 1..3.4 px right of it);
  * a light double rim hugs the wall at the top left (a lum-165 line from (348, 498) to (395, 519) and a lum-180 line
    inside it from (355, 498) to (379, 506));
  * the tray between the building thumbnails and the wall is a light shoulder (lum 157 at the top to 130 at the bottom);
    round 2 painted it with the tray body's paint, 8..15 lum darker than the art, so next to the kept thumbnails (art
    pixels) the darker band read as part of the well: the well looked like it started at the thumbnails;
  * the right end is not flat: the well's right side is a lit lip that leaves the tray's right edge at y 504 and curves down
    and left to x 755.2 at y 560, then runs straight down to the ledge at y 577; the lip is 3..4 px of lum 223 at the top
    narrowing to 2 px of lum 192 at the bottom, with a 2 px fade (179, 172) to a flat light apron (160, 160, 208);
  * under the well a lit ledge (y 577..580, lum 202 at the left to 223 at the corner) and a floor whose tone rises from
    lum 133 at x 425 to the apron's 165 at x 755.
"""
import copy
import json
import os
import sys

import numpy as np
from PIL import Image, ImageDraw

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hp  # noqa: E402
import bubble as B  # noqa: E402
from hp import sdf  # noqa: E402

EL = "amenities-palette"
APPROVED = os.path.join(hp.ROOT, "log", "roadmap", "P41", "work2", "hud_palettes", EL + ".json")
OUT6 = os.path.join(hp.ROOT, "log", "roadmap", "P41", "work6", "hud_palettes")
LUMW = np.array([0.3, 0.59, 0.11])
TOP = 497.6          # the well's top edge (row 497 is a dither between the rail and the well, row 498 is the well)
BOT = 577.0          # the well's bottom edge (row 576 dark, row 577 the ledge)
RIGHT = 797.0        # the tray's darker right edge (chrome, x 797..800) is left alone
TABS_X = [471.0 + 61.0 * i for i in range(5)]   # the five amenity tab centres (els_trays.amenities_content), y about 577
TAB_Y = 577.0


def poly(pts, smooth=0):
    S = {"t": "poly", "rings": [[[float(x), float(y)] for x, y in pts]]}
    if smooth:
        S["smooth"] = smooth
    return S


def stroke(pts, w, cap="round"):
    return {"t": "stroke", "pts": [[float(x), float(y)] for x, y in pts], "w": float(w), "cap": cap}


def rgbl(c):
    return [round(float(v), 1) for v in c]


def far_from_tabs(X, Y, r=24.0):
    k = np.ones(X.shape, bool)
    for cx in TABS_X:
        k &= np.hypot(X - cx, Y - TAB_Y) > r
    return k


def profile_paint(rgb, keep, axis, lo, hi, n, a_other=0.0, win=4.0):
    """A linear paint along x (axis 0) or y (axis 1): per row / column means of the kept pixels (the art's two-colour
    dithers averaged), smoothed over +-win px, sampled at n stops from lo to hi (held flat past the first / last sample)."""
    jj, ii = np.nonzero(keep)
    v = (ii if axis == 0 else jj)
    t_all = np.unique(v)
    off = np.array([0, 0])
    med = np.array([rgb[jj[v == t], ii[v == t]].astype(float).mean(axis=0) for t in t_all])
    ts = t_all + 0.5 + (hp.crops(EL)[0][2] if axis == 0 else hp.crops(EL)[0][3])
    del off
    span = hi - lo
    stops = []
    for s in np.linspace(0.0, 1.0, n):
        p = lo + s * span
        wgt = np.clip(1.0 - np.abs(ts - p) / win, 0.0, None)
        if wgt.sum() < 1e-6:
            j = int(np.argmin(np.abs(ts - p)))
            c = med[j]
        else:
            c = (med * wgt[:, None]).sum(0) / wgt.sum()
        stops.append([round(float(s), 4), rgbl(c)])
    if axis == 0:
        a, b = [lo, a_other], [hi, a_other]
    else:
        a, b = [a_other, lo], [a_other, hi]
    return {"linear": {"a": a, "b": b, "stops": stops}}


def measure(rgb, mask, x0, y0):
    lum = rgb.astype(float) @ LUMW
    left, right = [], []
    for y in range(498, 577):
        r = lum[y - y0]
        well = float(np.median(r[480 - x0:700 - x0]))
        # left wall: walk left from inside the well to the first pixel at or above the mid value, interpolate the crossing
        mid = 0.5 * (well + 148.0)
        x = 440
        while x > 340 and r[x - x0] < mid:
            x -= 1
        vi, vj = r[x - x0], r[x + 1 - x0]
        left.append((x + 0.5 + (vi - mid) / max(1e-6, vi - vj), y + 0.5))
        if y >= 507:   # rows 504..506: the lip is still on the tray's right edge (x 797..800, chrome)
            mid = 0.5 * (well + 200.0)
            x = 735
            while x < 799 and r[x + 1 - x0] < mid:
                x += 1
            vi, vj = r[x - x0], r[x + 1 - x0]
            xe = x + 0.5 + (mid - vi) / max(1e-6, vj - vi)
            # the lip's core: from the edge to the 50 % point between the row's peak (within 6 px) and the apron's 165
            seg = r[x + 1 - x0:min(797, x + 7) - x0]
            thr = 0.5 * (float(seg.max()) + 165.0) if len(seg) else 190.0
            k = x + 1 + int(np.argmax(seg)) if len(seg) else x + 1
            while k < 797 and r[k - x0] >= thr:
                k += 1
            vk0, vk1 = r[k - 1 - x0], r[k - x0]
            xo = min(float(RIGHT), (k - 1) + 0.5 + (vk0 - thr) / max(1e-6, vk0 - vk1))
            right.append((xe, y + 0.5, xo))
    return left, right


def smooth_pts(pts, k=2):
    """x smoothed by a straight-line fit over +-k rows (unbiased at the ends, where the edge is steep)."""
    a = np.array(pts, float)
    out = a.copy()
    for i in range(len(a)):
        lo, hi = max(0, i - k), min(len(a), i + k + 1)
        c = np.polyfit(a[lo:hi, 1], a[lo:hi, 0], 1)
        out[i, 0] = np.polyval(c, a[i, 1])
    return [tuple(float(v) for v in p) for p in out]


def build_layers(rgb, mask, x0, y0):
    h, w = mask.shape
    X, Y = B.grid(x0, y0, w, h)
    lum = rgb.astype(float) @ LUMW
    left, right = measure(rgb, mask, x0, y0)
    left = smooth_pts(left)
    rs = smooth_pts([(p[0], p[1]) for p in right])
    ro = smooth_pts([(p[2], p[1]) for p in right])
    # every third row, plus the end points
    def thin(p):
        q = p[::3]
        return q if q[-1] == p[-1] else q + [p[-1]]
    L3, R3, O3 = thin(left), thin(rs), thin(ro)
    # the well's left edge, extended up to its top and down to its bottom
    lx_top = L3[0][0] - (L3[1][0] - L3[0][0]) / (L3[1][1] - L3[0][1]) * (L3[0][1] - TOP)
    ledge = [(lx_top, TOP)] + L3 + [(L3[-1][0], BOT)]
    # the right edge: the lip leaves the tray's right edge at y 504 (row 504 is the first lit pixel at x 799)
    redge = [(RIGHT, 504.3)] + R3 + [(R3[-1][0], BOT)]
    oedge = [(RIGHT, 504.3)] + O3 + [(O3[-1][0], BOT)]
    well_true = poly([(lx_top, TOP), (RIGHT, TOP)] + redge + [(ledge[-1][0], BOT)] + ledge[::-1][1:])
    well_ext = poly([(360.0, 470.0), (820.0, 470.0), (820.0, 600.0), (ledge[-1][0], 600.0)] + ledge[::-1])
    well_clip = poly([(330.0, TOP), (RIGHT, TOP)] + redge + [(330.0, BOT)])
    d_true = sdf.sd(well_true, X, Y)
    d_left = sdf.sd(well_ext, X, Y)

    layers = []
    # 1. the light shoulder between the thumbnails and the wall (its left side runs under the kept thumbnails; above them,
    #    x 330..358, it blends into the tray body over a 28 px ramp so no seam shows where round 2's tray paint goes on)
    keep_edge = [(392.0, 600.0), (392.0, 561.0), (380.0, 547.0), (366.0, 538.0), (359.5, 525.0), (334.0, 510.5)]
    sh_shape = poly([(330.0, TOP), (lx_top + 3.0, TOP)] + [(x + 3.0, y) for x, y in ledge[1:]] +
                    [(ledge[-1][0] + 3.0, 600.0), (392.0, 600.0)] + keep_edge[1:] + [(330.0, 513.3)])
    sh_px = (mask & (sdf.sd(sh_shape, X, Y) < -0.5) & (d_left > 2.5) & (lum > 118) & (lum < 162) & (X > 340)
             & (sdf.sd(poly(keep_edge + [(250.0, 600.0)]), X, Y) > 1.5)
             & ~((Y < 532) & (d_left > -9.0) & (d_left < 9.0)))
    sh_fill = profile_paint(rgb, sh_px, 1, TOP, 580.0, 9, a_other=400.0)
    layers.append({"shape": {"t": "half", "p": [344.0, 0.0], "n": [-1.0, 0.0]}, "fill": sh_fill, "soft": 14.0,
                   "clip": sh_shape, "name": "well: the light shoulder between the thumbnails and the wall (as the art)"})
    # 2. the floor in front of the well and the light apron round its right end
    fa_shape = poly([(ledge[-1][0], 578.0), (R3[-1][0] + 2.0, 578.0), (R3[-1][0] + 2.0, 556.0), (762.0, 540.0), (772.0, 524.0),
                     (790.0, 504.0), (RIGHT, 500.0), (RIGHT, 600.0), (ledge[-1][0], 600.0)])
    out_lip = poly([(RIGHT, 504.3)] + [(x + 2.6, y) for x, y in O3] + [(O3[-1][0] + 2.6, 580.5), (RIGHT, 580.5)])
    fl_px = (mask & ((Y > 580.5) | (sdf.sd(out_lip, X, Y) < -0.5)) & (X > 395) & (X < RIGHT - 0.5)
             & far_from_tabs(X, Y) & (lum > 118))
    fa_fill = profile_paint(rgb, fl_px, 0, 392.0, RIGHT, 14, a_other=590.0, win=8.0)
    layers.append({"shape": fa_shape, "fill": fa_fill, "name": "well: the floor in front of it and the light apron round its right end"})
    # 3. the light band hugging the top of the left wall (its outer edge read on the art's lum-165 line: x 350 at y 498,
    #    370 at 503, 384 at 510, 391 at 515, 395 at 518; brightest at the top corner, gone into the shoulder by y 531)
    outer = [(348.0, TOP), (350.0, 498.5), (358.0, 499.5), (365.0, 501.5), (371.0, 503.5), (377.0, 506.5), (383.0, 509.5),
             (388.0, 512.5), (392.0, 515.5), (396.0, 519.0), (400.0, 523.0), (404.5, 528.0), (408.6, 531.5)]
    inner = [(x + 1.0, y) for x, y in ledge if y <= 531.5]
    band = poly(outer + inner[::-1] + [(348.0, TOP)])
    layers.append({"shape": band, "soft": 0.9, "clip": {"t": "half", "p": [0.0, TOP], "n": [0.0, -1.0]},
                   "fill": {"linear": {"a": [0.0, TOP], "b": [0.0, 531.5],
                                       "stops": [[0.0, [186.0, 186.0, 212.0]], [0.25, [174.0, 174.0, 208.0]],
                                                 [0.5, [165.0, 165.0, 207.0]], [0.75, [157.0, 157.0, 204.0]],
                                                 [1.0, sh_fill["linear"]["stops"][4][1]]]}},
                   "name": "well: the light band hugging the top of the left wall"})
    # 4. the well itself: a vertical gradient measured row by row; its left wall a 4 px soft ramp, top and right crisp
    in_px = mask & (d_true < -3.0) & far_from_tabs(X, Y, 23.0) & (lum < 130) & (lum > 60)
    well_fill = profile_paint(rgb, in_px, 1, TOP, BOT, 9, a_other=600.0)
    layers.append({"shape": well_ext, "fill": well_fill, "soft": 1.8, "clip": well_clip,
                   "name": "deep recess (round 6: the art's outline, a soft left wall, its right end a curved lit lip)"})
    # 5. the lip round the right end: its fade, then its core
    lip_px = mask & (X >= np.interp(Y, [p[1] for p in rs], [p[0] for p in rs]) + 0.3) & \
        (X <= np.interp(Y, [p[1] for p in ro], [p[0] for p in ro]) - 0.3) & (Y > 505) & (Y < BOT) & (lum >= 186)
    fade = poly([(RIGHT, 504.3)] + [(x + 2.2, y) for x, y in O3] + [(O3[-1][0] + 2.2, BOT)] + redge[::-1][1:])
    layers.append({"shape": fade, "fill": [176.0, 176.0, 208.0], "soft": 0.9, "clip": {"t": "half", "p": [RIGHT, 0.0], "n": [1.0, 0.0]},
                   "name": "well: the lip's outer fade into the apron"})
    core = poly(oedge + redge[::-1])
    layers.append({"shape": core, "fill": B.measure_linear(rgb, lip_px, x0, y0),
                   "clip": {"t": "half", "p": [RIGHT, 0.0], "n": [1.0, 0.0]},
                   "name": "well: the lit lip round its right end (leaves the right edge at y 504, curves down to the ledge)"})
    # 6. the lit ledge under the well, from the wall to the lip's foot
    lx0 = ledge[-1][0] - 0.3
    lg = poly([(lx0, BOT), (O3[-1][0], BOT), (O3[-1][0], 580.0), (lx0, 580.0)])
    lg_px = mask & (Y > BOT) & (Y < 580.0) & (X > lx0 + 1) & (X < O3[-1][0] - 0.5) & far_from_tabs(X, Y, 21.0) & (lum > 170)
    layers.append({"shape": lg, "fill": B.measure_linear(rgb, lg_px, x0, y0), "soft": 0.45,
                   "name": "light ledge under the recess (round 6: from the wall to the lip's foot, brighter toward the corner)"})
    return layers, dict(left=L3, right=R3, outer=O3)


def render(scene, st, z):
    _, src, x0, y0, rgb, mask = st
    h, w = mask.shape
    return hp.render_state(scene, x0, y0, w, h, z, art=hp.art_on_flat(rgb, mask))


def triple(st, scenes, z, box=None, ticks=False):
    _, src, x0, y0, rgb, mask = st
    h, w = mask.shape
    bx0, by0, bx1, by1 = box or (x0, y0, x0 + w, y0 + h)
    by0, by1 = max(by0, y0), min(by1, y0 + h)
    bx0, bx1 = max(bx0, x0), min(bx1, x0 + w)
    sl = (slice(by0 - y0, by1 - y0), slice(bx0 - x0, bx1 - x0))
    a0 = hp.art_on_flat(rgb, mask)[sl]
    bw, bh = bx1 - bx0, by1 - by0
    art = np.asarray(Image.fromarray(a0.astype(np.uint8)).resize((bw * z, bh * z), Image.NEAREST), float)
    cols = [art] + [hp.render_state(s, bx0, by0, bw, bh, z, art=a0) for s in scenes]
    H, W = cols[0].shape[:2]
    out = np.zeros((H + 14, 3 * W + 16, 3)) + 30
    for i, c in enumerate(cols):
        out[14:, i * (W + 8):i * (W + 8) + W] = c
    im = Image.fromarray(np.clip(out + 0.5, 0, 255).astype(np.uint8))
    d = ImageDraw.Draw(im)
    for i, t in enumerate(("art", "approved (round 2)", "round 6")):
        d.text((i * (W + 8) + 4, 1), t, fill=(255, 255, 0))
    return im


def main():
    os.makedirs(OUT6, exist_ok=True)
    doc = json.load(open(APPROVED))
    st = hp.crops(EL)[0]
    _, src, x0, y0, rgb, mask = st
    old = doc["states"]["idle"]["scene"]
    new = copy.deepcopy(old)
    layers, info = build_layers(rgb, mask, x0, y0)
    tray = new[0]["layers"]
    i0 = [i for i, L in enumerate(tray) if str(L.get("name", "")).startswith("deep recess")]
    i1 = [i for i, L in enumerate(tray) if str(L.get("name", "")).startswith("light ledge under the recess")]
    assert len(i0) == 1 and len(i1) == 1 and i1[0] == i0[0] + 1, (i0, i1)
    new[0]["layers"] = tray[:i0[0]] + layers + tray[i1[0] + 1:]
    doc["states"]["idle"]["scene"] = new
    doc["how"] = (doc.get("how", "") + "; round 6: the well matched to the art's (its outline re-read row by row, a soft left "
                  "wall with the light double rim at its top, the light shoulder between the thumbnails and the wall, the "
                  "right end a curved lit lip over a light apron instead of a flat end, the ledge ending at the lip's foot)")
    with open(os.path.join(OUT6, EL + ".json"), "w") as f:
        json.dump(doc, f, default=lambda o: float(o) if isinstance(o, np.floating) else int(o))
    # scores (mean abs RGB over the cut, edge disagreement) for the approved and the new scene
    for nm, sc in (("approved", old), ("round 6", new)):
        r = render(sc, st, 4).reshape(mask.shape[0], 4, mask.shape[1], 4, 3).mean((1, 3))
        dd = np.abs(r - rgb.astype(float)).mean(-1)
        X, Y = B.grid(x0, y0, mask.shape[1], mask.shape[0])
        wellzone = mask & (X > 340) & (Y < 600)
        print(f"  {nm}: mad {dd[mask].mean():.2f}  mad x>340 {dd[wellzone].mean():.2f}")
    triple(st, [old, new], 2).save(os.path.join(OUT6, EL + ".png"))
    triple(st, [old, new], 8, box=(330, 490, 440, 600)).save(os.path.join(OUT6, EL + "_left_8x.png"))
    triple(st, [old, new], 8, box=(700, 490, 800, 600)).save(os.path.join(OUT6, EL + "_right_8x.png"))
    print("  left edge", [(round(x, 1), y) for x, y in info["left"]][:6], "...")
    print("  right edge", [(round(x, 1), y) for x, y in info["right"]][:6], "...")
    return doc


if __name__ == "__main__":
    main()
