"""reports_maps: the glass bubbles of Buy Land / Routing map / Member Roster (component C1, recipes.bubble), with their paint
MEASURED on this area's own art: the body's lit surface by least squares (accept.solve_bevel over every instance of a
colour at once), the rim and glyph colours as medians, the specular spot and the cast shadow by a small descent.

An instance: {"id", "sheet", "rim": (cx, cy, r), "body": (cx, cy, r), "glyph": shape | None, "colour": blue | yellow | silver,
"box": (x0, y0, x1, y1) frame box of the scored region, "shadow": shape the cast shadow is clipped to | None}.
"""
import copy
import json
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import common as C      # noqa: E402
import accept           # noqa: E402
import recipes          # noqa: E402
import sdf              # noqa: E402
import tunefit as T     # noqa: E402

STYLE_FILE = os.path.join(C.OUT, "bubble_style.json")


def start_style():
    S = copy.deepcopy(recipes.STYLE["bubble"])
    S["silver"] = copy.deepcopy(S["blue"])
    S["silver"]["shade"] = recipes.flat([215, 215, 225], recipes.BUBBLE_STOPS)
    return S


def load_style():
    if os.path.exists(STYLE_FILE):
        with open(STYLE_FILE) as f:
            return json.load(f)
    return start_style()


def item(inst, S, glyph_override=None):
    cx, cy, r = inst["rim"]
    g = inst["glyph"] if glyph_override is None else glyph_override
    it = recipes.bubble(cx, cy, r, state=inst["colour"], glyph=g, shadow=inst.get("shadow"), style=S, body=list(inst["body"]))
    if inst.get("shadow") is not None:
        it["box"] = [cx - 1.6 * r, cy - 1.6 * r, cx + 1.6 * r, cy + 1.6 * r]
    return it


def grid(inst):
    x0, y0, x1, y1 = inst["box"]
    jj, ii = np.mgrid[y0:y1, x0:x1]
    return ii + 0.5, jj + 0.5


def measure_paint(insts, S, colour):
    """The lit surface of one colour over all its instances (art pixels inside the body, off the glyph, off the spot)."""
    p = S[colour]
    rows, rims, glyph = [], [], []
    for inst in insts:
        if inst["colour"] != colour:
            continue
        rgb, _ = C.sheet(inst["sheet"])
        x0, y0, x1, y1 = inst["box"]
        art = rgb[y0:y1, x0:x1]
        x, y = grid(inst)
        cx, cy, r = inst["rim"]
        bx, by, br = inst["body"]
        d = np.hypot(x - bx, y - by) - br
        gd = sdf.sd(inst["glyph"], x, y) if inst["glyph"] else np.full(x.shape, 9.0)
        far = gd > 1.3
        a = accept.spot_alpha(p["sp"], cx, cy, r, x, y)
        tgt = (art - a[..., None] * np.asarray(p["spec"], float)) / np.maximum(1 - a[..., None], 0.05)
        mask = (d < -0.6) & far & (a < 0.8)
        rr_ = np.maximum(np.hypot(x - bx, y - by), 1e-9)
        nx, ny = (x - bx) / rr_, (y - by) / rr_
        rows.append(((-d / br)[mask], nx[mask], ny[mask], tgt[mask]))
        ring = (d > 0.7) & (np.hypot(x - cx, y - cy) < r - 0.7)
        if ring.any():
            rims.append(art[ring])
        if (gd < -0.9).any():
            glyph.append(art[gd < -0.9])
    p["shade"], n = accept.solve_bevel(rows, recipes.BUBBLE_STOPS)
    if rims:
        p["rim"] = [round(float(v), 1) for v in np.median(np.concatenate(rims), axis=0)]
    if glyph:
        p["glyph"] = [round(float(v), 1) for v in np.median(np.concatenate(glyph), axis=0)]
    return n


def bench(insts, base_fn):
    """(art, base, origin) per instance; base_fn(inst) gives the instance's wiped background (h x w x 3)."""
    out = []
    for inst in insts:
        rgb, _ = C.sheet(inst["sheet"])
        x0, y0, x1, y1 = inst["box"]
        out.append((rgb[y0:y1, x0:x1], base_fn(inst), (x0, y0)))
    return out


def loss_fn(insts, S, B, s=3, colour=None, region="all"):
    def loss():
        tot, n = 0.0, 0
        for inst, (art, base, org) in zip(insts, B):
            if colour and inst["colour"] != colour:
                continue
            low = T.render_low([item(inst, S)], base, org, s)
            x, y = grid(inst)
            cx, cy, r = inst["rim"]
            m = np.hypot(x - cx, y - cy) < (r + 1.5 if region == "ball" else r * 1.6)
            tot += float(((low - art) ** 2)[m].sum())
            n += int(m.sum())
        return tot / max(n, 1)
    return loss


def fit_spot(insts, S, B, colour, passes=3):
    sp = S[colour]["sp"]
    params = [[sp, 0, 0.03, -0.8, 0.8, 0.25], [sp, 1, 0.03, -0.8, 0.8, 0.25], [sp, 2, 0.02, 0.05, 0.8, 0.2],
              [sp, 3, 0.05, 0, 1, 0.5], [sp, 4, 0.1, 0.5, 4, 1.5]]
    return T.descend(loss_fn(insts, S, B, colour=colour, region="ball"), params, passes=passes, verbose=True, label=f"spot {colour}")


def fit_rim_edges(insts, S, B, passes=2):
    params = [[S["edge"], 0, 0.1, -1, 1.5, 0.6], [S["edge"], 1, 0.1, -1, 1.5, 0.6]]
    return T.descend(loss_fn(insts, S, B, region="ball"), params, passes=passes, verbose=True, label="edges")


def fit_shadow(insts, S, B, passes=3):
    sh = S["sh"]
    params = [[sh, 0, 0.02, -0.4, 0.4, 0.2], [sh, 1, 0.02, -0.4, 0.4, 0.2], [sh, 2, 0.02, 0.02, 0.6, 0.3],
              [sh, 3, 0.04, 0, 1, 0.5]]
    return T.descend(loss_fn(insts, S, B), params, passes=passes, verbose=True, label="shadow")
