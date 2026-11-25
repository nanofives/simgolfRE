"""P41 area `marks`: shared helpers (art crops, pixel-grid views, comparison images, scoring).

The marks are tiny (16-32 px) cut-outs on a transparent key (palette index >= 0xfe). Everything here reads the original
sheets under original\\ through fit.load_sheet; a scene built by the element modules holds numbers only.
"""
import json
import os
import sys

import numpy as np
from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
UIVEC = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, UIVEC)
import fit  # noqa: E402
import sdf  # noqa: E402

ROOT = fit.ROOT
OUT = os.path.join(ROOT, "log", "roadmap", "P41", "work", "marks")
os.makedirs(OUT, exist_ok=True)
FLAT = np.array((70, 110, 90), float)        # the roster's backdrop for transparent pixels

GB = "GBUBBLES.pcx"
TA = "Interface/TacksandArrow.pcx"
CI = "Interface/courseinfo.pcx"

_sheets = {}


def sheet(rel):
    if rel not in _sheets:
        idx, pal = fit.load_sheet(rel)
        _sheets[rel] = (pal[idx].astype(float), idx < 0xfe, idx)
    return _sheets[rel]


def crop(rel, x, y, w, h):
    """(rgb float h x w x 3, opaque mask) of a sheet rectangle (sheet coordinates)."""
    rgb, m, _ = sheet(rel)
    return rgb[y:y + h, x:x + w].copy(), m[y:y + h, x:x + w].copy()


def on_flat(rgb, m, bg=FLAT):
    out = np.empty_like(rgb)
    out[:] = bg
    out[m] = rgb[m]
    return out


def textgrid(rel, x, y, w, h, out=None):
    """Print the crop as one letter per pixel: colours sorted dark to light get letters a..z (key '.' = transparent)."""
    rgb, m = crop(rel, x, y, w, h)
    cols = sorted({tuple(int(v) for v in rgb[j, i]) for j in range(h) for i in range(w) if m[j, i]}, key=lambda c: fit.lum(c))
    letters = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
    if len(cols) > len(letters):         # too many colours: luminance digit 0..9 instead
        key = {c: str(min(9, int(fit.lum(c) / 25.6))) for c in cols}
        cols = []
    else:
        key = {c: letters[k] for k, c in enumerate(cols)}
    lines = [f"{rel} x {x}..{x + w} y {y}..{y + h}",
             "    " + "".join(str(i % 10) for i in range(w))]
    for j in range(h):
        lines.append(f"{j:3d} " + "".join(key[tuple(int(v) for v in rgb[j, i])] if m[j, i] else "." for i in range(w)))
    lines.append("  ".join(f"{key[c]}={c}" for c in cols))
    s = "\n".join(lines)
    if out:
        with open(out, "w") as f:
            f.write(s + "\n")
    return s


def gridpng(rgb, z=24, path=None):
    h, w = rgb.shape[:2]
    big = Image.fromarray(np.clip(rgb + 0.5, 0, 255).astype(np.uint8)).resize((w * z, h * z), Image.NEAREST)
    d = ImageDraw.Draw(big)
    for g in range(max(w, h) + 1):
        col = (255, 0, 0) if g % 4 == 0 else (120, 120, 120)
        if g <= w:
            d.line([(g * z, 0), (g * z, h * z)], fill=col)
        if g <= h:
            d.line([(0, g * z), (w * z, g * z)], fill=col)
    if path:
        big.save(path)
    return big


def render_scene(scene, w, h, z, bg):
    """The scene (in crop coordinates: pixel (i, j) covers [i, i+1) x [j, j+1)) at z x over bg (h x w x 3 at 1x)."""
    base = np.asarray(Image.fromarray(np.clip(bg + 0.5, 0, 255).astype(np.uint8)).resize((w * z, h * z), Image.NEAREST), float).copy()
    sdf.render(scene, base, z, origin=(0.0, 0.0))
    return base


def render_1x(scene, w, h, bg, ss=6):
    """The scene at 1x (box-filtered from ss x) for scoring against the art."""
    big = render_scene(scene, w, h, ss, bg)
    return big.reshape(h, ss, w, ss, 3).mean(axis=(1, 3))


