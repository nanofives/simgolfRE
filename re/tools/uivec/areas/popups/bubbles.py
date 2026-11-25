"""P41 popups area: the glossy discs (popup-bullet, popup-back-bullet, popup-ok-check, trans-spinner, popup-icon-notice,
pair-ok-button, popup-arrow-glyph).

Each disc is measured on its own art: the circle from the half-level contour of its coverage against the ground (black on
TransPopups' bullet block), then its whole lit surface, rim included, as the toolkit's bevel paint (colour by depth inside the
circle and by direction: the navy rim on the upper left, the dark edge on the lower right and the highlight all come out of
the measurement, paint.fit_bevel, radial directions about the centre, deepest stop without direction). The glyph (check,
arrow, triangles, exclamation) is authored from its pixel grid and drawn over the surface; its pixels are kept out of the
surface measurement.

Usage: py -3.12 bubbles.py [id ...]
"""
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import common as c      # noqa: E402
import paint            # noqa: E402

TP = "TransPopups.pcx"
BLACK = np.array((0.0, 0.0, 0.0))
STOPS = [0.0, 0.04, 0.09, 0.15, 0.23, 0.33, 0.46, 0.62, 0.8, 1.0]
NAVY = [40, 48, 88]


def circle_of(rel, box, ground=BLACK, thr=None):
    """(cx, cy, r) of the disc in box: least-squares circle through the half-level contour of its coverage over the ground."""
    a = c.art(rel, box, ground)
    # how far each pixel is from the ground, in its brightest channel: the navy rim (max channel ~ 98) and a dim disabled disc
    # count as disc as much as the bright body does; half level at 25 levels from the ground
    dist = np.abs(a - np.asarray(ground, float)).max(axis=2)
    cov = np.clip(dist / 50.0, 0, 1) if thr is None else (dist > thr).astype(float)
    pts = []
    for loop in c.fit.contours(cov, 0.5):
        pts += loop
    p = np.array(pts) + np.array(box[:2], float)
    cx, cy, r, res = c.fit.fit_circle(p, trim=1.0)
    return cx, cy, r


def coverage_digits(rel, box, disc, inset=2.5, ground=BLACK):
    """The dark glyph's coverage inside the disc as text (for reading)."""
    a = c.art(rel, box, ground)
    jj, ii = np.mgrid[box[1]:box[3], box[0]:box[2]]
    inside = np.hypot(ii + .5 - disc[0], jj + .5 - disc[1]) < disc[2] - inset
    cov, lo, hi = c.fit.dark_coverage(a, inside)
    out = ["     " + "".join(str(x % 10) for x in range(box[0], box[2]))]
    for j in range(box[3] - box[1]):
        out.append(f"{box[1] + j:4d} " + "".join("." if v < 0.05 else str(min(int(v * 10), 9)) for v in cov[j]))
    return "\n".join(out), cov


def surface(rel, box, disc, glyph_shapes=(), ground=BLACK, stops=STOPS, grow=1.2):
    """The disc's lit surface measured on the art (glyph pixels, grown `grow` px, left out)."""
    cx, cy, r = disc
    a = c.art(rel, box, ground)
    jj, ii = np.mgrid[box[1]:box[3], box[0]:box[2]]
    x, y = ii + 0.5, jj + 0.5
    keep = np.hypot(x - cx, y - cy) < r + 0.5
    for S in glyph_shapes:
        keep &= c.sdf.sd(S, x, y) > grow
    sh = c.circ(cx, cy, r)
    # the first stop sits on the first whole pixel row inside the edge (an edge pixel is half ground, half disc: a stop there
    # learns a colour no pixel has and draws a bright hairline round the disc)
    u0 = 0.6 / r
    st = [u0] + [v for v in stops if v > u0 + 0.02]
    P, used = paint.fit_bevel(sh, a, box[:2], st, r, keep, radial_c=(cx, cy))
    return P


