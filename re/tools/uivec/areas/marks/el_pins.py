"""TacksandArrow.pcx: the map pins (5 colours x 4 cells) and the magenta arrow (2 cells).

Pin, read from the enlarged cells (read/tack-rRcC.png): every cell is an opaque black square holding a glossy round pin head
(about 12 px across, clipped where it runs past the cell's edge) and a short 2 px dark blue-grey needle stub sticking out
of the head: down-right (cell 1), up-right (cell 2), down-left (cell 3), up-left (cell 4, a wider cell with the head nearer
the middle). The head is a sphere lit from one point: drawn as a circle with a radial paint centred on its highlight.
Numbers: the head circle is fitted to the head's outline in each cell, the needle is a line fitted through the stub's
pixels, and the radial paint's colour stops are measured per colour over its four cells (fit.py style: numbers only).
"""
import numpy as np

import common as C
import fit

COLOURS = ("grey", "yellow", "red", "white", "green")


def cell_rect(r, c):
    """The opaque bounding box of a cell (sheet coordinates)."""
    x, y = 50 * c, 50 * r
    _, m = C.crop(C.TA, x, y, 25, 20)
    ys, xs = np.nonzero(m)
    return (int(x + xs.min()), int(y + ys.min()), int(xs.max() - xs.min() + 1), int(ys.max() - ys.min() + 1))


def _masks(art):
    L = fit.lum(art)
    head = L > 52
    # the needle: dim blue-grey pixels (blue above red) outside the head
    needle = (L > 22) & (art[..., 2] > art[..., 0] + 6) & ~head
    return head, needle


def measure(r, c):
    x, y, w, h = cell_rect(r, c)
    art, m = C.crop(C.TA, x, y, w, h)
    head, needle = _masks(art)
    # largest blob of head pixels (drop stray bright pixels of the needle)
    pts = C.fit.edge_points(head)
    inner = (pts[:, 0] > 0.6) & (pts[:, 0] < w - 0.6) & (pts[:, 1] > 0.6) & (pts[:, 1] < h - 0.6)   # not the cell's own edge
    cx, cy, rr, res = fit.fit_circle(pts[inner], trim=1.2)
    # the highlight: luminance-weighted centroid of the brightest head pixels
    L = fit.lum(art)
    jj, ii = np.mgrid[0:h, 0:w]
    sel = head & (L >= np.percentile(L[head], 88))
    hx, hy = float((ii[sel] + 0.5).mean()), float((jj[sel] + 0.5).mean())
    # needle: the pixels outside the head circle + 0.3
    out = needle & (np.hypot(ii + 0.5 - cx, jj + 0.5 - cy) > rr + 0.3)
    p = np.c_[ii[out] + 0.5, jj[out] + 0.5]
    if len(p) >= 2:
        mu = p.mean(0)
        u, s, vt = np.linalg.svd(p - mu)
        d = vt[0]
        t = (p - mu) @ d
        a, b = mu + d * t.min(), mu + d * t.max()
        if np.hypot(*(a - [cx, cy])) > np.hypot(*(b - [cx, cy])):
            a, b = b, a
        nd = (b - a) / max(np.hypot(*(b - a)), 1e-9)
        a = np.array([cx, cy]) + nd * (rr - 0.5)            # start the stub inside the head's rim
        b = b + nd * 0.5
        needle_seg = [[round(float(a[0]), 2), round(float(a[1]), 2)], [round(float(b[0]), 2), round(float(b[1]), 2)]]
        ncol = [int(v) for v in np.median(art[out], axis=0)]
    else:
        needle_seg, ncol = None, None
    return dict(rect=(x, y, w, h), c=[round(cx, 2), round(cy, 2)], r=round(rr, 2), hl=[round(hx, 2), round(hy, 2)],
                needle=needle_seg, ncol=ncol)


def measure_paint(row):
    """Radial colour stops for one colour: over its 4 cells, colour vs t = |p - highlight| / (r + |highlight - centre|)."""
    T, Cs = [], []
    for c in range(4):
        M = measure(row, c)
        x, y, w, h = M["rect"]
        art, _ = C.crop(C.TA, x, y, w, h)
        jj, ii = np.mgrid[0:h, 0:w]
        R = M["r"] + np.hypot(M["hl"][0] - M["c"][0], M["hl"][1] - M["c"][1])
        inside = np.hypot(ii + 0.5 - M["c"][0], jj + 0.5 - M["c"][1]) < M["r"] - 0.2
        t = np.hypot(ii + 0.5 - M["hl"][0], jj + 0.5 - M["hl"][1]) / R
        T.append(t[inside])
        Cs.append(art[inside])
    T, Cs = np.concatenate(T), np.concatenate(Cs)
    stops = []
    for t0 in (0.0, 0.2, 0.4, 0.6, 0.8, 1.0, 1.15):
        k = np.abs(T - t0) < 0.12
        if k.sum() < 3:
            continue
        stops.append([t0, [int(v) for v in np.median(Cs[k], axis=0)]])
    return stops


_paint = {}
_needle = {}


def needle_vec(c):
    """The needle stub of cell position c, shared by the five colours: median over colours of the stub's far end relative to
    the head's centre."""
    if c not in _needle:
        v = []
        for row in range(5):
            M = measure(row, c)
            if M["needle"]:
                v.append([M["needle"][1][0] - M["c"][0], M["needle"][1][1] - M["c"][1]])
        _needle[c] = np.median(np.array(v), axis=0)
    return _needle[c]


