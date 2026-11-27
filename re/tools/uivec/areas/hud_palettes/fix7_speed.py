"""P41 round 7 item 11 (log/roadmap/P41/FIX7_BRIEF.md): "el boton de velocidades ahora quedo pixelado porque no esta renderizado
como vectores".

The game-speed button is the port's own (P33A, log/roadmap/P33/P33A_notes.md; port/src/sim/gfx_hud.cpp speed_button): no sheet
carries it. The port makes it at 1x from the pause button's pixels in the 800 x 600 root (the 27 x 27 box at (94, 554)), scaled by
0.76 to a disc of radius 9.12 centred on pixel (105, 590) (edge coordinates (105.5, 590.5): the code samples at px - (105 + 0.5)),
its bars wiped by interpolating each row's gold ends, and 1 / 2 / 3 right-pointing triangles in the bars' navy (speed 1 / 2 / 4):
height 9, each 7 / 5.5 / 4.3 wide, side by side, the row's left edge at 105 - total / 2 + 0.7 (one) or + 0.3 (two, three), plus
the 0.5 of the pixel-centre convention. It has no hover look (the hover only shows a tip after a dwell).

What this draws (screen coordinates of the 800 x 600 root, pixel (i, j) covering [i, i + 1) x [j, j + 1); there is no sheet):
the SAME structure as its neighbour the pause button (hud-mode-base item 9, approved): a navy rim 1 px wide ([40, 48, 80]),
a glass body lit by the pause body's measured bevel (its stops scaled to the smaller radius through the bevel's L), the pause's
measured cast shadow scaled with the disc, and the navy glyph in the pause bars' navy ([32, 40, 80]). The disc keeps the port's
place and size (centre (105.5, 590.5), outer radius 9.12 = 12 x 0.76, the radius the hit test uses), so it still clears the
pause's rim above (bottom 580.98, top of this disc 581.38) and the frame's bottom row (599.62).
The triangles keep the port's pixel-grid numbers (heights, widths, optical shift) and get the pause bars' soft corners: each is
the triangle inset by 0.6 px about its incentre and grown back by 0.6 px (round corners of radius 0.6, the outline unchanged
elsewhere).
States: hud-speed-1x / -2x / -4x (idle: the pause's tan light), and -1x-hover / -2x-hover / -4x-hover (the pause's measured
yellow hover light) offered for a hover look the port does not draw today. Only the paint differs between states and the glyph
between speed levels; disc, rim, shadow and glyph places are identical.
"""
import json
import os
import sys

import numpy as np
from PIL import Image, ImageDraw

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hp  # noqa: E402
import geom as G  # noqa: E402
from hp import sdf  # noqa: E402
import fix7_pause as FP  # noqa: E402

OUT7 = FP.OUT7
SHEET = FP.SHEET
CX, CY = 105.5, 590.5            # the port's disc centre (gfx_hud.cpp kSpeedX / kSpeedY + 0.5)
R = 12.0 * 0.76                  # kPauseR * kSpeedScale
RIM_W = 1.0                      # the pause's rim: 13.14 - 12.14
TH = 9.0                         # triangle height (port)
TW = {1: 7.0, 2: 5.5, 3: 4.3}    # triangle width per count (port)
SHIFT = {1: 0.7, 2: 0.3, 3: 0.3}  # optical shift right (port)
ROUND = 0.6
LEVELS = (("1x", 1), ("2x", 2), ("4x", 3))
WIN = (84, 546, 52, 54)          # comparison window (x, y, w, h), screen


