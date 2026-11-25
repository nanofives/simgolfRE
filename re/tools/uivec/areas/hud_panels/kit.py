"""P41 hud_panels: small shape helpers and measured-paint helpers shared by the element builders.

Paint of a glyph part is SAMPLED on the art: the median colour of the art pixels whose centres lie inside the part (deep
enough to be the part's own colour, not its antialiased edge). Never a guessed colour.
"""
import json
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import common as C  # noqa: E402
import sdf  # noqa: E402


# ------------------------------------------------------------------------------------------------------------ shapes
def U(*s):
    return {"t": "union", "of": list(s)}


def line(*pts, w=1.0, cap="butt"):
    return {"t": "stroke", "pts": [list(map(float, p)) for p in pts], "w": w, "cap": cap}


def curve(*pts, w=1.0, cap="round", smooth=2):
    return {"t": "stroke", "pts": [list(map(float, p)) for p in pts], "w": w, "cap": cap, "smooth": smooth}


def poly(*pts, smooth=0):
    s = {"t": "poly", "rings": [[list(map(float, p)) for p in pts]]}
    if smooth:
        s["smooth"] = smooth
    return s


def rect(x0, y0, x1, y1, r=0.0):
    return {"t": "rrect", "c": [(x0 + x1) / 2, (y0 + y1) / 2], "half": [(x1 - x0) / 2, (y1 - y0) / 2], "r": r, "rot": 0.0}


def circle(cx, cy, r):
    return {"t": "circle", "c": [cx, cy], "r": r}


def ellipse(cx, cy, rx, ry, rot=0.0):
    return {"t": "ellipse", "c": [cx, cy], "r": [rx, ry], "rot": rot}


def sub(a, b):
    return {"t": "sub", "a": a, "b": b}


def inter(*s):
    return {"t": "inter", "of": list(s)}


def outline(s, w):
    return {"t": "outline", "of": s, "w": w}


def move(s, dx, dy):
    return {"t": "move", "of": s, "d": [dx, dy]} if (dx or dy) else s


def shift(node, dx, dy):
    """A copy of a shape tree with every point moved (keeps the JSON plain: no 'move' wrappers in the output)."""
    if not (dx or dy):
        return json.loads(json.dumps(node))
    if isinstance(node, list):
        return [shift(n, dx, dy) for n in node]
    if not isinstance(node, dict):
        return node
    out = {}
    for k, v in node.items():
        if k in ("c", "a", "b", "p") and isinstance(v, list) and len(v) == 2 and all(isinstance(t, (int, float)) for t in v):
            out[k] = [v[0] + dx, v[1] + dy]
        elif k == "p" and isinstance(v, list) and v and isinstance(v[0], list):
            out[k] = [[q[0] + dx, q[1] + dy] for q in v]
        elif k == "pts":
            out[k] = [[q[0] + dx, q[1] + dy] + list(q[2:]) for q in v]
        elif k == "box" and isinstance(v, list) and len(v) == 4:
            out[k] = [v[0] + dx, v[1] + dy, v[2] + dx, v[3] + dy]
        elif k == "rings":
            out[k] = [[[q[0] + dx, q[1] + dy] for q in r] for r in v]
        elif k == "x" and node.get("t") == "lathe":
            out[k] = v + dx
        elif k == "prof" and node.get("t") == "lathe":
            out[k] = [[q[0] + dy, q[1]] for q in v]
        elif k == "c" and node.get("t") == "half":
            out[k] = v
        elif isinstance(v, (dict, list)):
            out[k] = shift(v, dx, dy)
        else:
            out[k] = v
    if node.get("t") == "half":
        out["p"] = [node["p"][0] + dx, node["p"][1] + dy]
    return out


# ------------------------------------------------------------------------------------------------------------ paint
def grid(box):
    x0, y0, x1, y1 = (int(np.floor(box[0])), int(np.floor(box[1])), int(np.ceil(box[2])), int(np.ceil(box[3])))
    jj, ii = np.mgrid[y0:y1, x0:x1]
    return x0, y0, x1, y1, ii + 0.5, jj + 0.5


