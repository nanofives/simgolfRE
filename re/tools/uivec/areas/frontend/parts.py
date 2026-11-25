"""P41 frontend area: measured parts shared by the screens (no toolkit file is changed).

- tray(): a dark rounded panel with a navy outline (the trays behind the Load Game pills and list, the Pick a Pro list): the
  outline's edges read on rays from the middle of each side, the corner radius fitted to the outline's corner pixels, the
  fill a linear gradient fitted by least squares to the panel's own pixels.
- wires(): the thin dark connector lines on a panel, read as horizontal / vertical runs of dark pixels (each a `seg`).
- linear_fit(): a linear paint (two stops along the direction the colour changes most) fitted to some pixels.
- bevel_fit(): the toolkit's measured lit surface (accept.solve_bevel) over the pixels inside a shape.
- ellipse_of(): an ellipse fitted to pixel-edge points (fit.fit_ellipse), with outlier rounds.
"""
import numpy as np

import common as c
from common import fit, sdf

NAVY = [35, 43, 80]


def rr(x0, y0, x1, y1, r):
    return {"t": "rrect", "c": [(x0 + x1) / 2, (y0 + y1) / 2], "half": [(x1 - x0) / 2, (y1 - y0) / 2], "r": r, "rot": 0.0}


def grid(x0, y0, x1, y1):
    yy, xx = np.mgrid[y0:y1, x0:x1]
    return xx + 0.5, yy + 0.5


def linear_fit(art, mask, x0, y0):
    """art crop (h x w x 3), mask of pixels to fit, crop origin. Returns a linear paint (or a flat colour when it is flat)."""
    j, i = np.nonzero(mask)
    X, Y = i + 0.5 + x0, j + 0.5 + y0
    A = np.c_[np.ones(len(X)), X, Y]
    C = np.linalg.lstsq(A, art[j, i], rcond=None)[0]       # 3 x 3: rgb = c0 + cx x + cy y
    g = C[1:].mean(axis=1)                                    # the gradient of the mean channel
    n = np.hypot(*g)
    if n * max(np.ptp(X), np.ptp(Y)) < 3:
        return [round(float(v), 1) for v in art[j, i].mean(0)]
    d = g / n
    t = X * d[0] + Y * d[1]
    lo, hi = np.percentile(t, 1), np.percentile(t, 99)
    mx, my = X.mean(), Y.mean()
    tm = mx * d[0] + my * d[1]
    a = [mx + (lo - tm) * d[0], my + (lo - tm) * d[1]]
    b = [mx + (hi - tm) * d[0], my + (hi - tm) * d[1]]
    ca = C[0] + C[1] * a[0] + C[2] * a[1]
    cb = C[0] + C[1] * b[0] + C[2] * b[1]
    return {"linear": {"a": [round(v, 2) for v in a], "b": [round(v, 2) for v in b],
                       "stops": [[0.0, [round(float(v), 1) for v in ca]], [1.0, [round(float(v), 1) for v in cb]]]}}


def bevel_fit(art, shape, mask, x0, y0, L, stops):
    """The lit-surface paint of `shape` measured on art (crop at x0, y0) over the pixels of mask (accept.solve_bevel)."""
    import accept
    h, w = mask.shape
    X, Y = grid(x0, y0, x0 + w, y0 + h)
    d = sdf.sd(shape, X, Y)
    nx, ny = sdf.normal(shape, X, Y, d)
    m = mask & (d < -0.5)
    coef, n = accept.solve_bevel([((-d / L)[m], nx[m], ny[m], art[m])], stops)
    return {"bevel": {"L": L, "stops": stops, "coef": coef}}


def ellipse_of(pts, rot=None, tol=0.06, rounds=3):
    pts = np.asarray(pts, float)
    e = fit.fit_ellipse(pts, rot)
    for _ in range(rounds):
        cx, cy, a, b, r = e
        t = np.deg2rad(r)
        u = (pts[:, 0] - cx) * np.cos(t) + (pts[:, 1] - cy) * np.sin(t)
        v = -(pts[:, 0] - cx) * np.sin(t) + (pts[:, 1] - cy) * np.cos(t)
        keep = np.abs(np.hypot(u / a, v / b) - 1) < tol
        if keep.sum() < 10:
            break
        pts = pts[keep]
        e = fit.fit_ellipse(pts, rot)
    return [round(float(v), 2) for v in e]


