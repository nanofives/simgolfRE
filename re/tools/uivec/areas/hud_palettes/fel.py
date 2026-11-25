"""P41 hud_palettes: flat elements (pixel icons, flat chrome) built from authored parts with MEASURED flat colours.

A spec: id, parts [(name, shape, key)], drawn in order, shapes local to each state's cut; paint {key: "linear"} measures a
linear paint instead of a flat colour; offsets {state: (dx, dy)}. A key's flat colour per state is the most frequent art colour
among the pixels whose centre lies inside one of the key's parts and inside no later part (pixel art: the exact palette colour,
not a blend); colours {key: rgb} fixes one.
"""
import json
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hp  # noqa: E402
import bubble as B  # noqa: E402
import geom as G  # noqa: E402
from hp import sdf  # noqa: E402


def mode_colour(v):
    if len(v) == 0:
        return None
    u, n = np.unique(v.astype(int), axis=0, return_counts=True)
    return [float(c) for c in u[int(np.argmax(n))]]


def build(spec, write=True, verbose=True, z=None):
    el = spec["id"]
    cs = {c[0]: c for c in hp.crops(el)}
    states = spec.get("states") or list(cs)
    scenes, info = {}, {}
    for st in states:
        _, src, x0, y0, rgb, mask = cs[st]
        h, w = mask.shape
        X, Y = B.grid(x0, y0, w, h)
        dx, dy = spec.get("offsets", {}).get(st, (0, 0))
        parts = spec.get("parts_by_state", {}).get(st, spec["parts"])
        shapes = [G.moved(s, x0 + dx, y0 + dy) for _, s, _ in parts]
        ds = [sdf.sd(s, X, Y) for s in shapes]
        cols = {}
        keys = []
        for i, (name, s, key) in enumerate(parts):
            if key not in keys:
                keys.append(key)
        for key in keys:
            if key == "keep":
                continue
            fixed = spec.get("colours", {}).get(key)
            if isinstance(fixed, dict) and not ({"bevel", "linear", "radial"} & set(fixed)):
                fixed = fixed.get(st)
            if fixed is not None:
                cols[key] = fixed
                continue
            own = np.zeros((h, w), bool)
            anyin = np.zeros((h, w), bool)
            for i, (name, s, k) in enumerate(parts):
                if k != key:
                    continue
                later = np.zeros((h, w), bool)
                for j in range(i + 1, len(parts)):
                    if parts[j][2] != key:
                        later |= ds[j] < 0.2
                own |= (ds[i] < -0.05) & ~later
                anyin |= ds[i] < 0.3
            own &= mask
            pk = spec.get("paint", {}).get(key)
            if isinstance(pk, (list, tuple)) and pk[0] == "bevel" and own.sum() >= 6:
                # a lit surface of the key's (single) part: depth from its edge and the edge's direction, measured
                part = [G.moved(s, x0 + dx, y0 + dy) for n, s, k in parts if k == key][0]
                cols[key] = B.measure_bevel_shape(rgb, own | (mask & (sdf.sd(part, X, Y) < 0.0)), x0, y0, part, pk[1])
                cols[key]["bevel"]["of"] = part
                continue
            if pk == "linear" and own.sum() >= 3:
                cols[key] = B.measure_linear(rgb, own, x0, y0)
            else:
                c = mode_colour(rgb[own]) or mode_colour(rgb[anyin & mask]) or [128, 128, 128]
                cols[key] = c
        layers = [{"shape": s, "fill": cols[k], "name": n} for (n, _, k), s in zip(parts, shapes) if k != "keep"]
        scenes[st] = [{"box": [x0 - 2, y0 - 2, x0 + w + 2, y0 + h + 2], "layers": layers}]
        for (n, _, k), s in zip(parts, shapes):
            if k == "keep":      # a picture kept as art: the scene records where, the comparison shows the art's pixels there
                scenes[st].append({"keep_art": s, "name": n})
        info[st] = {"colours": cols}
    sc = hp.score(el, scenes, z=4)
    if verbose:
        for st in states:
            print(f"  {el} {st}: mad {sc[st][0]:.2f} edge {sc[st][1]}")
    if write:
        hp.compare(el, scenes, z=z)
        hp.save_scene(el, scenes, {"parts": [{"name": n, "key": k} for n, _, k in spec["parts"]], "measured": info,
                                   "how": spec.get("how", "")})
    return scenes, sc, info
