"""The report family's pieces as recipes (written once, instanced per sheet), plus the measurements that place them.

  slab(outline, holes)      the raised lavender slab: an rpoly minus its sunken windows (cream bands, table wells)
  pill(box) / oval(box)     a sunken capsule / ellipse: navy line + inner shade + flat fill (one bevel paint per fill)
  plate(...)                the sunken cream title plate with a circular arch on top
  wire(...)                 navy ink: polyline with rounded elbows, or a "(" bracket (half circle), 1.7 px
  every ink piece also gets a SHADOW: the same shape moved down-right, dark, soft, at low alpha (measured)

Measurement here reads line CENTRES on the art (darkness-weighted, linecentre.py) and puts the shape's outer edge half a
line width outside them; the line's own look (thin on top, thick below, the shade inside) is in the fitted bevel paint.
"""
import numpy as np

import common as C
import slabkit as K

LINE_W = 1.7           # navy line width (px), read on Hole Stats pills / wires
NAVY = [32.0, 42.0, 75.0]


def lum(v):
    return C.fit.lum(np.asarray(v, float))


def line_centre(raw, mode, k, lo, hi, thr=100.0, first_from=None):
    """Darkness-weighted centre of the dark line crossed by the scan lo..hi at column (mode v) / row (mode h) k.
    first_from "lo" / "hi": keep only the first dark run met when walking from that end (a scan out of a pill must not
    reach the next pill's line)."""
    v = raw[lo:hi, k] if mode == "v" else raw[k, lo:hi]
    w = np.clip(thr - lum(v), 0, None)
    if first_from:
        idx = range(len(w)) if first_from == "lo" else range(len(w) - 1, -1, -1)
        keep = np.zeros(len(w), bool)
        seen = False
        for i in idx:
            if w[i] > 0:
                keep[i] = True
                seen = True
            elif seen:
                break
        w = np.where(keep, w, 0)
    if w.sum() < 15:
        return None
    return float((w * (np.arange(lo, hi) + 0.5)).sum() / w.sum())


def line_profile(raw, mode, k, lo, hi, first_from, thr=110.0):
    """(centre, width) of the first dark line met walking from `first_from` ("lo" / "hi") along the scan: the run of
    pixels darker than thr, each pixel's coverage read against its neighbour just outside the run on its own side of the
    darkest pixel ((L_nb - L) / (L_nb - L_darkest), clipped to 0..1); width = summed coverage, centre = coverage-weighted."""
    v = raw[lo:hi, k] if mode == "v" else raw[k, lo:hi]
    L = lum(v)
    n = len(L)
    idx = list(range(n)) if first_from == "lo" else list(range(n - 1, -1, -1))
    run = []
    for i in idx:
        if L[i] < thr:
            run.append(i)
        elif run:
            break
    if not run:
        return None, None
    a, b = min(run), max(run)
    dk = a + int(np.argmin(L[a:b + 1]))
    Ld = L[dk]
    cov = {}
    for i in range(a, b + 1):
        if i == dk:
            cov[i] = 1.0
            continue
        nb = a - 1 if i < dk else b + 1
        Ln = L[nb] if 0 <= nb < n else 255.0
        cov[i] = float(np.clip((Ln - L[i]) / max(Ln - Ld, 1.0), 0, 1))
    wsum = sum(cov.values())
    c = sum((lo + i + 0.5) * w for i, w in cov.items()) / wsum
    return float(c), float(wsum)


# ------------------------------------------------------------------------------------------------------------ geometry
def fill_components(raw, alpha, colour, min_area=40):
    import boxes
    key = (raw[..., 0].astype(np.int32) << 16) | (raw[..., 1].astype(np.int32) << 8) | raw[..., 2].astype(np.int32)
    v = (int(colour[0]) << 16) | (int(colour[1]) << 8) | int(colour[2])
    out = []
    for comp in boxes.components((key == v) & (alpha > 0.99), min_area):
        j0, i0 = comp.min(0)
        j1, i1 = comp.max(0) + 1
        out.append((int(i0), int(j0), int(i1), int(j1), len(comp), comp))
    return out