def tray(art, box, r=None, outline=NAVY, exclude=None, max_l=150):
    """A dark panel with a navy outline. box: (x0, y0, x1, y1) of the outline's OUTER edge, read off a dump of its corners;
    r: the outer corner radius (fitted to the outline's corner pixels when None). Returns (layers, info)."""
    x0, y0, x1, y1 = box
    Lm = fit.lum(art)
    # the line's width on each side, read along rays through the middle of each side (outward is toward the box edge)
    cxm, cym = int((x0 + x1) / 2), int((y0 + y1) / 2)
    widths = {}
    for side, (px, py, dx, dy) in zip("tblr", ((cxm, int(y0) + 6, 0, -1), (cxm, int(np.ceil(y1)) - 7, 0, 1),
                                                (int(x0) + 6, cym, -1, 0), (int(np.ceil(x1)) - 7, cym, 1, 0))):
        run = [float(Lm[py + dy * k, px + dx * k]) for k in range(9)]
        dk = [k for k, v in enumerate(run) if v < 72]
        widths[side] = float(len(dk)) if dk else 1.0
    if r is None:
        # the corner: the outermost dark pixel per row in the top-left corner, fitted to a circle
        pts = []
        for j in range(int(y0), int(y0) + 12):
            row = Lm[j, int(x0):int(x0) + 14]
            k = np.nonzero(row < 72)[0]
            if len(k):
                pts.append((x0 + k[0], j + 0.5))
        for i in range(int(x0), int(x0) + 12):
            col = Lm[int(y0):int(y0) + 14, i]
            k = np.nonzero(col < 72)[0]
            if len(k):
                pts.append((i + 0.5, y0 + k[0]))
        pts = [p for p in pts if p[0] > x0 + 0.6 and p[1] > y0 + 0.6]
        if len(pts) >= 5:
            cx, cy, rr_, _ = fit.fit_circle(pts, trim=0.8)
            r = round(float(np.clip((cx - x0 + cy - y0) / 2 + 0.0, 2, 20)), 2)
            r = round(float(np.clip(rr_, 2, 20)), 2)
        else:
            r = 6.0
    t, b, l, rgt = widths["t"], widths["b"], widths["l"], widths["r"]
    outer = rr(x0, y0, x1, y1, r)
    inner = rr(x0 + l, y0 + t, x1 - rgt, y1 - b, max(r - (l + t) / 2, 0.5))
    # the fill: least squares over the inside, away from the line and from whatever is drawn on the panel
    ix0, iy0, ix1, iy1 = int(x0), int(y0), int(np.ceil(x1)), int(np.ceil(y1))
    X, Y = grid(ix0, iy0, ix1, iy1)
    m = sdf.sd(inner, X, Y) < -1.5
    if exclude is not None:
        m &= ~exclude[iy0:iy1, ix0:ix1]
    m &= (Lm[iy0:iy1, ix0:ix1] > 72) & (Lm[iy0:iy1, ix0:ix1] < max_l)      # not the light things drawn on the panel
    paint = linear_fit(art[iy0:iy1, ix0:ix1], m, ix0, iy0)
    layers = [{"shape": outer, "fill": list(outline)}, {"shape": inner, "fill": paint}]
    return layers, {"box": [x0, y0, x1, y1], "r": r, "widths": widths}


def wires(art, region, exclude, dark=70, min_len=3):
    """Thin dark connector lines inside region (x0, y0, x1, y1): horizontal and vertical runs of pixels darker than `dark`
    that are at most 2 px thick, outside `exclude` (a frame mask of the shapes already drawn). Each run becomes a seg,
    width = its thickness, colour = its median."""
    x0, y0, x1, y1 = region
    Lm = fit.lum(art)
    D = (Lm < dark)
    D[:y0] = False
    D[y1:] = False
    D[:, :x0] = False
    D[:, x1:] = False
    D &= ~exclude
    segs = []
    used = np.zeros_like(D)
    # horizontal runs: rows of a component no more than 2 tall
    for axis in (0, 1):
        A = D if axis == 0 else D.T
        U = used if axis == 0 else used.T
        H, W = A.shape
        for j in range(H):
            row = A[j]
            k = 0
            while k < W:
                if not row[k] or U[j, k]:
                    k += 1
                    continue
                e = k
                while e < W and row[e]:
                    e += 1
                if e - k >= min_len:
                    th = 1
                    if j + 1 < H and A[j + 1, k:e].mean() > 0.8:
                        th = 2
                    if not (j + th < H and A[j + th, k:e].mean() > 0.5):
                        cols = art[j:j + th, k:e] if axis == 0 else art[k:e, j:j + th]
                        col = [round(float(v), 1) for v in np.median(cols.reshape(-1, 3), axis=0)]
                        if axis == 0:
                            segs.append({"t": "seg", "a": [k, j + th / 2], "b": [e, j + th / 2], "w": float(th), "rgb": col})
                            U[j:j + th, k:e] = True
                        else:
                            segs.append({"t": "seg", "a": [j + th / 2, k], "b": [j + th / 2, e], "w": float(th), "rgb": col})
                            U[j:j + th, k:e] = True
                k = e
    return segs


