"""P41 hud_palettes: build one bubble element from its spec (see bubble.py for what is drawn and what is measured).

SPEC keys (all coordinates LOCAL to the reference cut, 0..w x 0..h):
  id, ref (state the glyph was read on), circle (cx, cy, r) a hand-read guess of the rim's outer edge,
  ground [(name, shape)], shadow (bool), glyph [(name, shape, colour key)], fixed_colours {key: rgb} (optional),
  offsets {state: (dx, dy)} the glyph's shift on other states' cuts (default: found by matching the dark pixels),
  rim_dark (lum below which a pixel is rim navy), states (subset), circle_by_state {state: (cx, cy, r)} guesses,
  exclude_grow (glyph margin left out of the paint measurement)
"""
import json
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hp  # noqa: E402
import bubble as B  # noqa: E402
import geom as G  # noqa: E402
from hp import fit, sdf  # noqa: E402


def dark_mask(rgb, mask, lim):
    if lim < 0:      # a light glyph: lum above -lim
        return (fit.lum(rgb.astype(float)) > -lim) & mask
    return (fit.lum(rgb.astype(float)) < lim) & mask


def extent_poly(mask, eps=0.55):
    """The outline of a cut's opaque pixels, local coordinates, simplified (Douglas-Peucker, eps px)."""
    loops = fit.contours(mask.astype(float), 0.5)
    loop = max(loops, key=len)
    q = fit.simplify(loop, eps)
    return {"t": "poly", "rings": [[[round(x, 2), round(y, 2)] for x, y in q]]}


def match_offset(ref_dark, dark, reach=3):
    best, bo = -1, (0, 0)
    h, w = ref_dark.shape
    for dy in range(-reach, reach + 1):
        for dx in range(-reach, reach + 1):
            a = np.zeros_like(ref_dark)
            ys, xs = np.nonzero(ref_dark)
            ys2, xs2 = ys + dy, xs + dx
            ok = (ys2 >= 0) & (ys2 < dark.shape[0]) & (xs2 >= 0) & (xs2 < dark.shape[1])
            sc = dark[ys2[ok], xs2[ok]].sum() - 0.5 * (~dark[ys2[ok], xs2[ok]]).sum()
            if sc > best:
                best, bo = sc, (dx, dy)
    return bo