def spot(rel, box, disc, P, glyph_shapes=(), ground=BLACK, min_res=22.0):
    """The specular spot, if the art has one: where the art is lighter than the measured surface by more than min_res levels,
    a soft spot (the toolkit's falloff: alpha fading to 0 at radius rs) of the art's colour there. Returns a layer or None."""
    cx, cy, r = disc
    a = c.art(rel, box, ground)
    jj, ii = np.mgrid[box[1]:box[3], box[0]:box[2]]
    x, y = ii + 0.5, jj + 0.5
    pred = c.sdf.paint(dict(P, bevel=dict(P["bevel"], of=c.circ(cx, cy, r))), x, y)
    res = c.fit.lum(a) - c.fit.lum(pred)
    ok = (np.hypot(x - cx, y - cy) < r - 1.5) & (x < cx + 0.2 * r) & (y < cy + 0.2 * r)     # a specular sits up and left
    for S in glyph_shapes:
        ok &= c.sdf.sd(S, x, y) > 1.0
    res = np.where(ok, res, -1e9)
    k = np.unravel_index(np.argmax(res), res.shape)
    peak = res[k]
    if peak < min_res:
        return None
    blob = res > peak / 2
    w = np.clip(res, 0, None) * blob
    sx, sy = float((w * x).sum() / w.sum()), float((w * y).sum() / w.sum())
    rs = max(1.2, float(np.sqrt(blob.sum() / np.pi)) * 1.8)
    col = [float(v) for v in a[k]]
    base = float(c.fit.lum(pred[k]))
    alpha = float(np.clip(peak / max(c.fit.lum(a[k]) - base + 1e-6, 1.0), 0.2, 1.0))
    return {"shape": c.circ(cx, cy, r), "fill": col, "alpha": round(alpha, 2),
            "falloff": {"c": [round(sx, 2), round(sy, 2)], "r": round(rs, 2), "pow": 1.5}}


def disc_item(box, disc, P, glyph=(), name=""):
    cx, cy, r = disc
    body = c.circ(cx, cy, r)
    Ls = [{"shape": body, "fill": P}]
    for S, col, kw in glyph:
        Lr = {"shape": S, "fill": col, "clip": body}
        Lr.update(kw)
        Ls.append(Lr)
    return c.item(box, Ls, name)


def stroke(*pts, w=1.0, cap="butt"):
    return {"t": "stroke", "pts": [list(p) for p in pts], "w": w, "cap": cap}


def poly(*pts, smooth=0):
    S = {"t": "poly", "rings": [[list(p) for p in pts]]}
    if smooth:
        S["smooth"] = smooth
    return S


def run_states(eid, rel, crops, glyph_fn=None, ground=BLACK, how="", unsure="", z=None, share_disc=False):
    """crops: list of (state, box). glyph_fn(disc) -> [(shape, colour, kw)] in sheet coordinates for that crop."""
    rows, states, stats = [], {}, []
    for st, box in crops:
        disc = circle_of(rel, box, ground)
        glyph = glyph_fn(st, box, disc) if glyph_fn else []
        P = surface(rel, box, disc, [g[0] for g in glyph], ground)
        it = disc_item(box, disc, P, glyph, f"{eid} {st}")
        # the dim (disabled) discs show no specular in the art; a residual found there is the glyph's soft edge
        sp = None if "disabled" in st else spot(rel, box, disc, P, [g[0] for g in glyph], ground)
        if sp:
            it["layers"].insert(1, sp)
        sc = [it]
        mad, w16 = c.score(sc, rel, box, ground)
        e = c.edge_px(sc, rel, box, ground)
        stats.append((st, mad, e, disc))
        rows.append((sc, rel, box, ground))
        states[st] = {"box": list(box), "disc": [round(v, 3) for v in disc], "scene": sc}
    p = c.compare(rows, eid, zoom_k=z)
    c.save_scene(eid, {"id": eid, "sheet": "Interface/" + rel, "states": states})
    mad = float(np.mean([s[1] for s in stats]))
    es = [s[2] for s in stats if s[2] is not None]
    c.record(eid, "done", how, mad, float(np.mean(es)) if es else None, unsure)
    for st, m_, e, d in stats:
        print(f"  {eid:20s} {st:16s} mad {m_:6.2f} edge {e if e is None else round(e, 2)}  disc {tuple(round(v, 2) for v in d)}")
    return p


