"""uivec: can a shape be drawn with BASIC shapes? (P41)

decompose(rings) rebuilds a silhouette (the outline rings fit.glyph_rings measured) from circles, ellipses and rotated
rectangles: added shapes that stay inside the silhouette, then subtracted ones that stay inside its holes. The result is a
plain sdf.py shape ({"t": "sub", "a": union of the added, "b": union of the subtracted}) plus how much of the silhouette it
reproduces (iou). A silhouette counts as BASIC when iou >= IOU_MIN with at most MAX_SHAPES shapes; otherwise it needs a
free outline (a path).

Usage: py -3.12 re/tools/uivec/basic.py      the title menu's glyphs: a table and basic_glyphs.png
"""
import os
import sys

import numpy as np
from PIL import Image

sys.path.insert(0, os.path.dirname(__file__))
import fit      # noqa: E402
import sdf      # noqa: E402

IOU_MIN = 0.95
MAX_SHAPES = 12
Z = 4            # working grid: cells per frame pixel


def _edt_inside(allowed):
    """Distance (cells) from each allowed cell to the nearest not-allowed cell (brute force; grids are small)."""
    H, W = allowed.shape
    pad = np.pad(allowed, 1)
    edge = ~pad & (np.roll(pad, 1, 0) | np.roll(pad, -1, 0) | np.roll(pad, 1, 1) | np.roll(pad, -1, 1))
    ej, ei = np.nonzero(edge)
    ej, ei = ej - 1.0, ei - 1.0
    out = np.zeros((H, W))
    jj, ii = np.nonzero(allowed)
    for k in range(0, len(jj), 4000):
        dj = jj[k:k + 4000, None] - ej[None, :]
        di = ii[k:k + 4000, None] - ei[None, :]
        out[jj[k:k + 4000], ii[k:k + 4000]] = np.sqrt((dj * dj + di * di).min(axis=1))
    return out


def _holes(mask):
    """The background cells that the mask encloses."""
    H, W = mask.shape
    out = np.zeros((H, W), bool)
    seen = np.zeros((H, W), bool)
    stack = [(j, i) for j in range(H) for i in (0, W - 1)] + [(j, i) for i in range(W) for j in (0, H - 1)]
    for j, i in stack:
        if not mask[j, i]:
            seen[j, i] = True
    stack = [p for p in stack if seen[p]]
    while stack:
        j, i = stack.pop()
        for dj, di in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            a, b = j + dj, i + di
            if 0 <= a < H and 0 <= b < W and not mask[a, b] and not seen[a, b]:
                seen[a, b] = True
                stack.append((a, b))
    return ~mask & ~seen


def _grow(test, vals, steps, lims):
    """Round-robin growth: raise each value by its step while test(vals) holds."""
    moved = True
    while moved:
        moved = False
        for k in range(len(vals)):
            if vals[k] + steps[k] > lims[k]:
                continue
            vals[k] += steps[k]
            if test(vals):
                moved = True
            else:
                vals[k] -= steps[k]
    return vals