def measure_pill(raw, fb, reach=7, off=3, square=False):
    """Navy line centres [left, top, right, bottom] of the sunken pill whose flat fill has bounding box fb = (x0, y0, x1, y1).
    Each side is scanned on two lines `off` px either side of its middle (a wire stub often meets a pill in the middle of
    a side), keeping the first dark run met from the fill; then the offset is undone for the pill's curvature (capsule
    ends are half circles, ovals are ellipses). Returns (centres, kind)."""
    x0, y0, x1, y1 = fb
    cx, cy = (x0 + x1) / 2, (y0 + y1) / 2

    widths = {}

    def side(mode, ks, lo, hi, ff, name):
        vals = [line_profile(raw, mode, int(k), lo, hi, ff) for k in ks]
        vals = [v for v in vals if v[0] is not None]
        if not vals:
            return None
        widths[name] = float(np.mean([v[1] for v in vals]))
        return float(np.mean([v[0] for v in vals]))
    cols = (cx - off, cx + off - 1) if (x1 - x0) > 2 * off + 2 else (cx,)
    rows = (cy - off, cy + off - 1) if (y1 - y0) > 2 * off + 2 else (cy,)
    t = side("v", cols, y0 - reach, y0, "hi", "t")
    b = side("v", cols, y1, y1 + reach, "lo", "b")
    l = side("h", rows, x0 - reach, x0, "hi", "l")
    r = side("h", rows, x1, x1 + reach, "lo", "r")
    if square and sum(v is None for v in (t, b, l, r)) == 1:
        # a box cut by a piece boundary: its line on that side lies past the cut, 2 px out from the fill (as on the
        # boxes' other sides)
        t = y0 - 2.0 if t is None else t
        b = y1 + 2.0 if b is None else b
        l = x0 - 2.0 if l is None else l
        r = x1 + 2.0 if r is None else r
        for k in "tblr":
            widths.setdefault(k, float(np.mean(list(widths.values()))))
    if None in (t, b, l, r):
        return None, None
    measure_pill.last_widths = widths
    w, h = r - l, b - t
    kind = "box" if square else ("ellipse" if (w - h) < 0.6 * h else "capsule")
    dy = (cols[1] + 0.5 - (cols[0] + 0.5)) / 2 if len(cols) == 2 else 0.0
    dx_ = (rows[1] + 0.5 - (rows[0] + 0.5)) / 2 if len(rows) == 2 else 0.0
    if kind == "ellipse":
        e = ellipse_ring(raw, ((l + r) / 2, (t + b) / 2), max(w, h) / 2 + reach)
        if e is not None:
            ccx, ccy, rx, ry = e
            l, r, t, b = ccx - rx, ccx + rx, ccy - ry, ccy + ry
    elif not square:
        rr = h / 2
        k = rr - np.sqrt(max(rr * rr - dx_ * dx_, 0.0))
        l, r = l - k, r + k
    return [l, t, r, b], kind


def ellipse_ring(raw, c, R, n=48, thr=100.0):
    """Axis-aligned ellipse through the centre of a navy ring around c: rays every 360/n degrees, the first dark run met
    walking out from c (bilinear samples every 0.25 px), its darkness-weighted centre; a 2-pass fit dropping points over
    0.6 px off (rays that ran along a wire stub)."""
    cx, cy = c
    H, W = raw.shape[:2]
    L = lum(raw)
    pts = []
    for k in range(n):
        a = 2 * np.pi * k / n
        ux, uy = np.cos(a), np.sin(a)
        ts = np.arange(1.0, R, 0.25)
        px, py = cx + ux * ts - 0.5, cy + uy * ts - 0.5
        i0, j0 = np.floor(px).astype(int), np.floor(py).astype(int)
        fx, fy = px - i0, py - j0
        ok = (i0 >= 0) & (j0 >= 0) & (i0 + 1 < W) & (j0 + 1 < H)
        if not ok.all():
            ts, i0, j0, fx, fy = ts[ok], i0[ok], j0[ok], fx[ok], fy[ok]
        v = (L[j0, i0] * (1 - fx) * (1 - fy) + L[j0, i0 + 1] * fx * (1 - fy) + L[j0 + 1, i0] * (1 - fx) * fy
             + L[j0 + 1, i0 + 1] * fx * fy)
        w = np.clip(thr - v, 0, None)
        on = np.nonzero(w > 0)[0]
        if len(on) == 0:
            continue
        s0 = on[0]
        s1 = s0
        while s1 + 1 < len(w) and w[s1 + 1] > 0:
            s1 += 1
        if (s1 - s0) * 0.25 > 4.0:      # a run this long is a wire, not the ring
            continue
        tt = float((w[s0:s1 + 1] * ts[s0:s1 + 1]).sum() / w[s0:s1 + 1].sum())
        pts.append((cx + ux * tt, cy + uy * tt))
    if len(pts) < 12:
        return None
    P = np.array(pts)
    # robust: drop the rays that ran to a neighbour's line (residual over 2.5 x the median, at least 0.5 px), 5 rounds
    for _ in range(5):
        ex, ey, rx, ry, _r = C.fit.fit_ellipse(P, rot=0.0)
        res = np.abs(np.hypot((P[:, 0] - ex) / rx, (P[:, 1] - ey) / ry) - 1) * min(rx, ry)
        keep = res <= max(0.5, 2.5 * float(np.median(res)))
        if keep.all() or keep.sum() < 10:
            break
        P = P[keep]
    ex, ey, rx, ry, _r = C.fit.fit_ellipse(P, rot=0.0)
    return float(ex), float(ey), float(rx), float(ry)