# ------------------------------------------------------------------------------------------------------------ elements
def bullet():
    crops = [(f"{sz} {s}", (300 + 50 * i, y, 300 + 50 * i + d, y + d)) for sz, y, d in (("large", 186, 40), ("medium", 234, 30),
                                                                                      ("small", 296, 24))
             for i, s in enumerate(("idle", "hover", "disabled"))]
    return run_states("popup-bullet", TP, crops, None, how="a plain glossy disc per size and state: its circle fitted on the "
                      "half-level edge against the black ground, its whole lit surface (thin navy rim on the upper left, orange "
                      "lower edge, yellow body, the highlight) measured as one bevel paint per state", z=4)


GNAVY = [36, 47, 84]


def glyph_shift(rel, box0, disc0, box, disc, ground=BLACK):
    """How far this state's glyph sits from the idle one's: the difference of the dark coverage centroids (inside the disc)."""
    def cen(b, d):
        a = c.art(rel, b, ground)
        jj, ii = np.mgrid[b[1]:b[3], b[0]:b[2]]
        inside = np.hypot(ii + .5 - d[0], jj + .5 - d[1]) < d[2] - 2.5
        cv, _, _ = c.fit.dark_coverage(a, inside)
        cv = np.where(cv > 0.5, cv, 0)
        return (cv * (ii + .5)).sum() / cv.sum(), (cv * (jj + .5)).sum() / cv.sum()
    x0, y0 = cen(box0, disc0)
    x1, y1 = cen(box, disc)
    return round(x1 - x0, 2), round(y1 - y0, 2)


def mvS(S, d):
    return {"t": "move", "of": S, "d": list(d)}


# the back arrow per size, read on the idle cells (column extents of the dark coverage): (shaft end x, y, tip x, shaft width,
# head length, head width). Large: tip (456.0, 207.0), head base x 465.0, head y 198.8-215.0, shaft y 203.0-211.0 to x 474.1.
ARROWS = {"large": (474.1, 207.0, 456.0, 8.0, 9.0, 16.2), "medium": (468.1, 253.0, 455.0, 5.5, 6.2, 11.2),
          "small": (462.0, 308.9, 453.4, 3.8, 4.6, 7.2)}


def back_bullet():
    sizes = (("large", 186, 40), ("medium", 234, 30), ("small", 296, 24))
    crops = [(f"{sz} {s}", (450 + 50 * i, y, 450 + 50 * i + d, y + d)) for sz, y, d in sizes
             for i, s in enumerate(("idle", "hover", "disabled"))]
    idle = {sz: ((450, y, 450 + d, y + d)) for sz, y, d in sizes}

    def glyph(st, box, disc):
        sz = st.split()[0]
        b0 = idle[sz]
        d0 = circle_of(TP, b0)
        dx, dy = glyph_shift(TP, b0, d0, box, disc)
        ax, ay, tx, w, hl, hw = ARROWS[sz]
        # one polygon (the toolkit's arrow is a union of two polygons, which leaves a seam where they touch)
        hx = tx + hl
        S = poly((tx, ay), (hx, ay - hw / 2), (hx, ay - w / 2), (ax, ay - w / 2), (ax, ay + w / 2), (hx, ay + w / 2), (hx, ay + hw / 2))
        col = GNAVY if "disabled" not in st else [44, 55, 94]
        return [(mvS(S, (dx, dy)), col, {})]
    return run_states("popup-back-bullet", TP, crops, glyph, how="the glossy disc (circle fitted on its edge, lit surface "
                      "measured per state) with a navy left arrow: a triangular head and a straight shaft with a flat end, both "
                      "read per size on the idle cells (large: tip x 456, head base x 465, head 16.2 tall, shaft 8 tall to "
                      "x 474.1); the hover and disabled arrows sit where their own dark centroid is", z=4)