def score(scene, art, m, bg):
    """interior_mad: mean |art - shapes| over opaque art pixels at least 1 px from the cut-out's edge; edge_px: count of
    pixels whose opacity class differs (shapes' coverage of bg vs the art's mask)."""
    w, h = art.shape[1], art.shape[0]
    r = render_1x(scene, w, h, bg)
    er = m.copy()
    er[1:, :] &= m[:-1, :]; er[:-1, :] &= m[1:, :]; er[:, 1:] &= m[:, :-1]; er[:, :-1] &= m[:, 1:]
    mad = float(np.abs(r - art)[er].mean()) if er.any() else float("nan")
    drawn = np.abs(r - bg).sum(-1) > 30
    edge = int((drawn != m).sum())
    return round(mad, 2), edge


def compare(states, z, path, gap=6):
    """states: list of (art rgb, art mask, scene, bg) stacked top to bottom; each row is the art (left) and the shapes (right)."""
    rows = []
    for art, m, scene, bg in states:
        h, w = art.shape[:2]
        a = np.asarray(Image.fromarray(np.clip(on_flat(art, m, bg) + 0.5, 0, 255).astype(np.uint8)).resize((w * z, h * z), Image.NEAREST), float)
        s = render_scene(scene, w, h, z, np.broadcast_to(bg, art.shape).astype(float) if np.ndim(bg) == 1 else bg)
        sep = np.full((h * z, gap, 3), 255.0)
        rows.append(np.concatenate([a, sep, s], axis=1))
    W = max(r.shape[1] for r in rows)
    rows = [np.pad(r, ((0, 0), (0, W - r.shape[1]), (0, 0)), constant_values=255) for r in rows]
    out = []
    for k, r in enumerate(rows):
        if k:
            out.append(np.full((gap, W, 3), 255.0))
        out.append(r)
    img = np.concatenate(out)
    Image.fromarray(np.clip(img + 0.5, 0, 255).astype(np.uint8)).save(path)
    return path


def bgfill(art, m, bg=FLAT):
    """The backdrop with the art's element wiped: transparent key colour everywhere (the marks are cut-outs)."""
    out = np.empty_like(art)
    out[:] = bg
    return out


def save_json(eid, obj):
    with open(os.path.join(OUT, f"{eid}.json"), "w") as f:
        json.dump(obj, f, indent=1)


# ---------------------------------------------------------------------------------------------------- shapes of my own
def _star(S, x, y):
    """marks.star: a regular n-pointed star, centre c, outer radius r, inner radius ri, first point at angle a0 (deg, 0 =
    right, -90 = up)."""
    n = S.get("n", 5)
    cx, cy = S["c"]
    pts = []
    for k in range(2 * n):
        a = np.deg2rad(S.get("a0", -90.0) + k * 180.0 / n)
        rr = S["r"] if k % 2 == 0 else S["ri"]
        pts.append([cx + rr * np.cos(a), cy + rr * np.sin(a)])
    return sdf.sd({"t": "poly", "rings": [pts]}, x, y)


sdf.EXTRA["marks.star"] = _star


def _smooth_open(pts, n):
    for _ in range(int(n)):
        pts = [pts[0]] + [[a[0] * (1 - f) + b[0] * f, a[1] * (1 - f) + b[1] * f]
                          for a, b in zip(pts, pts[1:]) for f in (0.25, 0.75)] + [pts[-1]]
    return pts