def _shape_of_box(bx, kind, r=None):
    x0, y0, x1, y1 = bx
    c = [(x0 + x1) / 2, (y0 + y1) / 2]
    hx, hy = max((x1 - x0) / 2, 0.3), max((y1 - y0) / 2, 0.3)
    if kind == "ellipse":
        return {"t": "ellipse", "c": c, "r": [hx, hy], "rot": 0.0}
    return {"t": "rrect", "c": c, "half": [hx, hy], "r": min(hx, hy) if r is None else max(min(r, hx, hy), 0.0), "rot": 0.0}


def pill_shapes(cen, kind, W, box_r=None):
    """(outer, inner) shapes of a pill from its line centres and the line widths W = {t, b, s} (top, bottom, sides): the
    navy line is the outer shape minus the inner one. kind "box": a rounded rectangle whose fill corner radius is box_r."""
    l, t, r, b = cen
    outer = [l - W["s"] / 2, t - W["t"] / 2, r + W["s"] / 2, b + W["b"] / 2]
    inner = [l + W["s"] / 2, t + W["t"] / 2, r - W["s"] / 2, b - W["b"] / 2]
    if kind == "box":
        rr = box_r or 0.0
        return _shape_of_box(outer, "box", rr + W["s"]), _shape_of_box(inner, "box", rr)
    return _shape_of_box(outer, kind), _shape_of_box(inner, kind)


def ringed(raw, fb, reach=4):
    """True when a dark line meets the fill box fb within `reach` px on all four sides (at the middle of each side)."""
    x0, y0, x1, y1 = fb
    cx, cy = (x0 + x1) // 2, (y0 + y1) // 2
    got = [line_profile(raw, "v", cx, max(y0 - reach, 0), y0, "hi")[0], line_profile(raw, "v", cx, y1, y1 + reach, "lo")[0],
           line_profile(raw, "h", cy, max(x0 - reach, 0), x0, "hi")[0], line_profile(raw, "h", cy, x1, x1 + reach, "lo")[0]]
    return "".join(k for k, g in zip("tblr", got) if g is not None)      # the sides that have a line


def plate_measure(raw, fb):
    """The title plate's line centres around its cream fill box fb: left / right / bottom at the middle of each side, the
    top sampled along x and fitted by a parabola (a shallow circular arch)."""
    x0, y0, x1, y1 = fb
    cy = (y0 + y1) // 2
    cx = (x0 + x1) // 2
    l, wl = line_profile(raw, "h", cy, x0 - 8, x0 + 1, "hi")
    r, wr = line_profile(raw, "h", cy, x1 - 1, x1 + 8, "lo")
    b, wb = line_profile(raw, "v", cx, y1 - 1, y1 + 8, "lo")
    wts = []
    xs, ys = [], []
    for x in np.linspace(x0 + 14, x1 - 14, 25):
        x = int(x)
        c, wt = line_profile(raw, "v", x, max(y0 - 16, 0), y0 + 12, "hi")
        if c is not None:
            xs.append(x + 0.5)
            ys.append(c)
            wts.append(wt)
    xs, ys = np.array(xs), np.array(ys)
    p = np.linalg.lstsq(np.c_[xs * xs, xs, np.ones_like(xs)], ys, rcond=None)[0]
    return {"left": l, "right": r, "bottom": b, "top": [float(v) for v in p],
            "top_res": float(np.abs(np.polyval(p, xs) - ys).max()),
            "widths": {"t": float(np.median(wts)), "b": wb, "s": (wl + wr) / 2}}


