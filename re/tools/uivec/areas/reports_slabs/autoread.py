"""Readers that place authored primitives from measurements (no colour-region vectorising: each result is a rounded
rectangle or a straight line whose numbers are read on the pixel grid, as boxes.py reads pills).

cells(sheet, ...)  flat light rectangles (table cells, colour bands, legend squares): every exact-colour component whose
                   best rounded rectangle (boxes.fit_rrect) covers it to >= 0.93 and whose corners are small (else it is a
                   pill and parts.measure_pill reads it). Returns [x0, y0, x1, y1, r, rgb].
lines(sheet, ...)  straight navy lines: rows (columns) holding a run of dark pixels >= minlen, grouped across adjacent rows
                   into one line; its centre and width are read with parts.line_profile across it. Returns horizontal and
                   vertical segments [a, b, w].
elbows(h, v)       a horizontal and a vertical line whose ends meet near a common corner: one path with a fillet whose
                   radius is the gap the art leaves at the corner.
"""
import numpy as np

import boxes
import common as C
import parts as P


def cells(raw, alpha, skip, min_area=30, min_fit=0.93, exclude_cols=(), include_cols=()):
    key = (raw[..., 0].astype(np.int32) << 16) | (raw[..., 1].astype(np.int32) << 8) | raw[..., 2].astype(np.int32)
    full = alpha > 0.99
    vals, counts = np.unique(key[full], return_counts=True)
    out = []
    for v, n in zip(vals, counts):
        if n < min_area:
            continue
        col = (int(v >> 16), int((v >> 8) & 255), int(v & 255))
        if col in exclude_cols or col == (148, 150, 198):     # the slab's own lilac is never a cell
            continue
        L = float(C.fit.lum(np.array(col, float)))
        sat = max(col) - min(col)
        if col in include_cols:
            pass
        elif L < 140 and sat < 120:      # navy, slab lilac and shading are not cells; saturated reds / greens are
            continue
        if col[2] > col[0] + 25 and col[2] > col[1] + 25 and L < 200:     # the lilac family: slab, pills, shading
            continue
        for comp in boxes.components((key == v) & full, min_area):
            x0, y0, x1, y1, r, iou = boxes.fit_rrect(comp)
            if min(x1 - x0, y1 - y0) < 3:
                continue
            if any(x0 >= a - 1 and y0 >= b - 1 and x1 <= c + 1 and y1 <= d + 1 for a, b, c, d in skip):
                continue
            if iou < min_fit:
                # a cell partly covered by pieces drawn over it (a pill on a band): accepted when its box's border
                # pixels are nearly all its own colour (the cover sits inside)
                if iou < 0.6:
                    continue
                m = np.zeros((y1 - y0, x1 - x0), bool)
                m[comp[:, 0] - y0, comp[:, 1] - x0] = True
                sides = [m[0].mean(), m[-1].mean(), m[:, 0].mean(), m[:, -1].mean()]
                if (sum(v >= 0.75 for v in sides) < 3 and iou < 0.8) or min(x1 - x0, y1 - y0) < 6:
                    continue
            out.append([x0, y0, x1, y1, r, list(col), round(iou, 3)])
    return out


def lines(meas, alpha, block, valid, thr=85.0, minlen=6):
    """block: bool mask of pixels already explained (pills, plates); valid: rows that hold art."""
    L = C.fit.lum(meas)
    D = (L < thr) & (alpha > 0.5) & ~block & valid
    found = {"h": [], "v": []}
    for mode in ("h", "v"):
        M = D if mode == "h" else D.T
        segs = []          # (row, x0, x1)
        for j in range(M.shape[0]):
            row = M[j]
            if not row.any():
                continue
            on = np.nonzero(np.diff(np.r_[0, row.astype(np.int8), 0]))[0]
            for a, b in zip(on[::2], on[1::2]):
                if b - a >= minlen:
                    segs.append((j, a, b))
        # group runs of adjacent rows that overlap
        groups = []
        for j, a, b in segs:
            for g in groups:
                if g["j1"] == j - 1 and min(b, g["b"]) - max(a, g["a"]) > 0.5 * min(b - a, g["b"] - g["a"]):
                    g["j1"] = j
                    g["rows"].append((j, a, b))
                    g["a"], g["b"] = min(g["a"], a), max(g["b"], b)
                    break
            else:
                groups.append({"j0": j, "j1": j, "a": a, "b": b, "rows": [(j, a, b)]})
        for g in groups:
            if g["j1"] - g["j0"] > 3:      # thicker than a line: a filled shape, not a wire
                continue
            # the line's extent: where its darkest row is dark
            a = int(np.median([r[1] for r in g["rows"]])) if len(g["rows"]) > 1 else g["a"]
            b = int(np.median([r[2] for r in g["rows"]])) if len(g["rows"]) > 1 else g["b"]
            a, b = g["a"], g["b"]
            ks = np.linspace(a + 1, b - 2, min(9, max(b - a - 2, 1))).astype(int)
            cs, ws = [], []
            for k in ks:
                lo, hi = g["j0"] - 3, g["j1"] + 4
                if mode == "h":
                    c, w = P.line_profile(meas, "v", int(k), max(lo, 0), hi, "lo")
                else:
                    c, w = P.line_profile(meas, "h", int(k), max(lo, 0), hi, "lo")
                if c is not None and g["j0"] - 0.5 <= c <= g["j1"] + 1.5:
                    cs.append(c)
                    ws.append(w)
            if not cs:
                continue
            found[mode].append({"c": float(np.median(cs)), "a": float(a), "b": float(b), "w": float(np.median(ws))})
    return found


def to_strokes(found, join=12.0):
    """Line records -> wire paths (butt ends), with an elbow wherever a horizontal and a vertical end meet near one corner.
    Returns list of (pts, radii, w)."""
    H = [dict(l, used_a=False, used_b=False) for l in found["h"]]
    V = [dict(l, used_a=False, used_b=False) for l in found["v"]]
    paths = []
    for h in H:
        for v in V:
            # corner point
            P_ = (v["c"], h["c"])
            for he in ("a", "b"):
                hx = h[he]
                if abs(hx - P_[0]) > join or h["used_" + he]:
                    continue
                for ve in ("a", "b"):
                    vy = v[ve]
                    if abs(vy - P_[1]) > join or v["used_" + ve]:
                        continue
                    # both ends must stop short of the corner on their own side
                    gx = abs(hx - P_[0])
                    gy = abs(vy - P_[1])
                    if gx < 1.5 and gy < 1.5:
                        continue
                    r = max(min(gx, gy), 1.0)
                    h["used_" + he] = v["used_" + ve] = True
                    hp = [h["b" if he == "a" else "a"], h["c"]]
                    vp = [v["c"], v["b" if ve == "a" else "a"]]
                    paths.append(([hp, [P_[0], P_[1]], vp], [r], (h["w"] + v["w"]) / 2, ("h", id(h)), ("v", id(v))))
    used = set()
    out = []
    for pts, rr, w, hk, vk in paths:
        used.add(hk)
        used.add(vk)
        out.append((pts, rr, w))
    for h in H:
        if ("h", id(h)) not in used:
            out.append(([[h["a"], h["c"]], [h["b"], h["c"]]], [], h["w"]))
    for v in V:
        if ("v", id(v)) not in used:
            out.append(([[v["c"], v["a"]], [v["c"], v["b"]]], [], v["w"]))
    return out
