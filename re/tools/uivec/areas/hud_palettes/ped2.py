"""Elevation palette pedestals, fix round (2026-11-24): Mariano's note on all four was "render actual shadows of the elements".

Round 1 drew the cylinder's shadow on the wall as a hard 7 px rectangle and left the shadow the picture (the two yellow
arrows, the ball's trails) casts on the wall inside the kept-art region, so it showed as a pasted patch with straight edges.
This round every shadow is its own layer: the CASTER's authored silhouette (the cylinder; the arrows or trails), moved by a
measured offset, with a measured soft edge and strength, clipped to the surface it falls on:
  1. the cylinder's shadow on the back wall (to its right),
  2. the picture's shadow on the back wall,
  3. the top lip's shadow on the cylinder just under the lip,
  4. the picture's shadow on the pedestal top (its rim, behind the arrows).
The offsets, softness and strength are fitted to the art per state (coordinate descent on the pixels of the receiving
surface, the same method as the bubbles' cast shadow in bubble.fit_shadow). The kept picture is now only the picture itself
(the arrows / trails / ball by their authored silhouettes, the mound or grass disc), so the wall between and beside them is
drawn, not pasted. Pressed states (selected, disabled, hover) sit 1 px lower (the top's outline bottom is at 73, not 72).
Local coordinates of each cut (sheet = local + cut origin).
"""
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hp  # noqa: E402
import bubble as B  # noqa: E402
import geom as G  # noqa: E402
from geom import rect, line, poly, oval, curve, U  # noqa: E402
from hp import sdf, fit  # noqa: E402

STOPS = [0.0, 0.8, 1.6, 2.6, 4.0, 6.0, 10.0]
SHADOW_RGB = [34, 32, 78]

# the two arrows on point / square / area (read on elevation-point's idle cut, yellow pixels: rows 2..27)
UP_ARROW = U(poly((34.6, 1.6), (36.2, 4.5), (39.4, 9.5), (41.3, 13.0), (41.2, 16.4), (39.4, 18.2), (30.6, 18.2), (28.9, 16.4),
                  (28.9, 13.0), (30.8, 9.5), (33.0, 4.5), smooth=1),
             rect(34.0, 17.5, 37.0, 21.5),
             poly((32.4, 21.3), (38.6, 21.3), (38.3, 27.4), (37.2, 28.1), (33.8, 28.1), (32.4, 27.0), smooth=1))
DOWN_ARROW = U(rect(46.0, 2.0, 52.2, 7.2, 1.2),
               rect(47.0, 6.5, 51.3, 11.5),
               poly((44.6, 11.0), (48.5, 11.0), (50.5, 11.2), (54.6, 12.0), (55.2, 14.2), (54.8, 16.6), (53.4, 19.0), (51.6, 23.2),
                    (49.8, 27.2), (49.0, 27.2), (47.4, 23.2), (45.6, 19.4), (43.4, 16.8), (43.2, 13.6), smooth=1))
ARROWS = U(UP_ARROW, DOWN_ARROW)

# smooth: the ball and its three trails, each a curved band ending in a head (read on elevation-smooth's idle cut)
BALL = oval(19.0, 48.4, 4.6, 4.3)
TRAILS = U(curve((22.5, 44.5), (29.0, 27.0), (52.0, 25.0), 3.2, 3.6),
           poly((51.0, 21.6), (60.0, 23.6), (62.0, 24.6), (60.0, 25.8), (51.0, 28.6), smooth=1),
           curve((26.6, 45.2), (34.0, 32.0), (57.0, 31.6), 2.6, 3.0),
           poly((55.6, 28.6), (64.0, 30.4), (67.4, 32.0), (64.0, 33.4), (55.6, 35.0), smooth=1),
           curve((30.0, 47.4), (38.6, 37.0), (61.0, 38.4), 3.4, 4.0),
           poly((60.0, 34.8), (68.0, 37.8), (71.6, 40.6), (68.0, 42.4), (60.0, 43.2), smooth=1))