def bbox(shape, pad=2.0):
    """A generous box of a shape tree from its points."""
    xs, ys = [], []

    def walk(n):
        if isinstance(n, list):
            for q in n:
                walk(q)
            return
        if not isinstance(n, dict):
            return
        t = n.get("t")
        if t in ("circle",):
            xs.extend([n["c"][0] - n["r"], n["c"][0] + n["r"]]); ys.extend([n["c"][1] - n["r"], n["c"][1] + n["r"]])
        elif t == "ellipse":
            m = max(n["r"])
            xs.extend([n["c"][0] - m, n["c"][0] + m]); ys.extend([n["c"][1] - m, n["c"][1] + m])
        elif t == "rrect":
            m = max(n["half"])
            xs.extend([n["c"][0] - m, n["c"][0] + m]); ys.extend([n["c"][1] - m, n["c"][1] + m])
        elif t == "arc":
            xs.extend([n["c"][0] - n["r"], n["c"][0] + n["r"]]); ys.extend([n["c"][1] - n["r"], n["c"][1] + n["r"]])
        elif t == "lathe":
            for yy, hh in n["prof"]:
                xs.extend([n["x"] - hh, n["x"] + hh]); ys.append(yy)
        for k in ("pts",):
            if k in n:
                for q in n[k]:
                    xs.append(q[0]); ys.append(q[1])
        if "rings" in n:
            for r in n["rings"]:
                for q in r:
                    xs.append(q[0]); ys.append(q[1])
        if t == "seg" or t == "arrow":
            xs.extend([n["a"][0], n["b"][0]]); ys.extend([n["a"][1], n["b"][1]])
        if t == "qstroke":
            for q in n["p"]:
                xs.append(q[0]); ys.append(q[1])
        for k in ("of", "a", "b"):
            if k in n and isinstance(n[k], (dict, list)) and not (k in ("a", "b") and t in ("seg", "arrow")):
                walk(n[k])
    walk(shape)
    return [min(xs) - pad, min(ys) - pad, max(xs) + pad, max(ys) + pad]


def sample(key, shape, depth=0.3, exclude=None, stat="median"):
    """The art's colour inside a shape: pixels whose centre is at least `depth` inside it (falling back to the deepest
    pixels when none is that deep). exclude: a shape whose pixels (grown 0.5) are left out."""
    rgb, m = C.sheet(key)
    x0, y0, x1, y1, x, y = grid(bbox(shape))
    x0c, y0c = max(x0, 0), max(y0, 0)
    H, W = m.shape
    x1c, y1c = min(x1, W), min(y1, H)
    x, y = x[y0c - y0:y1c - y0, x0c - x0:x1c - x0], y[y0c - y0:y1c - y0, x0c - x0:x1c - x0]
    d = sdf.sd(shape, x, y)
    a = rgb[y0c:y1c, x0c:x1c]
    ok = m[y0c:y1c, x0c:x1c].copy()
    if exclude is not None:
        ok &= sdf.sd(exclude, x, y) > 0.5
    sel = ok & (d < -depth)
    if sel.sum() < 1:
        dd = np.where(ok, d, 1e9)
        sel = ok & (dd <= dd.min() + 0.25)
    v = a[sel]
    return [round(float(t), 1) for t in (np.median(v, axis=0) if stat == "median" else v.mean(axis=0))]


def lay(shape, fill, **kw):
    L = {"shape": shape, "fill": fill}
    L.update(kw)
    return L


def item(layers, box=None, pad=2.0):
    if box is None:
        bs = [bbox(L["shape"], pad) for L in layers]
        box = [min(b[0] for b in bs), min(b[1] for b in bs), max(b[2] for b in bs), max(b[3] for b in bs)]
    return {"box": box, "layers": layers}