# ------------------------------------------------------------------------------------------------ the port's 1x button, in Python
def port_1x(n, bg):
    """gfx_hud.cpp speed_button on the tray art (5-bit channels as the root holds them); bg: the art window (float RGB 0..255)
    of WIN. Returns the window with the button drawn, 0..255."""
    rgb, mask = hp.BR.sheet(SHEET)
    q = lambda a: np.floor(a.astype(float) / 255.0 * 31.0 + 0.5)            # 8-bit -> 5-bit
    box = q(rgb[554:581, 94:121])
    K, c0, fr = 27, 13.0, 9.5
    gold = lambda c: c[0] >= 23 and c[0] - c[2] >= 8
    face = box.copy()
    for j in range(K):
        ins = [i for i in range(K) if (i - c0) ** 2 + (j - c0) ** 2 <= fr * fr]
        if not ins:
            continue
        lo, hi = ins[0], ins[-1]
        i = lo
        while i <= hi:
            if gold(face[j, i]):
                i += 1
                continue
            e = i
            while e <= hi and not gold(face[j, e]):
                e += 1
            hl, hr = i > lo, e <= hi
            L = face[j, i - 1] if hl else face[j, min(e, hi)]
            Rr = face[j, e] if hr else L
            for k in range(i, e):
                t = (k - i + 1) / (e - i + 1)
                face[j, k] = L + (Rr - L) * t
            i = e
    navy = box[567 - 554, 102 - 94]
    tw = TW[n]
    tot = tw * n
    tx0 = 105.0 - tot / 2 + SHIFT[n]

    def in_tri(x, y):
        if y < 590 - TH / 2 or y > 590 + TH / 2 or x < tx0 or x >= tx0 + tot:
            return False
        lx = np.fmod(x - tx0, tw)
        return abs(y - 590) <= (TH / 2) * (1.0 - lx / tw)

    def sample(sx, sy):
        sx, sy = min(max(sx, 0.0), K - 1.0), min(max(sy, 0.0), K - 1.0)
        x0, y0 = min(int(sx), K - 2), min(int(sy), K - 2)
        fx, fy = sx - x0, sy - y0
        return (face[y0, x0] * (1 - fx) + face[y0, x0 + 1] * fx) * (1 - fy) + (face[y0 + 1, x0] * (1 - fx) + face[y0 + 1, x0 + 1] * fx) * fy

    out = q(np.clip(bg, 0, 255))
    wx, wy = WIN[0], WIN[1]
    rt = R
    for y in range(int(590 - rt) - 1, int(590 + rt) + 2):
        for x in range(int(105 - rt) - 1, int(105 + rt) + 2):
            cover = tri = 0
            for sj in range(4):
                for si in range(4):
                    px, py = x + (si + 0.5) / 4, y + (sj + 0.5) / 4
                    dx, dy = px - 105.5, py - 590.5
                    if dx * dx + dy * dy > rt * rt:
                        continue
                    cover += 1
                    tri += in_tri(px - 0.5, py - 0.5)
            if cover == 0:
                continue
            f = sample(c0 + (x - 105) / 0.76, c0 + (y - 590) / 0.76)
            ta, ca = tri / 16, cover / 16
            top = f + (navy - f) * ta / ca
            u = out[y - wy, x - wx]
            out[y - wy, x - wx] = np.clip(np.floor(u + (top - u) * ca + 0.5), 0, 31)
    return out * 255.0 / 31.0


# ------------------------------------------------------------------------------------------------------------- the vector button
def tri_shape(x0, w, cy, h, r):
    """A right-pointing triangle (back edge x0, tip x0 + w, height h, centred on cy) with corners rounded by r."""
    A, B, C = np.array([x0, cy - h / 2]), np.array([x0 + w, cy]), np.array([x0, cy + h / 2])
    a, b, c = np.linalg.norm(B - C), np.linalg.norm(A - C), np.linalg.norm(A - B)
    inc = (a * A + b * B + c * C) / (a + b + c)
    s = (a + b + c) / 2
    rin = np.sqrt(s * (s - a) * (s - b) * (s - c)) / s
    k = (rin - r) / rin
    P = [inc + (p - inc) * k for p in (A, B, C)]
    return G.grow(G.poly(*[(round(float(p[0]), 4), round(float(p[1]), 4)) for p in P]), r)


def glyph(n):
    tw = TW[n]
    tot = tw * n
    x0 = 105.0 - tot / 2 + SHIFT[n] + 0.5      # the port's convention: in_tri(px - 0.5, ...)
    return [tri_shape(x0 + i * tw, tw, CY, TH, ROUND) for i in range(n)]


def scene(n, hover):
    tray = FP.tray_pause()
    shadow_t, rim_t, body_t, lb_t = tray["layers"][0], tray["layers"][1], tray["layers"][2], tray["layers"][3]
    pc = rim_t["shape"]["c"]
    k = R / rim_t["shape"]["r"]
    rb = R - RIM_W
    kb = rb / body_t["shape"]["r"]
    body = G.circle(CX, CY, rb)
    if hover:
        fill = json.loads(json.dumps(FP.load("hud-pause")["states"]["hover"]["scene"][0]["layers"][3]["fill"]))
    else:
        fill = json.loads(json.dumps(body_t["fill"]))
    fill["bevel"]["L"] = round(kb * fill["bevel"].get("L", 1.0), 5)
    fill["bevel"]["of"] = body
    sh = shadow_t["shape"]
    shadow = {"shape": G.circle(CX + (sh["c"][0] - pc[0]) * k, CY + (sh["c"][1] - pc[1]) * k, sh["r"] * k),
              "fill": shadow_t["fill"], "alpha": shadow_t.get("alpha", 0.4), "soft": round(shadow_t.get("soft", 1.3) * k, 4),
              "name": "the cast shadow: the pause button's measured shadow, scaled with the disc (offset, radius, softness)"}
    layers = [shadow,
              {"shape": G.circle(CX, CY, R), "fill": rim_t["fill"],
               "name": f"rim: the pause's navy rim, 1 px wide (outer radius {R:.2f}, the port's disc)"},
              {"shape": body, "fill": fill,
               "name": ("body: the pause's measured " + ("yellow hover" if hover else "tan") + " glass light, its stops scaled "
                        f"to radius {rb:.2f} (bevel L {fill['bevel']['L']})")}]
    for i, t in enumerate(glyph(n)):
        layers.append({"shape": t, "fill": lb_t["fill"], "clip": body,
                       "name": f"triangle {i + 1} of {n}: pointing right, {TH:g} tall, {TW[n]:g} wide, corners rounded "
                               f"{ROUND:g} px (the pause bars' navy)"})
    box = [int(np.floor(CX - R - 3)), int(np.floor(CY - R - 3)), int(np.ceil(CX + R + 3)), int(np.ceil(CY + R + 3))]
    return [{"box": box, "layers": layers}]