def seg_layers(segs):
    return [{"shape": {k: v for k, v in s.items() if k != "rgb"}, "fill": s["rgb"]} for s in segs]


DISC_STOPS = [0.0, 0.15, 0.35, 0.6, 1.0]


def socket(art, cx, cy, r, reach=10.0, dark=56):
    """The dark socket ring a bubble sits in (Load Game OK / Delete, the Theme Packs and world map bubbles): along rays from
    the bubble's centre, past its rim, a shaded band and then a navy line; the line's outer edge is where the last dark
    pixel ends (median over the rays the wires do not cross). Drawn as a navy disc of that radius and the band (a lit
    surface measured on the art) inside its line. Returns (layers, info)."""
    Lm = fit.lum(art)
    outs, lines = [], []
    for deg in range(0, 360, 6):
        t = np.deg2rad(deg)
        ks = np.arange(r + 1.0, r + reach, 0.5)
        v = np.array([Lm[int(cy + k * np.sin(t)), int(cx + k * np.cos(t))] for k in ks])
        dk = np.nonzero(v < dark)[0]
        if len(dk) == 0:
            continue
        last = dk[-1]
        if last == len(ks) - 1:     # a wire runs out along this ray
            continue
        first = dk[0]
        # the line: the run of dark ending at `last`
        s = last
        while s - 1 >= 0 and v[s - 1] < dark:
            s -= 1
        if ks[s] - r < 1.5:          # dark from the rim on: no band on this ray
            continue
        outs.append(ks[last] + 0.5)
        lines.append(ks[last] + 0.5 - ks[s])
    R = round(float(np.median(outs)) - 0.4, 2)     # the last dark pixel's far side overshoots the line's edge
    w = round(float(np.clip(np.median(lines), 1.0, 2.5)), 2)
    x0, y0 = int(cx - R - 2), int(cy - R - 2)
    x1, y1 = int(np.ceil(cx + R + 2)), int(np.ceil(cy + R + 2))
    X, Y = grid(x0, y0, x1, y1)
    band = {"t": "circle", "c": [cx, cy], "r": R - w}
    rr_ = np.hypot(X - cx, Y - cy)
    m = (rr_ < R - w - 0.6) & (rr_ > r + min(2.5, (R - w - r) / 2)) & (Lm[y0:y1, x0:x1] > dark)
    paint = bevel_fit(art[y0:y1, x0:x1], band, m, x0, y0, R - w - r, DISC_STOPS)
    line_rgb = [round(float(v), 1) for v in np.median(art[y0:y1, x0:x1][(rr_ > R - w + 0.2) & (rr_ < R - 0.2)], axis=0)]
    layers = [{"shape": {"t": "circle", "c": [cx, cy], "r": R}, "fill": line_rgb, "soft": 0.5},
              {"shape": band, "fill": paint}]
    return layers, {"R": R, "line": w}


