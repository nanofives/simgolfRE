"""courseinfo.pcx loose medals (cash, fun, skill), two cells each.

Read (read/course-*.png, 12-20x): a blue glass bubble, ~31 px: a dark navy rim 2-3 px wide, a body lit from the upper left
(pale lavender) to the lower right (slate blue), and a glyph ENGRAVED in navy. Cell a (31 px) is the lit state: a soft white
glow sits behind the glyph. Cell b (32 px) is the plain state: no glow, the body a little darker, and a near-black drop
shadow one pixel down-right of the bubble.

Numbers: the bubble's circle is fitted to its outline on the upper-left half (the half without the drop shadow); the rim's
colour is the median of the rim band; the body is sdf's bevel paint (depth x facing) whose coefficients are fitted by least
squares to the body pixels away from the glyph and its glow (the same model accept.py measures for the title bubbles). The
glyphs are authored (read off the grids in read/), not traced.
"""
import numpy as np

import common as C
import fit
import sdf

STOPS = [0.0, 0.1, 0.3, 0.7]          # beyond 0.7 the glyph covers the body: no data, so the paint is flat there
CELLS = {"cash": ((350, 69, 31, 31), (450, 69, 32, 32)), "fun": ((350, 169, 31, 32), (450, 169, 32, 32)),
         "skill": ((350, 269, 31, 31), (450, 269, 32, 32))}
NAVY = [24, 33, 66]


def outer_circle(art, m):
    pts = fit.edge_points(m)
    h, w = m.shape
    c0 = np.array([w / 2, h / 2])
    ang = np.degrees(np.arctan2(pts[:, 1] - c0[1], pts[:, 0] - c0[0]))
    keep = (ang < -40) | (ang > 130)                    # the upper-left half: no drop shadow there
    cx, cy, r, res = fit.fit_circle(pts[keep], trim=1.0)
    return cx, cy, r


def rim_width(art, cx, cy, r):
    """Median over upper-left rays of the depth at which the colour leaves the navy rim (lum > 115)."""
    L = fit.lum(art)
    ws = []
    for a in np.deg2rad(np.arange(140, 320, 6)):
        for k in np.arange(0.3, 8, 0.25):
            x, y = cx + (r - k) * np.cos(a), cy + (r - k) * np.sin(a)
            if L[int(y), int(x)] > 115:
                ws.append(k)
                break
    return float(np.median(ws))


def fit_bevel(art, mask, S, L, stops=STOPS, ridge=1e-3):
    """Least-squares coefficients of sdf's bevel paint on shape S (depth scale L) over the pixels in mask."""
    h, w = mask.shape
    jj, ii = np.mgrid[0:h, 0:w]
    x, y = ii[mask] + 0.5, jj[mask] + 0.5
    d = sdf.sd(S, x, y)
    nx, ny = sdf.normal(S, x, y, d)
    T = sdf.bevel_terms(-d / L, nx, ny, stops)
    A = np.stack([t for row in T for t in row], axis=1)
    coef = []
    for ch in range(3):
        b = art[mask][:, ch]
        sol = np.linalg.solve(A.T @ A + ridge * np.eye(A.shape[1]), A.T @ b)
        coef.append(sol)
    coef = np.array(coef).T.reshape(len(stops), 5, 3)
    return [[[round(float(v), 2) for v in coef[k, hh]] for hh in range(5)] for k in range(len(stops))]


# ------------------------------------------------------------------------------------------------------------- glyphs
def glyph_cash():
    """Dollar sign: a 2.2 px S (top terminal at the right curling down, bottom terminal at the left) and two 1.2 px
    vertical bars, rows 6-24, a pixel beyond the S above and below."""
    S = C.pline([[20.2, 12.8], [19.6, 10.8], [15.0, 10.4], [11.2, 11.0], [10.3, 12.8], [11.6, 14.6], [15.0, 15.0], [19.0, 15.6],
                 [20.5, 17.5], [19.6, 19.7], [15.0, 20.3], [11.2, 19.8], [10.0, 18.2]], 2.3, smooth=2)
    return {"t": "union", "of": [S, C.bar([12.6, 6.0], [12.6, 25.0], 1.3), C.bar([17.4, 6.0], [17.4, 25.0], 1.3)]}


def glyph_fun():
    """Laughing face (the bubble is the face): two short slanted eye strokes squeezed shut, and a huge open mouth: a lip
    line from the upper left slanting down then running right, the mouth's outline rounding down to the chin, and a dark
    band across it (upper teeth above, tongue below)."""
    eyes = [C.pline([[11.8, 6.2], [13.8, 9.0]], 1.4), C.pline([[18.8, 7.6], [20.8, 10.0]], 1.4)]
    lip = C.pline([[9.4, 15.0], [9.0, 12.4], [9.2, 10.0], [13.4, 12.5], [23.8, 12.5], [22.6, 16.4]], 1.4)
    chin = C.pline([[10.6, 17.2], [12.6, 21.4], [15.6, 23.6], [19.2, 22.2], [21.2, 18.8], [22.2, 17.0]], 1.4, smooth=1)
    band = C.pline([[9.8, 16.4], [22.6, 16.4]], 2.8)
    mouth = {"t": "union", "of": [lip, chin]}
    return {"t": "union", "of": eyes + [mouth, band]}