def render(items, z, base):
    x, y, w, h = WIN
    img = np.empty((h * z, w * z, 3))
    img[:] = hp.FLAT
    sdf.render(base + items, img, z, origin=(x, y))
    return img


def label(img, t):
    im = Image.fromarray(np.clip(img + 0.5, 0, 255).astype(np.uint8))
    d = ImageDraw.Draw(im)
    d.rectangle([0, 0, 7 * len(t) + 6, 13], fill=(30, 30, 30))
    d.text((3, 1), t, fill=(255, 255, 0))
    return np.asarray(im, float)


def main():
    os.makedirs(OUT7, exist_ok=True)
    z = 8
    x, y, w, h = WIN
    rgb, mask = hp.BR.sheet(SHEET)
    art = rgb[y:y + h, x:x + w].astype(float).copy()
    art[~mask[y:y + h, x:x + w]] = hp.FLAT
    base = [it for it in FP.load("hud-mode-base")["states"]["idle"]["scene"] if "layers" in it]
    rows, check = [], {}
    for nm, n in LEVELS:
        cols = []
        one = port_1x(n, art)
        cols.append(label(FP.up(one, z), f"port now {nm} (1x)"))
        for hov in (False, True):
            sc = scene(n, hov)
            eid = f"hud-speed-{nm}" + ("-hover" if hov else "")
            d = {"id": eid, "coords": "screen pixels of the 800 x 600 root (no sheet: the port's own button, gfx_hud.cpp "
                                      "speed_button; pixel (i, j) covers [i, i+1) x [j, j+1))",
                 "centre": [CX, CY], "radius": R, "speed": {1: 1, 2: 2, 3: 4}[n], "state": "hover" if hov else "idle",
                 "scene": sc}
            with open(os.path.join(OUT7, eid + ".json"), "w", encoding="utf-8") as f:
                json.dump(d, f)
            cols.append(label(render(sc, z, base), eid))
            # checks: the disc's geometry per state, and the glyph's box against the disc centre
            gx = []
            for L in sc[0]["layers"][3:]:
                pts = np.array(L["shape"]["of"]["rings"][0])
                gx += [pts[:, 0].min() - ROUND, pts[:, 0].max() + ROUND]
            gy0 = CY - TH / 2
            check[eid] = {"rim": [CX, CY, round(R, 3)], "body r": round(R - RIM_W, 3),
                          "glyph box x": [round(min(gx), 2), round(max(gx), 2)],
                          "glyph box centre - disc centre (x, y)": [round((min(gx) + max(gx)) / 2 - CX, 2), round(gy0 + TH / 2 - CY, 2)]}
        gap = np.zeros((cols[0].shape[0], 8, 3)) + 30
        rows.append(np.concatenate([cols[0], gap, cols[1], gap, cols[2]], 1))
    sep = np.zeros((8, rows[0].shape[1], 3)) + 30
    big = np.concatenate(sum(([r, sep] for r in rows), [])[:-1])
    Image.fromarray(np.clip(big, 0, 255).astype(np.uint8)).save(os.path.join(OUT7, "hud-speed.png"))
    # overlay of the six states: alpha coverage of rim and body identical? (render each state's disc mask at 8x and compare)
    masks = {}
    jj, ii = np.mgrid[0:h * z, 0:w * z]
    X, Y = (ii + 0.5) / z + x, (jj + 0.5) / z + y
    for nm, n in LEVELS:
        for hov in (False, True):
            sc = scene(n, hov)[0]["layers"]
            masks[(nm, hov)] = [np.clip(0.5 - sdf.sd(L["shape"], X, Y) * z, 0, 1) for L in sc[:3]]
    ref = masks[("1x", False)]
    check["geometry identical across all six states (rim, body, shadow coverage)"] = bool(all(
        all(np.array_equal(a, b) for a, b in zip(ref, m)) for m in masks.values()))
    with open(os.path.join(OUT7, "hud-speed_check.json"), "w") as f:
        json.dump(check, f, indent=1)
    print(json.dumps(check, indent=1))
    # the speed button among its neighbours at 1x .. 4x scale, for the look in place (art tray vs vector tray)
    zs = []
    for nm, n in LEVELS:
        zs.append(label(render(scene(n, False), 4, base), f"{nm} in place, 4x"))
    gap = np.zeros((zs[0].shape[0], 8, 3)) + 30
    Image.fromarray(np.clip(np.concatenate([zs[0], gap, zs[1], gap, zs[2]], 1), 0, 255).astype(np.uint8)).save(
        os.path.join(OUT7, "hud-speed_inplace_4x.png"))


if __name__ == "__main__":
    main()