def disc_button(arts, cell, glyph, ground=None, thr=10, circle=None):
    """A matte disc button (Back): the disc is every pixel of the cell that differs from the plain ground round it by more
    than `thr` levels, outermost on rays, fitted to a circle; per state its lit paint is measured on that state's art
    (accept.solve_bevel over the disc minus the glyph) and the glyph's colour is the median of its pixels.
    arts: {state: frame image}. glyph: the authored glyph shape. Returns ({state: item}, (cx, cy, r))."""
    a0 = next(iter(arts.values()))
    x, y, w, h = cell
    sub = a0[y:y + h, x:x + w]
    if ground is None:
        ring = np.concatenate([sub[0], sub[-1], sub[:, 0], sub[:, -1]])
        ground = np.median(ring, axis=0)
    if circle is None:
        m = np.abs(sub - np.asarray(ground, float)).max(axis=2) > thr
        pts = fit.outermost(m, (w / 2, h / 2))
        cx, cy, r, res = fit.fit_circle(pts, trim=0.8)
        cx, cy = cx + x, cy + y
    else:       # read off a dump where the ground is textured and no clean outline exists
        (cx, cy, r), res = circle, 0.0
    disc = {"t": "circle", "c": [cx, cy], "r": r}
    bx0, by0 = int(cx - r - 2), int(cy - r - 2)
    bx1, by1 = int(cx + r + 3), int(cy + r + 3)
    X, Y = grid(bx0, by0, bx1, by1)
    gm = sdf.sd(glyph, X, Y) > 1.3
    out = {}
    for stn, a in arts.items():
        dm = (sdf.sd(disc, X, Y) < -0.6) & gm
        paint = bevel_fit(a[by0:by1, bx0:bx1], disc, dm, bx0, by0, r, [0.0, 0.05, 0.12, 0.25, 0.45, 0.7, 1.0])
        g = a[by0:by1, bx0:bx1][sdf.sd(glyph, X, Y) < -1.0]
        grgb = [round(float(v), 1) for v in np.median(g, axis=0)]
        out[stn] = {"box": [bx0, by0, bx1, by1], "layers": [{"shape": disc, "fill": paint, "soft": 0.6},
                                                            {"shape": glyph, "fill": grgb, "clip": disc}]}
    return out, (round(cx, 2), round(cy, 2), round(r, 2), round(res, 3))


def bubble_button(idx, pal, art, cell, hint, glyph, state="blue", socket_art=None, near_k=1.05, like=None):
    """A glass bubble measured like the title menu's (title.measure_bubble: a circle fitted to the rim's outermost dark
    pixels near the hint; title.measure_body), painted with the current recipes.STYLE (Load Game's measured glass after
    load.setup()), the authored glyph, and the socket ring measured on socket_art when given."""
    import title
    import recipes
    k0 = title.NEAR_K
    title.NEAR_K = near_k
    cx, cy, r, res = title.measure_bubble(idx, pal, cell, near=hint)
    title.NEAR_K = k0
    body = title.measure_body(art, cx, cy, r)
    if body is not None and body[2] > r - 1.1:
        # on a small bubble the body fit reaches the rim; the art's navy rim is about 1.5 px there (dumps)
        body = [body[0], body[1], round(r - 1.1, 2)]
    if like is not None:
        # the same bubble drawn again (a hover cell): its own centre, the other state's radius and body offset (a second
        # fit on the yellow cell's softer rim gave a body larger than its rim)
        lc, lb = like["c"], like["body"]
        r = lc[2]
        body = [round(cx + lb[0] - lc[0], 2), round(cy + lb[1] - lc[1], 2), lb[2]]
    import copy
    st = copy.deepcopy(recipes.STYLE["bubble"])
    item = recipes.bubble(cx, cy, r, state, glyph, style=st, body=body)
    item = __import__("json").loads(__import__("json").dumps(item))
    info = {"c": [round(cx, 2), round(cy, 2), round(r, 2)], "body": body}
    if socket_art is not None:
        lay, sinfo = socket(socket_art, cx, cy, r)
        item["layers"] = lay + item["layers"]
        info["socket"] = sinfo
        m = sinfo["R"] + 2.5
        item["box"] = [min(item["box"][0], cx - m), min(item["box"][1], cy - m), max(item["box"][2], cx + m),
                       max(item["box"][3], cy + m)]
    return item, info


def moved_bubble(item, art, info, dx, dy, state, glyph):
    """The same bubble in another state drawn dx, dy away (a hover cell): Load Game's glass in that state's paint, the
    glyph moved with it, the socket measured again on the art at the new place."""
    import copy
    import recipes
    cx, cy, r = info["c"]
    b = info["body"]
    st = copy.deepcopy(recipes.STYLE["bubble"])
    it = recipes.bubble(cx + dx, cy + dy, r, state, {"t": "move", "of": glyph, "d": [dx, dy]}, style=st,
                        body=[b[0] + dx, b[1] + dy, b[2]])
    it = __import__("json").loads(__import__("json").dumps(it))
    if "socket" in info:
        lay, sinfo = socket(art, cx + dx, cy + dy, r)
        it["layers"] = lay + it["layers"]
        m = sinfo["R"] + 2.5
        it["box"] = [cx + dx - m, cy + dy - m, cx + dx + m, cy + dy + m]
    return it