def body_cov(key, body, shrink=1.4):
    """Dark coverage of a bubble body (frame-aligned box of the body, origin x0, y0)."""
    import fit
    rgb, m = C.sheet(key)
    bx, by, br = body
    x0, y0 = int(bx - br) - 1, int(by - br) - 1
    n = int(2 * br) + 4
    jj, ii = np.mgrid[y0:y0 + n, x0:x0 + n]
    ins = np.hypot(ii + 0.5 - bx, jj + 0.5 - by) < br - shrink
    cov, _, _ = fit.dark_coverage(rgb[y0:y0 + n, x0:x0 + n], ins)
    return cov, x0, y0


def match_offset(ref, tgt, reach=3):
    """The whole-pixel move (dx, dy) that lays the reference body's glyph onto the target's: ref / tgt = (key, body).
    Correlation of the two dark-coverage maps over every shift within `reach` of the bodies' rounded centre offset."""
    ca, ax, ay = body_cov(*ref)
    cb, bx, by = body_cov(*tgt)
    base = (bx - ax, by - ay)
    best = None
    H = min(ca.shape[0], cb.shape[0])
    for dy in range(-reach, reach + 1):
        for dx in range(-reach, reach + 1):
            a = ca[max(0, -dy):H - max(0, dy), max(0, -dx):H - max(0, dx)]
            b = cb[max(0, dy):H - max(0, -dy), max(0, dx):H - max(0, -dx)]
            n = min(a.shape[0], b.shape[0]), min(a.shape[1], b.shape[1])
            v = (a[:n[0], :n[1]] * b[:n[0], :n[1]]).sum()
            if best is None or v > best[0]:
                best = (v, dx, dy)
    return base[0] + best[1], base[1] + best[2]


def sample_dark(key, shape, q=0.3, grow=0.6):
    """The colour of the darkest part of the art under a shape (grown a little): the median of the pixels whose luminance
    is in the lowest q of those under it. For thin dark glyph strokes whose pixel centres rarely fall deep inside."""
    import fit
    rgb, m = C.sheet(key)
    g = {"t": "grow", "of": shape, "d": grow}
    b = bbox(shape)
    H, W = m.shape
    b = [max(b[0], 0), max(b[1], 0), min(b[2], W), min(b[3], H)]
    x0, y0, x1, y1, x, y = grid(b)
    x1, y1 = min(x1, W), min(y1, H)
    x, y = x[:y1 - y0, :x1 - x0], y[:y1 - y0, :x1 - x0]
    a = rgb[y0:y1, x0:x1]
    sel = (sdf.sd(g, x, y) < 0) & m[y0:y1, x0:x1]
    v = a[sel]
    L = fit.lum(v)
    k = L <= np.quantile(L, q)
    return [round(float(t), 1) for t in np.median(v[k], axis=0)]


def find_cell(key, rct, region):
    """Where a cell's art sits inside a larger region of the same sheet (a button cut from the tray it is drawn on):
    the masked sum of squared differences over every placement (FFT), returns (dx, dy, mean abs diff) to move the cell
    onto its best spot. region = (x0, y0, x1, y1)."""
    rgb, m = C.sheet(key)
    x, y, w, h = rct
    T = rgb[y:y + h, x:x + w]
    M = m[y:y + h, x:x + w].astype(float)
    X0, Y0, X1, Y1 = region
    I = rgb[Y0:Y1, X0:X1]
    H, W = I.shape[:2]
    F = lambda a: np.fft.rfft2(a, s=(H, W))  # noqa: E731
    iF = lambda a: np.fft.irfft2(a, s=(H, W))  # noqa: E731

    def corr(a, b):        # sum over the template of a(t) * b(t + u), for every u
        return iF(np.conj(F(a)) * F(b))
    ssd = np.zeros((H, W))
    for c in range(3):
        t = T[..., c]
        ssd += (M * t * t).sum() - 2 * corr(M * t, I[..., c]) + corr(M, I[..., c] ** 2)
    valid = np.zeros((H, W), bool)
    valid[:H - h + 1, :W - w + 1] = True
    ssd = np.where(valid, ssd, np.inf)
    j, i = np.unravel_index(np.argmin(ssd), ssd.shape)
    mad = float(np.sqrt(max(ssd[j, i], 0) / max(M.sum() * 3, 1)))
    return X0 + i - x, Y0 + j - y, mad