def plate_shapes(m, W, r_top=6.0, r_bot=2.0):
    """(outer, inner) of the plate: a quadrilateral whose top edge is a circular arch (sagitta from the fitted top line),
    the corners rounded; the line is W = {t, b, s} wide around the measured centre lines."""
    p = m["top"]
    out = []
    for sgn in (-1, 1):       # -1 outer, +1 inner
        l = m["left"] + sgn * W["s"] / 2
        r = m["right"] - sgn * W["s"] / 2
        b = m["bottom"] + W["b"] / 2 if sgn < 0 else m["bottom"] - W["b"] / 2
        off = -W["t"] / 2 if sgn < 0 else W["t"] / 2
        yl, yr = float(np.polyval(p, l)) + off, float(np.polyval(p, r)) + off
        ym = float(np.polyval(p, (l + r) / 2)) + off
        sag = (yl + yr) / 2 - ym
        rt = r_top + (W["s"] / 2 if sgn < 0 else -W["s"] / 2)
        rb = r_bot + (W["b"] / 2 if sgn < 0 else -W["b"] / 2)
        rt, rb = max(rt, 0.5), max(rb, 0.3)
        out.append({"t": "reports_slabs.rpoly", "verts": [[l, yl, rt, rt], [r, yr, rt, rt], [r, b, rb, rb], [l, b, rb, rb]],
                    "sag": {"0": round(sag, 3)}})
    return out[0], out[1]


def fillet_path(pts, radii, step=1.0):
    """An open polyline whose inner vertices are rounded by circular fillets of the given radii (radius 0 = sharp)."""
    P = [np.asarray(p, float) for p in pts]
    out = [P[0].tolist()]
    for i in range(1, len(P) - 1):
        r = radii[i - 1] if isinstance(radii, (list, tuple)) else radii
        a, v, b = P[i - 1], P[i], P[i + 1]
        din = (v - a) / max(np.hypot(*(v - a)), 1e-9)
        dout = (b - v) / max(np.hypot(*(b - v)), 1e-9)
        if r <= 0:
            out.append(v.tolist())
            continue
        cosang = float(np.clip(-din @ dout, -1, 1))
        half = np.arccos(cosang) / 2                     # half of the angle between the two edges
        t = r / max(np.tan(half), 1e-9)                  # distance from the vertex to each tangent point
        P1, P2 = v - din * t, v + dout * t
        m = max(int(r * 1.5), 6)
        for k in range(m + 1):
            u = k / m
            # circular arc via the rational quadratic Bezier (weight cos(half sweep))
            w = np.sin(half)
            q = ((1 - u) ** 2 * P1 + 2 * u * (1 - u) * w * v + u * u * P2) / ((1 - u) ** 2 + 2 * u * (1 - u) * w + u * u)
            out.append(q.tolist())
    out.append(P[-1].tolist())
    return out


def wire(pts, radii=0.0, w=LINE_W, cap="round"):
    return {"t": "stroke", "pts": fillet_path(pts, radii) if len(pts) > 2 else [list(map(float, p)) for p in pts],
            "w": w, "cap": cap}


def bracket(x, y0, y1, w=LINE_W, side="left"):
    """A "(" (side left) or ")" joining two points on the same vertical: a half circle through them."""
    r = (y1 - y0) / 2
    if side == "left":
        return {"t": "arc", "c": [x, (y0 + y1) / 2], "r": r, "a0": 90.0, "sweep": 180.0, "w": w}
    return {"t": "arc", "c": [x, (y0 + y1) / 2], "r": r, "a0": -90.0, "sweep": 180.0, "w": w}