def glyph_skill():
    """Read as a golf ball on a tee: a 1.3 px ring (the ball) over a tee that narrows to a short stem, on a ground line."""
    ball = {"t": "outline", "of": {"t": "circle", "c": [13.0, 11.2], "r": 5.0}, "w": 1.5}
    tee = C.pline([[9.6, 16.2], [14.0, 18.4], [16.0, 18.6], [20.8, 15.6]], 1.6, smooth=1)
    stem = C.bar([15.0, 18.4], [15.0, 22.0], 1.8)
    ground = C.pline([[10.2, 22.6], [19.8, 22.6]], 1.6)
    return {"t": "union", "of": [ball, tee, stem, ground]}


GLYPHS = {"cash": glyph_cash, "fun": glyph_fun, "skill": glyph_skill}


def medal_scene(kind, state):
    x, y, w, h = CELLS[kind][0 if state == "a" else 1]
    art, m = C.crop(C.CI, x, y, w, h)
    cx, cy, r = outer_circle(art, m)
    rw = rim_width(art, cx, cy, r) - 0.5          # the ray samples floor to the pixel: half a pixel too deep
    jj, ii = np.mgrid[0:h, 0:w]
    dist = np.hypot(ii + 0.5 - cx, jj + 0.5 - cy)
    rim_px = m & (dist < r - 0.6) & (dist > r - rw + 0.4)
    outer = {"t": "circle", "c": [round(cx, 2), round(cy, 2)], "r": round(r, 2)}
    rim_paint = fit_bevel(art, rim_px, outer, rw, stops=[0.0])          # one depth: a colour plus its facing terms
    body = {"t": "circle", "c": [round(cx, 2), round(cy, 2)], "r": round(r - rw, 2)}
    G = GLYPHS[kind]()
    gd = sdf.sd(G, ii + 0.5, jj + 0.5)
    fitmask = m & (dist < r - rw - 0.3) & (gd > (3.2 if state == "a" else 1.2))
    shade = fit_bevel(art, fitmask, body, r - rw)
    L = []
    if state == "b":
        L.append({"shape": {"t": "circle", "c": [cx + 1, cy + 1], "r": r}, "fill": [12, 13, 18]})      # drop shadow
    L.append({"shape": outer, "fill": {"bevel": {"L": round(rw, 2), "stops": [0.0], "coef": rim_paint}}})
    L.append({"shape": body, "fill": {"bevel": {"L": round(r - rw, 2), "stops": STOPS, "coef": shade}}})
    if state == "a":
        L.append({"shape": {"t": "grow", "of": G, "d": 1.9}, "fill": [244, 244, 255], "soft": 1.4, "alpha": 0.95, "clip": body})
    L.append({"shape": G, "fill": NAVY, "clip": body})
    return [{"box": [-1, -1, w + 2, h + 2], "layers": L}]


HOW = {"cash": "dollar sign: a 2.3 px S (curled terminals) and two 1.3 px vertical bars through it",
       "fun": "laughing face (the bubble is the face): two short slanted eye strokes squeezed shut and a huge open mouth "
              "(lip line slanting then running right, outline rounding to the chin, a dark band across it)",
       "skill": "golf ball on a tee (read): a ring for the ball, a tee narrowing to a stem, a ground line"}
UNSURE = {
    "skill": "BLOCKED: glyph not identified after two reads. Under a ring (rows 6-16) the art has a short bar under the ring and "
             "two strokes slanting down-left (from x 21 and x 14 to x 15 and x 7) plus a blob at the lower left. Try 1, a golf "
             "ball on a tee, misses the right-hand stroke; a stick-figure reading (ring = head) is not supported by the pixels. "
             "The bubble itself (rim, body, glow, shadow) is done; the image shows the try-1 glyph",
    "fun": "the mouth's lower half (tongue / lower teeth) is left to the glow in state a, as in the art"}
ELEMENTS = {}
for _k in CELLS:
    ELEMENTS[f"course-{_k}-medal"] = {
        "how": "blue glass bubble: circle fitted to its outline, navy rim (colour + facing terms fitted, lighter on the lit side), body = bevel paint fitted by "
               "least squares; glyph engraved in navy: " + HOW[_k] + "; cell a adds a soft white glow (glyph grown 1.9 px), "
               "cell b a near-black drop shadow one pixel down-right",
        "unsure": UNSURE.get(_k, ""),
        "status": "blocked" if _k == "skill" else "done",
        "z": 10,
        "states": [("a", C.CI, CELLS[_k][0], (lambda k=_k: medal_scene(k, "a"))),
                   ("b", C.CI, CELLS[_k][1], (lambda k=_k: medal_scene(k, "b")))]}