def _cover(target, allowed, x, y, max_shapes, done=0.04, inside=0.97, rects=True):
    """Greedy: the shape that covers the most still-uncovered target, staying inside `allowed`, again and again."""
    dt = _edt_inside(allowed) / Z
    covered = np.zeros(target.shape, bool)
    shapes, total = [], max(int(target.sum()), 1)
    diag = float(np.hypot(x.max() - x.min(), y.max() - y.min()))
    while len(shapes) < max_shapes:
        left = target & ~covered
        if left.sum() < done * total:
            break
        k = int(np.argmax(np.where(left, dt, -1)))
        pj, pi = divmod(k, target.shape[1])
        px, py, r0 = float(x[pj, pi]), float(y[pj, pi]), float(dt[pj, pi])
        if r0 < 0.3:
            break
        best = None

        def consider(shape, m):
            nonlocal best
            if not rects and shape["t"] == "poly":
                return
            gain = int((m & left).sum())
            if best is None or gain > best[0]:
                best = (gain, shape, m)

        consider({"t": "circle", "c": [round(px, 2), round(py, 2)], "r": round(r0, 2)}, np.hypot(x - px, y - py) <= r0)
        for deg in np.arange(0, 180, 15.0):
            c, s = np.cos(np.deg2rad(deg)), np.sin(np.deg2rad(deg))
            u, v = (x - px) * c + (y - py) * s, -(x - px) * s + (y - py) * c

            def rect(q):
                return (u >= -q[0]) & (u <= q[1]) & (v >= -q[2]) & (v <= q[3])

            def ok_rect(q):
                m = rect(q)
                n = m.sum()
                return n > 0 and (m & allowed).sum() >= inside * n

            a = r0 * 0.68
            q = _grow(ok_rect, [a, a, a, a], [0.25] * 4, [diag] * 4)
            if ok_rect(q):
                cu, cv = (q[1] - q[0]) / 2, (q[3] - q[2]) / 2
                hu, hv = (q[0] + q[1]) / 2, (q[2] + q[3]) / 2
                cx, cy = px + cu * c - cv * s, py + cu * s + cv * c
                pts = [[round(cx + du * c - dv * s, 2), round(cy + du * s + dv * c, 2)]
                       for du, dv in ((-hu, -hv), (hu, -hv), (hu, hv), (-hu, hv))]
                consider({"t": "poly", "rings": [pts]}, rect(q))
            if deg < 90:    # an ellipse has two axes: 0..90 covers every orientation

                def ell(q):
                    return (u / q[0]) ** 2 + (v / q[1]) ** 2 <= 1

                def ok_ell(q):
                    m = ell(q)
                    n = m.sum()
                    return n > 0 and (m & allowed).sum() >= inside * n

                q = _grow(ok_ell, [r0 * 0.95, r0 * 0.95], [0.25, 0.25], [diag, diag])
                if ok_ell(q) and max(q) > 1.15 * min(q):
                    consider({"t": "ellipse", "c": [round(px, 2), round(py, 2)], "r": [round(q[0], 2), round(q[1], 2)],
                              "rot": float(deg)}, ell(q))
        if best is None or best[0] < 0.01 * total:
            break
        shapes.append(best[1])
        covered |= best[2]
    return shapes, covered


def decompose(rings, max_shapes=MAX_SHAPES):
    """(shape, info) for the silhouette of `rings` (frame coordinates). info: n_add, n_sub, iou, basic."""
    pts = np.array([p for r in rings for p in r])
    x0, y0 = np.floor(pts.min(0)) - 1
    x1, y1 = np.ceil(pts.max(0)) + 1
    ys, xs = np.mgrid[0:int((y1 - y0) * Z), 0:int((x1 - x0) * Z)]
    x, y = (xs + 0.5) / Z + x0, (ys + 0.5) / Z + y0
    mask = sdf.sd({"t": "poly", "rings": rings}, x, y) < 0
    holes = _holes(mask)
    filled = mask | holes
    adds, cov_a = _cover(filled, filled, x, y, max_shapes)
    subs, cov_s = ([], np.zeros(mask.shape, bool))
    if holes.sum() >= 0.01 * filled.sum():
        subs, cov_s = _cover(holes, ~mask, x, y, max(2, max_shapes - len(adds)), done=0.06)
    got = cov_a & ~cov_s
    iou = float((got & mask).sum() / max((got | mask).sum(), 1))
    shape = {"t": "union", "of": adds} if adds else None
    if subs and shape:
        shape = {"t": "sub", "a": shape, "b": {"t": "union", "of": subs}}
    n = len(adds) + len(subs)
    return shape, {"n_add": len(adds), "n_sub": len(subs), "iou": round(iou, 3),
                   "basic": bool(shape is not None and iou >= IOU_MIN and n <= max_shapes)}