def caster_zone(c, sweep=((0, 0), (3, 1), (6, 2), (9, 3), (12, 4), (15, 5)), grow=1.5):
    """Where a caster's shadow can fall to its lower right (kept out of the paint measurements)."""
    return G.grow(U(*[G.moved(c, dx, dy) for dx, dy in sweep]), grow)


def fit_cast(rgb, recv, x0, y0, base_items, layer_fn, init, steps, lo, hi, z=3, rounds=7, grid=None):
    """Coordinate descent of a cast shadow's parameters on the receiving pixels (recv); base_items: what lies under it.
    grid: [(dx, dy), ...] offsets tried first (with init's softness and strength), the best one starts the descent."""
    h, w = recv.shape
    base = hp.render_state(base_items, x0, y0, w, h, z)

    def loss(p):
        img = base.copy()
        sdf.render([{"box": [x0, y0, x0 + w, y0 + h], "layers": [layer_fn(p)]}], img, z, origin=(x0, y0))
        img = img.reshape(h, z, w, z, 3).mean((1, 3))
        return float(np.abs(img - rgb)[recv].mean())

    p, st = list(init), list(steps)
    best = loss(p)
    for gx, gy in grid or ():
        q = [gx, gy] + list(init[2:])
        lq = loss(q)
        if lq < best:
            best, p = lq, q
    for _ in range(rounds):
        for k in range(len(p)):
            for sgn in (1, -1):
                q = list(p)
                q[k] = min(max(q[k] + sgn * st[k], lo[k]), hi[k])
                lq = loss(q)
                if lq < best:
                    best, p = lq, q
        st = [s * 0.55 for s in st]
    return p, best


def shadow_layer(shape, name, clip=None):
    def fn(p):
        L = {"shape": G.moved(shape, p[0], p[1]), "fill": SHADOW_RGB, "alpha": p[3], "soft": max(p[2], 0.3), "name": name}
        if clip is not None:
            L["clip"] = clip
        return L
    return fn