def ok_check():
    crops = [(s, (300 + 50 * i, 140, 350 + 50 * i, 190)) for i, s in enumerate(("idle", "hover", "disabled"))]
    b0 = crops[0][1]
    # a thick block check, read row by row on the idle cell: a short arm whose flat top runs y 156.3 from x 305.9 to 311.7,
    # a long arm whose flat top runs y 149.2 from x 317.7 to 323.6, the notch between them down to (314.0, 159.3), a flat
    # bottom y 163.9 from x 311.3 to 317.6; the long arm's right edge bends at (318.6, 158.2) and drops nearly straight
    check = poly((305.9, 156.3), (311.7, 156.3), (313.0, 158.7), (314.0, 159.3), (315.0, 158.2), (317.7, 149.2), (323.6, 149.2),
                 (318.6, 158.2), (317.6, 163.9), (311.3, 163.9), (310.8, 161.0), (309.5, 158.9), (307.5, 157.0))

    def glyph(st, box, disc):
        d0 = circle_of(TP, b0)
        dx, dy = glyph_shift(TP, b0, d0, box, disc)
        return [(mvS(check, (dx, dy)), GNAVY, {})]
    return run_states("popup-ok-check", TP, crops, glyph, how="a glass disc (circle fitted on its edge; its lit surface measured "
                      "per state: blue, yellow, grey) with a thick navy block check read row by row: a short arm with a flat "
                      "top, a long arm with a flat top end, a notch between them and a flat bottom", z=6)


def notice_icon():
    import auto
    rel, box = "PopUpIcons.pcx", (270, 0, 360, 80)
    coin = c.circ(315.14, 39.97, 39.31)            # circle fitted on its edge
    # the groove: a black ring whose edges were fitted separately (inner r 28.9, outer r 32.3 through the dark pixel centres
    # along 60 rays, so the edges sit half a pixel further out / in)
    groove = {"t": "outline", "of": c.circ(315.11, 39.55, 30.62), "w": 4.25}
    face = c.circ(315.09, 39.45, 28.45)
    # the '!' is italic, read as each row's run of black: the bar's right edge a straight line from (325.4, 22.4) to (315.2,
    # 46.6), its left edge 313 at rows 24-30, 312 at rows 31-39, 311 at rows 40-44, 310 at rows 45-46, a rounded slanted top
    # end (y 18.9-22) and a pointed foot at (312.8, 47.9); the dot an ellipse c (310.5, 53.6), r (5.6, 4.0), turned -12 deg
    bar = poly((313.2, 24.0), (314.2, 22.0), (316.5, 20.4), (319.5, 19.3), (322.5, 18.9), (324.8, 19.0), (325.7, 20.2),
               (325.4, 22.4), (315.2, 46.6), (312.8, 47.9), (310.2, 46.8), (310.6, 44.0), (312.0, 33.0), (313.0, 26.0))
    dot = {"t": "ellipse", "c": [310.5, 53.6], "r": [5.6, 4.0], "rot": -12.0}
    lip = stroke((326.4, 21.5), (317.0, 45.0), w=1.0)
    stops = [0.025, 0.06, 0.1, 0.16, 0.25, 0.4, 0.6, 0.8, 1.0]
    parts = [(coin, {"bevel": {"stops": stops, "L": 39.31, "radial": (315.14, 39.97)}}),
             ({"t": "outline", "of": c.circ(315.14, 39.97, 38.75), "w": 1.1}, "flat", {"alpha": 0.9}),
             (groove, "flat"),
             (face, {"bevel": {"stops": [0.02, 0.06, 0.12, 0.22, 0.38, 0.6, 1.0], "L": 28.45, "radial": (315.09, 39.45)}}),
             (bar, "flat"), (dot, "flat"),
             (lip, [255, 236, 120], {"alpha": 0.7})]
    it = auto.measured_item(rel, box, parts, name="popup-icon-notice")
    return auto.deliver("popup-icon-notice", rel, [("idle", box, it)], how="a yellow coin (circle fitted on its edge; its outer "
                        "band lit from the upper left with a dark edge, measured) with a black groove ring 4.2 px wide (both "
                        "edges fitted), a raised inner face lit the same way (bright upper left, orange lower right, measured) and "
                        "a 1 px dark outline round the coin, and a black italic exclamation mark read row by row: a bar with a straight right edge, a rounded slanted top "
                        "and a pointed foot, and an oval dot; a bright lip along the bar's right side", z=4)


if __name__ == "__main__":
    fns = {"popup-bullet": bullet, "popup-back-bullet": back_bullet, "popup-ok-check": ok_check, "popup-icon-notice": notice_icon}
    todo = sys.argv[1:] or list(fns)
    for k, eid in enumerate(todo):
        print(f"[{k + 1}/{len(todo)}] {eid} {fns[eid]()}", flush=True)