def polygons(rings, iou_min=0.96):
    """The same silhouette as straight-edged polygons with as few corners as reproduce it to iou_min:
    (rings, corners, iou). A second, looser meaning of "basic shapes": plain polygons, no curves."""
    pts = np.array([p for r in rings for p in r])
    x0, y0 = np.floor(pts.min(0)) - 1
    x1, y1 = np.ceil(pts.max(0)) + 1
    ys, xs = np.mgrid[0:int((y1 - y0) * Z), 0:int((x1 - x0) * Z)]
    x, y = (xs + 0.5) / Z + x0, (ys + 0.5) / Z + y0
    mask = sdf.sd({"t": "poly", "rings": rings}, x, y) < 0
    best = None
    for eps in (2.5, 2.0, 1.5, 1.2, 1.0, 0.8, 0.6, 0.45, 0.35, 0.25, 0.18):
        out = []
        for r in rings:
            q = fit.simplify([tuple(p) for p in r], eps)
            if len(q) >= 3:
                out.append([[round(a, 2), round(b, 2)] for a, b in q])
        if not out:
            continue
        got = sdf.sd({"t": "poly", "rings": out}, x, y) < 0
        iou = float((got & mask).sum() / max((got | mask).sum(), 1))
        best = (out, sum(len(r) for r in out), round(iou, 3))
        if iou >= iou_min:
            break
    return best


def main():
    import title
    out = os.path.join(fit.ROOT, "log", "roadmap", "P41", "title")
    meas = title.measure(verbose=False)
    z = 10
    rows = []
    print(f"{'glyph':10s} {'outline pts':>11s} {'rings':>5s} | {'added':>5s} {'cut':>4s} {'iou':>6s} verdict | "
          f"{'corners':>7s} {'iou':>6s}")
    for k, (name, m) in enumerate(meas.items()):
        g = m["idle"]
        shape, info = decompose(g["glyph"])
        verdict = "BASIC" if info["basic"] else "PATH"
        poly = polygons(g["glyph"])
        print(f"{name:10s} {sum(len(r) for r in g['glyph']):11d} {len(g['glyph']):5d} | {info['n_add']:5d} {info['n_sub']:4d} "
              f"{info['iou']:6.3f} {verdict:7s} | {poly[1]:7d} {poly[2]:6.3f}", flush=True)
        cx, cy, r = g["c"]
        box = (int(cx - r), int(cy - r), int(np.ceil(cx + r)), int(np.ceil(cy + r)))
        tiles = []
        for S in ({"t": "poly", "rings": g["glyph"]}, {"t": "poly", "rings": poly[0]}, shape):
            img = np.full(((box[3] - box[1]) * z, (box[2] - box[0]) * z, 3), 235.0)
            if S is not None:
                sdf.render([{"box": list(box), "layers": [{"shape": S, "fill": [36, 42, 90]}]}], img, z, origin=box[:2])
            tiles.append(img)
        # the basic shapes one by one, each in its own tint, so the construction can be read
        img = np.full_like(tiles[0], 235.0)
        if shape is not None:
            adds = shape["a"]["of"] if shape["t"] == "sub" else shape["of"]
            subs = shape["b"]["of"] if shape["t"] == "sub" else []
            tint = [[70, 90, 200], [200, 90, 70], [60, 160, 90], [170, 80, 180], [200, 150, 40], [40, 160, 180]]
            L = [{"shape": s, "fill": tint[i % len(tint)], "alpha": 0.6} for i, s in enumerate(adds)]
            L += [{"shape": s, "fill": [255, 255, 255], "alpha": 0.85} for s in subs]
            sdf.render([{"box": list(box), "layers": L}], img, z, origin=box[:2])
        tiles.append(img)
        rows.append(np.concatenate(tiles, axis=1))
        print(f"[{k + 1}/{len(meas)}] {name} {verdict}", flush=True)
    W = max(r.shape[1] for r in rows)
    rows = [np.pad(r, ((0, 0), (0, W - r.shape[1]), (0, 0)), constant_values=20) for r in rows]
    path = os.path.join(out, "basic_glyphs.png")
    Image.fromarray(np.clip(np.concatenate(rows) + 0.5, 0, 255).astype(np.uint8)).save(path)
    print(path)


if __name__ == "__main__":
    main()