def build(spec, write=True, z=None, verbose=True):
    el = spec["id"]
    cs = spec.get("_crops") or {c[0]: c for c in hp.crops(el)}      # _crops: a window of another cut (a bubble inside a tray)
    states = spec.get("states") or list(cs)
    ref = spec.get("ref", states[0])
    _, _, rx0, ry0, rrgb, rmask = cs[ref]
    lim = spec.get("rim_dark", 70)
    gcx, gcy, gr = spec["circle"]
    glyph_union = G.U(*[s for _, s, _ in spec["glyph"]]) if spec["glyph"] else None
    # the glyph's position per state, found on its dark pixels inside the body
    x, y = B.grid(0, 0, rmask.shape[1], rmask.shape[0])
    body_in = np.hypot(x - gcx, y - gcy) < gr - 3
    ref_dark = dark_mask(rrgb, rmask, spec.get("glyph_dark", lim + 20)) & body_in
    scenes, info = {}, {}
    for st in states:
        _, src, x0, y0, rgb, mask = cs[st]
        h, w = mask.shape
        off = spec.get("offsets", {}).get(st)
        if off is None:
            xs, ys = B.grid(0, 0, w, h)
            bin_s = np.hypot(xs - gcx, ys - gcy) < gr - 3
            off = (0, 0) if st == ref else match_offset(ref_dark, dark_mask(rgb, mask, spec.get("glyph_dark", lim + 20)) & bin_s)
        cg = spec.get("circle_by_state", {}).get(st, (gcx + off[0], gcy + off[1], gr))
        circ = B.fit_rim(rgb, mask, x0, y0, dark=lim, guess=(cg[0] + x0, cg[1] + y0, cg[2]), arc=spec.get("rim_arc", (110.0, 350.0)))
        cx, cy, r = circ[:3]
        if st in spec.get("circle_fixed", {}):      # read by hand on the luminance profiles (the fit is fooled there)
            fx, fy, fr = spec["circle_fixed"][st]
            cx, cy, r = fx + x0, fy + y0, fr
        X, Y = B.grid(x0, y0, w, h)
        dc = np.hypot(X - cx, Y - cy) - r
        gl = [G.moved(s, x0 + off[0], y0 + off[1]) for _, s, _ in spec["glyph"]]
        # measure_shift (fix round): the glyph is drawn somewhere else than the art has it (re-centred); its paints and the
        # body's exclusion are measured on the art's glyph position (drawn position + shift), then moved back with the glyph
        ms = spec.get("measure_shift", (0.0, 0.0))
        glm = [G.moved(q, ms[0], ms[1]) for q in gl]
        excl = np.zeros((h, w), bool)
        if gl:
            excl = sdf.sd(G.U(*glm), X, Y) < spec.get("exclude_grow", 1.3)
        keep = mask & (dc < -0.55) & ~excl
        circle = {"t": "circle", "c": [cx, cy], "r": r}
        rim_l, body_l, rw, _ = B.measure_rim_body(rgb, mask, x0, y0, (cx, cy, r), excl, kdir=spec.get("body_kdir", 1.0))
        layers = []
        gspec = spec.get("ground_by_state", {}).get(st, spec.get("ground", ()))
        if gspec == "auto":
            gspec = [("the tray pixels the sprite carries round the bubble: the cut's extent (a simplified outline of its "
                      "opaque pixels), painted as a socket lit round the bubble", extent_poly(mask), "socket")]
        if gspec == "auto_circle":
            ec = fit.fit_circle(fit.edge_points(mask), trim=1.0)
            gspec = [("the light outer ring the cut carries round the rim (a circle fitted to the cut's extent), painted as "
                      "a socket lit round the bubble", G.circle(ec[0], ec[1], ec[2]), "socket")]
        grounds, gkinds = [], []
        for g in gspec:
            name, gs0 = g[0], g[1]
            kind = g[2] if len(g) > 2 else "linear"
            gs = G.moved(gs0, x0, y0)
            if kind.startswith("well:"):      # a round recess round the bubble, dr px wider than its rim, lit as measured
                gs = G.circle(cx, cy, r + float(kind.split(":")[1]))
            near = 0.6 if (kind.startswith("well:") or kind == "socket") else (
                min(0.75 * r, spec.get("ground_gap", 5.0)) if spec.get("shadow") else 0.8)
            gk = mask & (sdf.sd(gs, X, Y) < -0.3) & (dc > near)
            if gk.sum() <= 3:
                gp = [128, 128, 160]
            elif kind == "bevel":
                gp = B.measure_bevel_shape(rgb, gk, x0, y0, gs, spec.get("ground_stops", [0.0, 1.0, 2.0, 3.5, 5.5, 8.0, 12.0]))
            elif kind.startswith("well:"):
                gp = B.measure_bevel_shape(rgb, mask & (dc > 0.6) & (sdf.sd(gs, X, Y) < -0.3), x0, y0, gs,
                                           [0.0, 1.0, 2.0, 3.0, 4.0, 5.5, 7.0])
                gp["bevel"]["of"] = gs
            elif kind == "socket":
                sock = {"t": "circle", "c": [cx, cy], "r": r + spec.get("socket_w", 7.0)}
                gp = B.measure_bevel_shape(rgb, mask & (dc > 0.6) & (sdf.sd(gs, X, Y) < -0.3), x0, y0, sock,
                                           [0.0, 1.5, 3.0, 4.5, 5.8, 6.6, 7.4])
                gp["bevel"]["of"] = sock
            elif kind == "flat":
                gp = [round(float(v), 1) for v in np.median(rgb[gk].astype(float), 0)]
            else:
                gp = B.measure_linear(rgb, gk, x0, y0)
            grounds.append(gs)
            gkinds.append(kind)
            layers.append({"shape": gs, "fill": gp, "name": name})
        if spec.get("shadow") and grounds and st in spec.get("shadow_states", [st]):
            base = [{"box": [x0, y0, x0 + w, y0 + h], "layers": list(layers)}]
            gk = mask & (dc > 0.6) & (sdf.sd(G.U(*grounds), X, Y) < -0.5)
            shl, sl = B.fit_shadow(rgb.astype(float), gk, x0, y0, base, (cx, cy, r),
                                   init=spec.get("shadow_init", (0.15, 0.2, 1.0, 0.3, 0.4)))
            shl["clip"] = G.U(*grounds)
            shl["name"] = "the bubble's cast shadow (measured)"
            layers.append(shl)
        layers.append(rim_l)
        layers.append(body_l)
        body = {"t": "circle", "c": [cx, cy], "r": r - rw}
        cols = {}
        for (name, s, key), sm in zip(spec["glyph"], gl):
            fixed = spec.get("fixed_colours", {}).get(st, {}).get(key) or spec.get("fixed_colours", {}).get(key)
            if key == "hole":      # an opening in the glyph: the body's own paint shows through
                hp_ = json.loads(json.dumps(body_l["fill"]))
                hp_["bevel"]["of"] = body_l["shape"]
                cols[key] = hp_
            elif fixed is not None:
                cols[key] = fixed
            elif key not in cols and isinstance(spec.get("paint", {}).get(key), dict):
                # two colours split across a vertical line: each measured as the median on its own side
                xc, hw = spec["paint"][key]["split_x"]
                same = G.U(*[g for (n2, s2, k2), g in zip(spec["glyph"], gl) if k2 == key])
                kk = mask & (sdf.sd(same, X, Y) < -spec.get("colour_depth", 0.3))
                xa = x0 + off[0] + xc
                lc = np.median(rgb[kk & (X < xa - hw)].astype(float), 0)
                rc = np.median(rgb[kk & (X > xa + hw)].astype(float), 0)
                yy = float(y0 + h / 2)
                cols[key] = {"linear": {"a": [xa - hw, yy], "b": [xa + hw, yy],
                                        "stops": [[0.0, [round(float(v), 1) for v in lc]], [1.0, [round(float(v), 1) for v in rc]]]}}
            elif key not in cols and spec.get("paint", {}).get(key) == "linear":
                same = G.U(*[g for (n2, s2, k2), g in zip(spec["glyph"], glm) if k2 == key])
                kk = mask & (sdf.sd(same, X, Y) < -spec.get("colour_depth", 0.3))
                cols[key] = G.moved(B.measure_linear(rgb, kk, x0, y0), -ms[0], -ms[1])
            elif key not in cols:
                same = G.U(*[g for (n2, s2, k2), g in zip(spec["glyph"], glm) if k2 == key])
                cols[key] = B.part_colour(rgb, mask, x0, y0, same, depth=spec.get("colour_depth", 0.3), fallback=[36, 42, 90],
                                          pick=spec.get("pick", {}).get(key, "dark" if key.startswith("navy") else "median"))
            L = {"shape": sm, "fill": cols[key], "name": name}
            if spec.get("clip_glyph", True):
                L["clip"] = body
            layers.append(L)
        m = 2
        scenes[st] = [{"box": [x0 - m, y0 - m, x0 + w + m, y0 + h + m], "layers": layers}]
        info[st] = {"circle": [round(cx, 2), round(cy, 2), round(r, 2)], "glyph_offset": list(off),
                    "colours": {k: v for k, v in cols.items() if k != "hole"}, "rim_w": rw,
                    "shadow": [round(v, 2) for v in layers[len(grounds)]["shape"]["c"]] + [round(layers[len(grounds)]["shape"]["r"], 2), round(layers[len(grounds)]["alpha"], 2)] if spec.get("shadow") and grounds and st in spec.get("shadow_states", [st]) else None}
    if spec.get("_crops"):
        return scenes, None, info
    sc = hp.score(el, scenes, z=4)
    if verbose:
        for st in states:
            print(f"  {el} {st}: circle {info[st]['circle']} off {info[st]['glyph_offset']} rim {info[st]['rim_w']} sh {info[st]['shadow']} mad {sc[st][0]:.2f} edge {sc[st][1]}")
    if write:
        hp.compare(el, scenes, z=z)
        parts = [{"name": n, "key": k} for n, _, k in spec["glyph"]]
        hp.save_scene(el, scenes, {"parts": parts, "measured": info,
                                   "how": spec.get("how", "")})
    return scenes, sc, info