def build(spec):
    el = spec["id"]
    cx, cy, rx, ry = spec["ellipse"]
    scenes, info = {}, {}
    for st, src, x0, y0, rgb, mask in hp.crops(el):
        h, w = mask.shape
        rgbf = rgb.astype(float)
        X, Y = B.grid(x0, y0, w, h)
        po = spec["pic_off"].get(st, (0, 0))          # the picture's shift on this cut
        pd = 0.0 if st == "idle" else 1.0              # pressed states sit 1 px lower
        mv = lambda s, d=(0.0, pd): G.moved(s, x0 + d[0], y0 + d[1])  # noqa: E731
        top = mv(oval(cx, cy, rx, ry))
        cyl_body = mv(rect(cx - rx, cy, cx + rx, h + 4))
        cyl_sil = U(cyl_body, top)
        caster = G.moved(spec["caster"], x0 + po[0], y0 + po[1])
        keep = U(G.grow(caster, 0.5), mv(spec["keep_inner"]))
        zone = caster_zone(caster)
        d_sil = sdf.sd(cyl_sil, X, Y)
        d_keep = sdf.sd(keep, X, Y)
        d_zone = sdf.sd(zone, X, Y)
        wall_px = mask & (d_sil > 1.2) & (d_keep > 1.0)
        # 1. back wall: a linear paint measured on wall pixels clear of every shadow (left of the cylinder's middle, or high up)
        clean = wall_px & (d_zone > 0.5) & ((X - x0 < cx - 0.2 * rx) | (Y - y0 < 3))
        wall = {"shape": mv(rect(-2, -2, w + 2, h + 4), (0, 0)), "fill": B.measure_linear(rgb, clean, x0, y0), "name": "back wall"}
        # 2. the cylinder's shadow on the wall, to its right
        recv = wall_px & (X - x0 > cx + 0.3 * rx) & (Y - y0 > cy - pd) & (d_zone > 0.5)
        fn_c = shadow_layer(cyl_sil, "the cylinder's cast shadow on the back wall (offset, softness, strength measured)")
        pc, lc = fit_cast(rgbf, recv, x0, y0, [{"box": [x0, y0, x0 + w, y0 + h], "layers": [wall]}], fn_c,
                          init=(3.0, -1.0, 3.0, 0.3), steps=(2.0, 2.0, 2.0, 0.15), lo=(0, -8, 0.3, 0), hi=(12, 6, 10, 0.9))
        Lc = fn_c(pc)
        # 3. the picture's shadow on the wall
        recv2 = wall_px & (d_zone < 0.0)
        fn_p = shadow_layer(caster, "the picture's cast shadow on the back wall (offset, softness, strength measured)",
                            clip=G.sub(mv(rect(-2, -2, w + 2, h + 4), (0, 0)), cyl_sil))
        pp, lp = fit_cast(rgbf, recv2, x0, y0, [{"box": [x0, y0, x0 + w, y0 + h], "layers": [wall, Lc]}], fn_p,
                          init=(6.0, 3.0, 4.5, 0.35), steps=(1.5, 1.5, 1.5, 0.15), lo=(-2, -4, 4.0, 0), hi=(20, 10, 8, 0.9),
                          grid=[(gx, gy) for gx in range(0, 21, 2) for gy in range(-4, 11, 2)])
        Lp = fn_p(pp)
        # 4. the cylinder: horizontal shading measured across it (5 stops), its dark edge line on the right
        cyl_px = mask & (sdf.sd(cyl_body, X, Y) < -0.6) & (sdf.sd(top, X, Y) > 2.5) & (X - x0 < cx + rx - 1.6)
        cyl_fill = B.measure_linear(rgb, cyl_px & (Y - y0 > cy + ry + 5 + pd), x0, y0, n=5)
        Lcyl = {"shape": cyl_body, "fill": cyl_fill, "name": "cylinder body (shading measured across it)"}
        edge = mv(line((cx + rx - 0.6, cy), (cx + rx - 0.6, h + 4), w=1.2))
        ek = mask & (sdf.sd(edge, X, Y) < -0.1) & (Y - y0 > cy + ry + pd)
        Le = {"shape": edge, "fill": fel_mode(rgb[ek]) if ek.sum() else [60, 60, 100], "name": "cylinder: dark edge line on its right"}
        # 5. the top lip's shadow on the cylinder, just under the lip
        recv3 = cyl_px & (Y - y0 < cy + ry + 8 + pd)
        fn_l = shadow_layer(top, "the top lip's shadow on the cylinder under it (offset, softness, strength measured)", clip=cyl_body)
        base3 = [{"box": [x0, y0, x0 + w, y0 + h], "layers": [wall, Lc, Lp, Lcyl, Le]}]
        pl, ll = fit_cast(rgbf, recv3, x0, y0, base3, fn_l, init=(0.5, 1.5, 1.5, 0.3), steps=(1.0, 1.0, 1.0, 0.15),
                          lo=(-3, 0, 0.3, 0), hi=(4, 6, 5, 0.9))
        Ll = fn_l(pl)
        # 6. the top: navy outline and rim, state colour, one measured bevel (the shadow zone and the picture left out)
        own = mask & (sdf.sd(top, X, Y) < -0.05) & (d_keep > 0.8) & (d_zone > 0.0)
        tp = B.measure_bevel_shape(rgb, own, x0, y0, top, STOPS)
        tp["bevel"]["of"] = top
        Lt = {"shape": top, "fill": tp, "name": "top: an ellipse, navy outline and rim in its measured bevel"}
        # 7. the picture's shadow on the top (on the rim behind the arrows / beside the trails)
        recv4 = mask & (sdf.sd(top, X, Y) < -0.6) & (d_keep > 0.8) & (d_zone < 0.0)
        fn_t = shadow_layer(caster, "the picture's cast shadow on the pedestal top (offset, softness, strength measured)", clip=top)
        base4 = base3[:1]
        base4 = [{"box": base3[0]["box"], "layers": base3[0]["layers"] + [Ll, Lt]}]
        if recv4.sum() > 8:
            pt, lt = fit_cast(rgbf, recv4, x0, y0, base4, fn_t, init=(2.0, 3.0, 1.5, 0.4), steps=(1.0, 1.0, 1.0, 0.15),
                              lo=(-3, -2, 1.5, 0), hi=(16, 12, 6, 0.95),
                              grid=[(gx, gy) for gx in range(0, 17, 2) for gy in range(0, 13, 2)])
            Lt2 = [fn_t(pt)]
        else:
            pt, Lt2 = None, []
        layers = [wall, Lc, Lp, Lcyl, Le, Ll, Lt] + Lt2
        scenes[st] = [{"box": [x0 - 2, y0 - 2, x0 + w + 2, y0 + h + 2], "layers": layers},
                      {"keep_art": keep, "name": "picture on the top, kept as art: " + spec["pic"]}]
        r4 = lambda p: None if p is None else [round(float(v), 2) for v in p]  # noqa: E731
        info[st] = {"cyl_shadow": r4(pc), "pic_shadow_wall": r4(pp), "lip_shadow": r4(pl), "pic_shadow_top": r4(pt),
                    "params": "dx, dy, soft, alpha"}
    sc = hp.score(el, scenes, z=3)
    for st in scenes:
        print(f"  {el} {st}: mad {sc[st][0]:.2f} edge {sc[st][1]} shadows {info[st]}")
    hp.compare(el, scenes)
    hp.save_scene(el, scenes, {"how": spec["how"], "measured": info})
    return scenes, sc