def pin_scene(row, c):
    if row not in _paint:
        _paint[row] = measure_paint(row)
    M = measure(row, c)
    x, y, w, h = M["rect"]
    R = M["r"] + np.hypot(M["hl"][0] - M["c"][0], M["hl"][1] - M["c"][1])
    cellbox = {"t": "rrect", "c": [w / 2, h / 2], "half": [w / 2, h / 2]}
    L = [{"shape": cellbox, "fill": [0, 0, 0]}]
    v = needle_vec(c)
    n = v / np.hypot(*v)
    a = [round(float(M["c"][0] + n[0] * (M["r"] - 0.5)), 2), round(float(M["c"][1] + n[1] * (M["r"] - 0.5)), 2)]
    b = [round(float(M["c"][0] + v[0]), 2), round(float(M["c"][1] + v[1]), 2)]
    L.append({"shape": C.pline([a, b], 1.5, w1=1.0), "fill": [36, 36, 54], "clip": cellbox})
    L.append({"shape": {"t": "circle", "c": M["c"], "r": M["r"]},
              "fill": {"radial": {"c": M["hl"], "r": round(float(R), 2), "stops": _paint[row]}}, "clip": cellbox})
    return [{"box": [-1, -1, w + 1, h + 1], "layers": L}]


ELEMENTS = {}
for _row, _col in enumerate(COLOURS):
    ELEMENTS[f"tack-{_col}"] = {
        "how": f"{_col} map pin, 4 cells: the cell's black square, a glossy round head (circle fitted to its outline, r ~6, "
               "clipped by the cell edge) painted with a radial gradient centred on its highlight (stops measured on the "
               "art), and a blue-grey needle stub tapering 1.5 to 1 px (one direction per cell position, shared by the colours) (down-right, up-right, down-left, up-left)",
        "unsure": "the black square is opaque in the art cells and is drawn; if the game keys black out, drop layer 0",
        "z": 16,
        "states": [(f"cell {c + 1}", C.TA, cell_rect(_row, c), (lambda r=_row, cc=c: pin_scene(r, cc))) for c in range(4)]}


# ------------------------------------------------------------------------------------------------------- magenta arrow
# Read from read/arrow-a.png and its row runs: a thick arrow pointing down and a little left. The shaft is a band 17 px wide
# running from a square top end (16, 10.6)-(31.2, 17.2) down-left (direction about (-0.39, 1)); the head is a triangle whose
# left barb tips at (0, 28.2), right barb at (30.4, 41.4) and point at (8, 53.2); the barbs' upper edges meet the shaft at
# (7.6, 31) and (23, 37.3). Raised and lit from the left: a ~2 px light pink bevel on the edges facing left, flat magenta
# (222, 0, 255) inside, a darker purple shade on the edges facing right and down, then black. Cell b is the same arrow moved
# (+4, -10) and cut off above row 9 (a bobbing frame).
ARROW_PTS = [[16.0, 10.6], [31.2, 17.2], [23.0, 37.3], [30.4, 41.4], [8.0, 53.2], [0.0, 28.2], [7.6, 31.0]]
ARROW_R = [1.5, 2.0, 0.6, 2.0, 1.6, 1.4, 0.6]          # corner radii: the art's corners are soft, the barbs' inner corners tight
ARROW = {"t": "poly", "rings": [C.rounded(ARROW_PTS, ARROW_R)]}
_Lx, _Ly = -0.99, -0.12          # toward the light: left-facing edges light, the top end and the right edges dark


def _bevel_magenta():
    lit, dark, body = np.array([247, 182, 255.0]), np.array([125, 0, 145.0]), [222, 0, 255]
    mid, half = (lit + dark) / 2, (lit - dark) / 2
    edge = [[round(float(v), 1) for v in mid], [round(float(v), 1) for v in half * _Lx], [round(float(v), 1) for v in half * _Ly],
            [0, 0, 0], [0, 0, 0]]
    return {"bevel": {"L": 1.0, "stops": [0.0, 2.0, 3.4], "coef": [edge, edge, [body, [0, 0, 0], [0, 0, 0], [0, 0, 0], [0, 0, 0]]]}}


def arrow_scene(dx=0.0, dy=0.0, top=None):
    cellbox = {"t": "rrect", "c": [23, 27], "half": [23, 27]}
    shape = {"t": "move", "of": ARROW, "d": [dx, dy]} if (dx or dy) else ARROW
    clip = cellbox if top is None else {"t": "inter", "of": [cellbox, {"t": "half", "p": [0, top], "n": [0, -1]}]}
    return [{"box": [-1, -1, 47, 55], "layers": [{"shape": cellbox, "fill": [0, 0, 0]},
                                                {"shape": shape, "fill": _bevel_magenta(), "clip": clip}]}]


ELEMENTS["magenta-arrow"] = {
    "how": "thick arrow pointing down-left: one 7-point polygon with rounded corners (square-ended 17 px shaft from the top right, triangular head "
           "with two barbs) on the cell's black square, painted as a raised bevel lit from the left (light pink 2 px band on "
           "left-facing edges, flat magenta inside, dark purple on right-facing edges); cell b = same arrow moved (+4, -10), "
           "cut off above row 9",
    "unsure": "the dark side's shade in the art is ~4 px wide against the lit side's 2 px; one bevel width serves both",
    "z": 7,
    "states": [("a", C.TA, (0, 250, 46, 54), lambda: arrow_scene()), ("b", C.TA, (50, 250, 46, 54), lambda: arrow_scene(4, -10, 9))]}