def _pline(S, x, y):
    """marks.pline: an open polyline of width w (optional "smooth": n rounds of corner cutting, ends kept). Joins are round;
    "cap" "butt" (default) cuts each END SEGMENT flat at its own end only (sdf's stroke cuts the whole shape with the end
    half-planes, which removes a hook that curls back past its start). "w1": a second width for the far end (linear)."""
    pts = _smooth_open([list(p) for p in S["pts"]], S.get("smooth", 0))
    segs = list(zip(pts, pts[1:]))
    L = [0.0]
    for a, b in segs:
        L.append(L[-1] + np.hypot(b[0] - a[0], b[1] - a[1]))
    tot = max(L[-1], 1e-9)
    w0, w1 = S["w"], S.get("w1", S["w"])
    d = np.full(x.shape, 1e18)
    for k, (a, b) in enumerate(segs):
        ex, ey = b[0] - a[0], b[1] - a[1]
        ll = max(ex * ex + ey * ey, 1e-12)
        wx, wy = x - a[0], y - a[1]
        u = np.clip((wx * ex + wy * ey) / ll, 0, 1)
        t = (L[k] + u * np.sqrt(ll)) / tot
        dk = np.hypot(wx - ex * u, wy - ey * u) - (w0 * (1 - t) + w1 * t) / 2
        if S.get("cap", "butt") == "butt":
            # the segments within one width of an end are all cut by that end's plane (after corner cutting the end
            # segment is short and the next one's round join would otherwise bulge past the flat end)
            if L[k] < max(w0, w1):
                p, q = pts[0], pts[1]
                ux, uy = q[0] - p[0], q[1] - p[1]
                n = max(np.hypot(ux, uy), 1e-9)
                dk = np.maximum(dk, -((x - p[0]) * ux + (y - p[1]) * uy) / n)
            if tot - L[k + 1] < max(w0, w1):
                p, q = pts[-1], pts[-2]
                ux, uy = p[0] - q[0], p[1] - q[1]
                n = max(np.hypot(ux, uy), 1e-9)
                dk = np.maximum(dk, ((x - p[0]) * ux + (y - p[1]) * uy) / n)
        d = np.minimum(d, dk)
    return d


sdf.EXTRA["marks.pline"] = _pline


def pline(pts, w, smooth=0, cap="butt", w1=None):
    S = {"t": "marks.pline", "pts": [list(p) for p in pts], "w": w, "smooth": smooth, "cap": cap}
    if w1 is not None:
        S["w1"] = w1
    return S


def _superellipse(S, x, y):
    """marks.superellipse: |dx/a|^n + |dy/b|^n = 1 (a squircle for n > 2), centre c, radii r = [a, b]. The distance is the
    norm's level offset scaled by the radius (exact on the axes, within a few percent elsewhere for n < 3)."""
    a, b = S["r"]
    n = S["n"]
    u, v = np.abs(x - S["c"][0]) / a, np.abs(y - S["c"][1]) / b
    k = (u ** n + v ** n) ** (1.0 / n)
    return (k - 1.0) * min(a, b)


sdf.EXTRA["marks.superellipse"] = _superellipse


def rounded(pts, r, n=6):
    """The polygon pts with every corner replaced by a circular arc of radius r (a number or one per corner) tangent to
    both edges, as a point list for a "poly" ring."""
    P = np.asarray(pts, float)
    N = len(P)
    rs = r if isinstance(r, (list, tuple)) else [r] * N
    out = []
    for k in range(N):
        p0, p1, p2 = P[k - 1], P[k], P[(k + 1) % N]
        u, v = p0 - p1, p2 - p1
        lu, lv = np.hypot(*u), np.hypot(*v)
        u, v = u / lu, v / lv
        ang = np.arccos(np.clip(u @ v, -1, 1))
        rr = rs[k]
        if rr <= 0 or ang < 1e-3 or ang > np.pi - 1e-3:
            out.append(p1.tolist())
            continue
        t = min(rr / np.tan(ang / 2), lu * 0.45, lv * 0.45)        # tangent length, kept inside the edges
        rr = t * np.tan(ang / 2)
        a, b = p1 + u * t, p1 + v * t
        bis = (u + v) / np.hypot(*(u + v))
        c = p1 + bis * (rr / np.sin(ang / 2))
        a0, a1 = np.arctan2(*(a - c)[::-1]), np.arctan2(*(b - c)[::-1])
        d = (a1 - a0 + np.pi) % (2 * np.pi) - np.pi
        for i in range(n + 1):
            th = a0 + d * i / n
            out.append([round(float(c[0] + rr * np.cos(th)), 3), round(float(c[1] + rr * np.sin(th)), 3)])
    return out


def bar(a, b, w):
    """A straight line with flat ends."""
    return pline([a, b], w)


def results_row(eid, status, how, mad="", edge="", unsure=""):
    p = os.path.join(OUT, "results.tsv")
    rows = {}
    if os.path.exists(p):
        with open(p) as f:
            next(f)
            for line in f:
                c = line.rstrip("\n").split("\t")
                rows[c[0]] = c
    rows[eid] = [eid, status, how, str(mad), str(edge), unsure]
    with open(p, "w") as f:
        f.write("id\tstatus\thow\tinterior_mad\tedge_px\tunsure\n")
        for k in sorted(rows):
            f.write("\t".join(rows[k]) + "\n")