def fel_mode(v):
    u, n = np.unique(v.astype(int), axis=0, return_counts=True)
    return [float(c) for c in u[int(np.argmax(n))]]


HOW = ("cylinder pedestal with its shadows drawn as layers (fix round): back wall; the cylinder's cast shadow on the wall and "
       "the picture's cast shadow on the wall (each the caster's silhouette moved by a measured offset, soft edge and strength "
       "measured); the cylinder (shading measured across it, a dark edge line on its right) with the top lip's shadow under the "
       "lip; its elliptical top (navy outline and rim, state colour) in a measured bevel with the picture's shadow on its rim; "
       "the picture kept as art (only the picture: ")
PRESS = {"selected": (0, 1), "disabled": (0, 1), "hover": (0, 1)}
SPECS = [
    dict(id="elevation-point", ellipse=(40.6, 48.4, 37.6, 23.6), caster=ARROWS, keep_inner=oval(41.0, 47.5, 21.0, 14.5),
         pic_off=dict(idle=(0, 0), **PRESS), pic="a grass mound with a red peak and two shaded yellow arrows",
         how=HOW + "a grass mound with a red peak, two shaded yellow arrows)"),
    dict(id="elevation-square", ellipse=(40.4, 48.4, 37.4, 23.6), caster=ARROWS, keep_inner=oval(40.0, 45.0, 26.6, 16.8),
         pic_off=dict(idle=(-2, -1), selected=(-2, 0), disabled=(-2, 0), hover=(-2, 0)),
         pic="a grass mound with a magenta flat top and two shaded yellow arrows",
         how=HOW + "a grass mound with a magenta flat top, two shaded yellow arrows)"),
    dict(id="elevation-area", ellipse=(40.4, 48.4, 37.2, 23.6), caster=ARROWS, keep_inner=oval(40.0, 46.6, 30.6, 18.8),
         pic_off=dict(idle=(-2, -1), selected=(-2, 0), disabled=(-2, 0), hover=(-2, 0)),
         pic="a wide grass mound under a red grid and two shaded yellow arrows",
         how=HOW + "a wide grass mound under a red grid, two shaded yellow arrows)"),
    dict(id="elevation-smooth", ellipse=(28.6, 50.2, 25.0, 16.2), caster=U(TRAILS, BALL), keep_inner=oval(28.6, 50.2, 22.0, 13.4),
         pic_off=dict(idle=(0, 0), **PRESS), pic="a grass disc, a ball on it shooting three green, white and yellow trails",
         how=HOW + "a grass disc, a ball shooting three green, white and yellow trails)"),
]

if __name__ == "__main__":
    want = sys.argv[1:]
    for sp in SPECS:
        if not want or sp["id"] in want:
            build(sp)
