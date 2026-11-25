"""uivec wide canvas (P41W, log/roadmap/P41/WIDE_BRIEF.md): one way for every front-end screen to fill any window from 4:3 up.

THE CANVAS
  Frame coordinates stay 800 x 600 for the centre. A window of aspect a >= 4/3 shows x in [400 - 300 a, 400 + 300 a], y in
  [0, 600] (view(a)). A wide scene is valid for any x in DESIGN_X = [-700, 1500] (32:9 is [-666.7, 1466.7]); span items are
  built for [-2000, 2800] so nothing ends inside any view.

A WIDE SCENE (JSON, numbers only, no art bytes):
  {"id": "<screen>-wide", "screen": ..., "design_x": [-700, 1500],
   "ground": {...how the ground was made, for the record...},
   "draw": [ENTRY, ...]}                  painted in order (the game's order: backdrop, then cells, then pictures on top)
  ENTRY is one of
    {"name", "anchor", "box", "layers"}   an ordinary sdf.py item
    {"name", "anchor", "ref": "<approved element id>" [, "state": "idle"] [, "omit": [[item, layer], ...]]}
                                          an approved element's scene, unchanged (omit drops named layers that a span item of
                                          this scene replaces, e.g. a line that now runs on to the screen edges)
    {"name", "anchor", "picture": {"sheet", "rect": [x, y, w, h], "at": [x, y], "matte": {...}}}
                                          art lifted from the game file at run time and placed on top (see MATTES)
  "anchor" is
    "centre"  stays at its frame position (the composition);
    "span"    continues to both screen edges by construction (half planes, long segments, ground blobs laid out to x = +-2000);
    "left" / "right"  attached to that screen edge in the art: moved by edge_dx(a, anchor) = -+ (300 a - 400).
  place(scene, a, refs) resolves the anchors for one aspect and returns plain sdf items plus pictures.

THE GROUND (a mottled lilac field)
  A base colour plus soft blobs: each blob is an sdf ellipse layer with "soft" = its short radius, so its alpha is
  smoothstep(1 - rho / (2 s)) across 2 s (rho measured on the ellipse), painted with a colour on the screen's ground ramp
  (ramp(lum): colour of the art's ground at that luminance, a quadratic per channel measured on the art).
  fit_ground(): inside the frame the blobs are placed where the art has them (greedy matching pursuit on the art's ground
  luminance, exact alpha compositing, hidden pixels free); outside it continue_ground() lays blobs with the same sizes,
  shapes, contrasts and density from a fixed seed into x in [-2000, 0) and (800, 2800], and those are composed FIRST, so the
  fit inside sees their tails and the frame edge carries no seam. The result is plain layers in the scene: the port renders
  it, it never runs the generator.

MATTES (pictures that stay art)
  {"kind": "key"}: the sheet's own transparency (index >= 0xfe), alpha 0 / 1 (the game's own keyed cells).
  {"kind": "opaque"}: the whole cut (a picture inside a frame that an approved element draws over its border).
  {"kind": "ground_diff", "t0", "t1"}: the picture is painted on the old ground in its sheet. With G = the DRAWN screen
  beneath it at 1x (the entries before it in "draw", 4:3 placement, rendered at frame pixel centres: under_1x) and A = the
  art pixel: dist = |A - G| (RGB euclidean),
  alpha = clip((dist - t0) / (t1 - t0), 0, 1), and the picture's own colour F = G + (A - G) / alpha (the old ground taken
  out of its soft edge, clipped to 0..255); "feather": f multiplies alpha by min(1, e / f), e = distance of the pixel centre
  from the cut's border. The port does the same per pixel at load time: matte(), lift().
"""
import json
import math
import os
import sys

import numpy as np
from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import fit      # noqa: E402
import sdf      # noqa: E402

DESIGN_X = (-700.0, 1500.0)
SPAN_X = (-2000.0, 2800.0)
ASPECTS = {"43": 4 / 3, "1610": 16 / 10, "169": 16 / 9, "219": 21 / 9, "329": 32 / 9}


def view(a):
    """The frame x range a window of aspect a shows."""
    return 400.0 - 300.0 * a, 400.0 + 300.0 * a


def edge_dx(a, anchor):
    if anchor == "left":
        return 400.0 - 300.0 * a
    if anchor == "right":
        return 300.0 * a - 400.0
    return 0.0


# ------------------------------------------------------------------------------------------------------------ shifting
def _sp(p, dx):
    return [p[0] + dx, p[1]]


def shift_shape(S, dx):
    """The same shape moved by dx along x (coordinates rewritten, so analytic normals stay analytic)."""
    if dx == 0 or S is None:
        return S
    t = S["t"]
    T = dict(S)
    if t in ("circle", "ellipse", "rrect", "arc", "dimpled"):
        T["c"] = _sp(S["c"], dx)
    elif t in ("seg", "arrow"):
        T["a"], T["b"] = _sp(S["a"], dx), _sp(S["b"], dx)
    elif t == "half":
        T["p"] = _sp(S["p"], dx)
    elif t == "qstroke":
        T["p"] = [_sp(p, dx) for p in S["p"]]
    elif t == "stroke":
        T["pts"] = [_sp(p, dx) for p in S["pts"]]
    elif t == "poly":
        T["rings"] = [[_sp(p, dx) for p in r] for r in S["rings"]]
    elif t == "lathe":
        T["x"] = S["x"] + dx
    elif t in ("union", "inter"):
        T["of"] = [shift_shape(q, dx) for q in S["of"]]
    elif t == "sub":
        T["a"], T["b"] = shift_shape(S["a"], dx), shift_shape(S["b"], dx)
    elif t in ("grow", "outline", "move"):
        T["of"] = shift_shape(S["of"], dx)
    else:
        T = {"t": "move", "of": S, "d": [dx, 0.0]}
    return T


def shift_paint(P, dx):
    if dx == 0 or not isinstance(P, dict):
        return P
    P = json.loads(json.dumps(P))
    if "radial" in P:
        P["radial"]["c"] = _sp(P["radial"]["c"], dx)
    if "linear" in P:
        P["linear"]["a"] = _sp(P["linear"]["a"], dx)
        P["linear"]["b"] = _sp(P["linear"]["b"], dx)
    if "bevel" in P and P["bevel"].get("of"):
        P["bevel"]["of"] = shift_shape(P["bevel"]["of"], dx)
    return P


def shift_item(item, dx):
    if dx == 0:
        return item
    x0, y0, x1, y1 = item["box"]
    L2 = []
    for L in item["layers"]:
        M = dict(L)
        M["shape"] = shift_shape(L["shape"], dx)
        M["fill"] = shift_paint(L["fill"], dx)
        if "clip" in L:
            M["clip"] = shift_shape(L["clip"], dx)
        if "falloff" in L:
            M["falloff"] = dict(L["falloff"], c=_sp(L["falloff"]["c"], dx))
        L2.append(M)
    return dict(item, box=[x0 + dx, y0, x1 + dx, y1], layers=L2)


# ------------------------------------------------------------------------------------------------------------ placing
def load_ref(ref, state="idle", scenes_dir=None):
    """An approved element's scene items from re/tools/uivec/scenes/<area>/<id>.json."""
    scenes_dir = scenes_dir or os.path.join(HERE, "scenes", "frontend")
    with open(os.path.join(scenes_dir, ref + ".json")) as f:
        doc = json.load(f)
    for st in doc["states"]:
        if st["name"] == state:
            return st["scene"]
    return doc["states"][0]["scene"]


def place(scene, a, refs=None):
    """(items, pictures): the wide scene's entries for aspect a, anchors resolved. refs: {ref: items} for refs that are not
    plain scene files (title.py's buttons); others load from the approved scenes. Pictures keep their order index so a
    caller can interleave them: each output entry is ("items", [...]) or ("picture", entry, dx)."""
    out = []
    for e in scene["draw"]:
        dx = edge_dx(a, e.get("anchor", "centre"))
        if "picture" in e:
            out.append(("picture", e, dx))
            continue
        if "ref" in e:
            items = (refs or {}).get(e["ref"])
            if items is None:
                items = load_ref(e["ref"], e.get("state", "idle"))
            items = json.loads(json.dumps(items))
            for it_i, ly_i in sorted(e.get("omit", []), reverse=True):
                del items[it_i]["layers"][ly_i]
            out.append(("items", [shift_item(it, dx) for it in items if it["layers"]]))
            continue
        out.append(("items", [shift_item(e, dx)]))
    return out


def span_only(scene):
    return dict(scene, draw=[e for e in scene["draw"] if e.get("anchor") == "span" and "layers" in e])


# ------------------------------------------------------------------------------------------------------------ mattes
def matte(art, ground, t0, t1):
    """alpha and own colour of a picture painted on an old ground (see the module doc). art, ground: H x W x 3 float."""
    dist = np.sqrt(((art - ground) ** 2).sum(-1))
    al = np.clip((dist - t0) / max(t1 - t0, 1e-6), 0.0, 1.0)
    col = np.where(al[..., None] > 1e-3, ground + (art - ground) / np.maximum(al[..., None], 1e-3), art)
    return al, np.clip(col, 0, 255)


def lift(pic, ground_full=None):
    """(rgb float h x w x 3, alpha h x w) of a picture entry's cut. ground_full: the drawn ground at 1x over the frame
    (needed by ground_diff)."""
    idx, pal = fit.load_sheet(pic["sheet"])
    x, y, w, h = pic["rect"]
    art = pal[idx[y:y + h, x:x + w]].astype(float)
    m = pic["matte"]
    if m["kind"] == "key":
        return art, (idx[y:y + h, x:x + w] < 0xfe).astype(float)
    if m["kind"] == "opaque":
        return art, np.ones((h, w))
    if m["kind"] == "ground_diff":
        ax, ay = pic.get("at", [x, y])
        g = ground_full[ay:ay + h, ax:ax + w]
        al, col = matte(art, g, m["t0"], m["t1"])
        f = float(m.get("feather", 0.0))
        if f > 0:
            # alpha ramps up over f px from the cut's border (pixel centres), so the cut's rectangle never shows
            jj, ii = np.mgrid[0:h, 0:w]
            e = np.minimum(np.minimum(ii + 0.5, w - ii - 0.5), np.minimum(jj + 0.5, h - jj - 0.5))
            al = al * np.clip(e / f, 0.0, 1.0)
        return col, al
    raise ValueError(m["kind"])


def composite_picture(img, pic, s, origin, dx=0.0, ground_full=None):
    """Lay a lifted picture over a render (scale s, frame origin), upscaled bilinearly with premultiplied alpha, as the port's
    textured quad does."""
    rgb, al = lift(pic["picture"], ground_full)
    x, y, w, h = pic["picture"]["rect"]
    ax, ay = pic["picture"].get("at", [x, y])
    ax += dx
    X0, Y0 = (ax - origin[0]) * s, (ay - origin[1]) * s
    W, H = int(round(w * s)), int(round(h * s))
    pm = np.dstack([rgb * al[..., None], al * 255.0]).astype(np.float32)
    chans = [np.asarray(Image.fromarray(pm[..., k], "F").resize((W, H), Image.BILINEAR)) for k in range(4)]
    big = np.dstack(chans)
    i0, j0 = int(round(X0)), int(round(Y0))
    Hh, Ww = img.shape[:2]
    a0, b0 = max(0, -i0), max(0, -j0)
    a1, b1 = min(W, Ww - i0), min(H, Hh - j0)
    if a1 <= a0 or b1 <= b0:
        return img
    sub = big[b0:b1, a0:a1]
    A = (sub[..., 3:4] / 255.0)
    dst = img[j0 + b0:j0 + b1, i0 + a0:i0 + a1]
    dst[:] = dst * (1 - A) + sub[..., :3]
    return img


# ------------------------------------------------------------------------------------------------------------ rendering
def under_1x(scene, refs=None, upto=None):
    """The drawn screen over the frame at 1x (4:3 placement) from the entries before index `upto` (all when None), pictures
    left out: what a ground_diff matte of the picture at `upto` is taken against. The port renders the same at load time."""
    sub = dict(scene, draw=[e for e in scene["draw"][:upto] if "picture" not in e])
    img = np.zeros((600, 800, 3))
    for kind, items in place(sub, 4 / 3, refs):
        sdf.render(items, img, 1.0)
    return img


def ground_1x(scene, refs=None):
    return under_1x(span_only(scene), refs)


def unders(scene, refs=None):
    """{draw index: under_1x} for every ground_diff picture of the scene."""
    out = {}
    for i, e in enumerate(scene["draw"]):
        if "picture" in e and e["picture"]["matte"]["kind"] == "ground_diff":
            out[i] = under_1x(scene, refs, i)
    return out


def render(scene, a, height, refs=None, origin_x=None, width=None, s=None, y0=0.0, h_px=None, under=None):
    """The screen at aspect a, `height` pixels tall (scale height / 600). origin_x / width / s / y0 / h_px render a crop.
    under: unders(scene) when already computed."""
    s = s or height / 600.0
    if origin_x is None:
        origin_x = view(a)[0]
    if width is None:
        width = int(round(600.0 * a * s))
    if h_px is None:
        h_px = int(round(600 * s))
    img = np.zeros((h_px, width, 3))
    if under is None:
        under = unders(scene, refs)
    pics = [i for i, e in enumerate(scene["draw"]) if "picture" in e]
    k = 0
    for entry in place(scene, a, refs):
        if entry[0] == "items":
            sdf.render(entry[1], img, s, origin=(origin_x, y0))
        else:
            composite_picture(img, entry[1], s, (origin_x, y0), entry[2], under.get(pics[k]))
            k += 1
    return np.clip(img + 0.5, 0, 255).astype(np.uint8)


def ticks(img, a, s, colour=(255, 0, 0), length=14):
    """Thin tick marks on the top edge at frame x = 0 and x = 800."""
    out = Image.fromarray(img)
    d = ImageDraw.Draw(out)
    x0 = view(a)[0]
    for fx in (0.0, 800.0):
        px = (fx - x0) * s
        d.line([(px, 0), (px, length)], fill=colour, width=2)
    return np.asarray(out)


# ------------------------------------------------------------------------------------------------------------ ground
def ramp_fit(rgb, lum_vals):
    """Quadratic per channel: colour of the ground at a luminance."""
    return [[float(v) for v in np.polyfit(lum_vals, rgb[:, k], 2)] for k in range(3)]


def ramp(coef, t):
    return [float(np.clip(np.polyval(c, t), 0, 255)) for c in coef]


def blob_layer(c, rx, ry, rot, colour, alpha):
    s = min(rx, ry)
    return {"shape": {"t": "ellipse", "c": [round(c[0], 2), round(c[1], 2)], "r": [round(rx, 2), round(ry, 2)], "rot": round(rot, 1)},
            "soft": round(s, 2), "fill": [round(v, 1) for v in colour], "alpha": round(alpha, 3)}


def blob_box(L):
    S = L["shape"]
    m = max(S["r"]) + L["soft"] + 1
    return [S["c"][0] - m, S["c"][1] - m, S["c"][0] + m, S["c"][1] + m]


def _kernel(rx, ry, rot):
    s = min(rx, ry)
    m = int(math.ceil(max(rx, ry) + s)) + 1
    ys, xs = np.mgrid[-m:m + 1, -m:m + 1].astype(float)
    d = sdf.sd({"t": "ellipse", "c": [0.0, 0.0], "r": [rx, ry], "rot": rot}, xs, ys)
    u = np.clip(0.5 - d / (2 * s), 0, 1)
    return u * u * (3 - 2 * u)


def _blur(img, s):
    if s <= 0:
        return img
    r = int(3 * s) + 1
    k = np.exp(-0.5 * (np.arange(-r, r + 1) / s) ** 2)
    k /= k.sum()
    o = np.apply_along_axis(lambda v: np.convolve(v, k, "same"), 0, img)
    return np.apply_along_axis(lambda v: np.convolve(v, k, "same"), 1, o)


DICT_SCALES = (90, 64, 45, 32, 23, 16, 11, 8)
DICT_SHAPES = ((1.0, 0.0), (1.9, 0.0), (1.9, 45.0), (1.9, 90.0), (1.9, 135.0))


def _atoms():
    out = []
    for s in DICT_SCALES:
        for q, rot in DICT_SHAPES:
            out.append((s, q, rot, s * q, s, rot))      # rx = s q along the rotated x axis, ry = s
    return out


def _composite_lum(C, k, cx, cy, tc, al):
    """C = C (1 - al k) + tc al k with kernel k centred at integer pixel (cx, cy)."""
    m = k.shape[0] // 2
    H, W = C.shape
    x0, x1, y0, y1 = cx - m, cx + m + 1, cy - m, cy + m + 1
    a0, b0 = max(0, -x0), max(0, -y0)
    a1, b1 = k.shape[1] - max(0, x1 - W), k.shape[0] - max(0, y1 - H)
    if a1 <= a0 or b1 <= b0:
        return
    kk = k[b0:b1, a0:a1] * al
    sub = C[y0 + b0:y0 + b1, x0 + a0:x0 + a1]
    sub[:] = sub * (1 - kk) + tc * kk


def fit_ground(art_rgb, mask, coef, base_t, fixed_layers=(), n_atoms=600, batch=12, k_alpha=0.3, t_range=(95.0, 254.0),
               min_gain=0.0, log=print):
    """Soft blobs that redraw the art's ground inside the frame (see the module doc). Returns the blob layers in paint order
    (after fixed_layers, which are composited first and are not returned)."""
    H, W = mask.shape
    L = fit.lum(art_rgb)
    M = mask.astype(float)
    den = _blur(M, 1.2)
    T = np.where(den > 1e-3, _blur(L * M, 1.2) / np.maximum(den, 1e-3), L)
    ys, xs = np.mgrid[0:H, 0:W]
    C = np.full((H, W), float(base_t))
    # the fixed blobs (continuation outside) composited exactly at pixel centres
    for Lr in fixed_layers:
        S = Lr["shape"]
        bx = blob_box(Lr)
        i0, i1 = max(0, int(bx[0])), min(W, int(bx[2]) + 1)
        j0, j1 = max(0, int(bx[1])), min(H, int(bx[3]) + 1)
        if i1 <= i0 or j1 <= j0:
            continue
        x, y = xs[j0:j1, i0:i1] + 0.5, ys[j0:j1, i0:i1] + 0.5
        d = sdf.sd(S, x, y)
        u = np.clip(0.5 - d / (2 * Lr["soft"]), 0, 1)
        k = u * u * (3 - 2 * u) * Lr["alpha"]
        tc = float(fit.lum(np.asarray(Lr["fill"], float)))
        C[j0:j1, i0:i1] = C[j0:j1, i0:i1] * (1 - k) + tc * k
    atoms = _atoms()
    kers = [_kernel(rx, ry, rot) for (_, _, _, rx, ry, rot) in atoms]
    FH, FW = 1 << int(math.ceil(math.log2(H + 400))), 1 << int(math.ceil(math.log2(W + 400)))

    def conv(img, k):
        F = np.fft.rfft2(img, (FH, FW)) * np.fft.rfft2(k, (FH, FW))
        o = np.fft.irfft2(F, (FH, FW))
        m = k.shape[0] // 2
        return o[m:m + H, m:m + W]

    kf = []
    for k in kers:
        kf.append((np.fft.rfft2(k, (FH, FW)), k.shape[0] // 2))
    norms = [conv(M, k * k) for k in kers]
    energy = [float((k * k).sum()) for k in kers]
    layers = []
    stalls = 0
    ban = {}       # atom index -> pixels where a pick was refused (so the pursuit does not pick it again)
    while len(layers) < n_atoms:
        R = (T - C) * M
        FR = np.fft.rfft2(R, (FH, FW))
        best = []
        for ai, ((Fk, m), nm) in enumerate(zip(kf, norms)):
            co = np.fft.irfft2(FR * Fk, (FH, FW))[m:m + H, m:m + W]
            # centres on frame pixels only; a blob needs a fair share of its footprint on visible ground to be placed
            g = np.where(nm > 0.15 * energy[ai], co * co / np.maximum(nm, 1e-6), 0.0)
            if ai in ban:
                g[ban[ai]] = 0.0
            best.append((g, co / np.maximum(nm, 1e-6), ai))
        # pick a batch of separated maxima over all atoms
        G = np.stack([b[0] for b in best])
        picks = []
        Gf = G.copy()
        for _ in range(batch):
            idx = int(np.argmax(Gf))
            ai, cy, cx = np.unravel_index(idx, Gf.shape)
            gain = Gf[ai, cy, cx]
            if gain <= min_gain:
                break
            A = best[ai][1][cy, cx]
            picks.append((ai, cx, cy, A, gain))
            s = atoms[ai][0] * max(atoms[ai][1], 1.0)
            r = int(2.2 * s)
            Gf[:, max(0, cy - r):cy + r + 1, max(0, cx - r):cx + r + 1] = 0
        if not picks:
            break
        n_before = len(layers)
        for ai, cx, cy, A, gain in picks:
            s, q, rot, rx, ry, _ = atoms[ai]
            c0 = C[cy, cx]
            tc = float(np.clip(c0 + A / k_alpha, *t_range))
            if abs(tc - c0) < 1e-3:
                continue
            # the alpha that best fits the residual under true compositing (delta = al k (tc - C)), on the visible footprint
            k = kers[ai]
            m = k.shape[0] // 2
            x0, y0 = cx - m, cy - m
            a0, b0 = max(0, -x0), max(0, -y0)
            a1, b1 = k.shape[1] - max(0, x0 + k.shape[1] - W), k.shape[0] - max(0, y0 + k.shape[0] - H)
            kk = k[b0:b1, a0:a1]
            sl = (slice(y0 + b0, y0 + b1), slice(x0 + a0, x0 + a1))
            basis = kk * (tc - C[sl]) * M[sl]
            den_ = float((basis * basis).sum())
            if den_ < 1e-6:
                continue
            al = float(np.clip((basis * (T[sl] - C[sl])).sum() / den_, 0.0, 0.95))
            if al < 0.01:
                b = ban.setdefault(ai, np.zeros((H, W), bool))
                b[max(0, cy - 2):cy + 3, max(0, cx - 2):cx + 3] = True
                continue
            _composite_lum(C, kers[ai], cx, cy, tc, al)
            layers.append(blob_layer((cx + 0.5, cy + 0.5), rx, ry, rot, ramp(coef, tc), al))
        if len(layers) == n_before:
            # every pick of this batch was refused (its alpha under 1 %): the same peaks would come back forever
            stalls += 1
            if stalls >= 2:
                break
        else:
            stalls = 0
        if len(layers) % 60 < batch:
            e = np.abs((T - C)[mask])
            log(f"  ground: {len(layers)} blobs, mean |res| {e.mean():.2f}, p95 {np.percentile(e, 95):.2f}")
    return layers, C


def continue_ground(inside, coef, seed, xr=((SPAN_X[0], 0.0), (800.0, SPAN_X[1])), frame_area=800.0 * 600.0):
    """Blobs for the ground outside the frame with the statistics of the fitted ones inside: for each blob size, the same
    count per area (Poisson disc, a fixed seed), shapes, angles and contrasts drawn from the inside blobs of that size.
    Colour contrast is kept as (blob lum - base lum) about the base."""
    rng = np.random.default_rng(seed)
    by = {}
    for L in inside:
        s = round(min(L["shape"]["r"]), 2)
        by.setdefault(s, []).append(L)
    out = []
    for s in sorted(by, reverse=True):
        group = by[s]
        dens = len(group) / frame_area
        for x0, x1 in xr:
            area = (x1 - x0) * (600 + 2 * s)
            n = int(round(dens * area))
            if n == 0:
                continue
            dmin = 0.7 * math.sqrt(area / max(n, 1))
            pts = []
            tries = 0
            while len(pts) < n and tries < n * 60:
                tries += 1
                p = (rng.uniform(x0, x1), rng.uniform(-s, 600 + s))
                if all((p[0] - q[0]) ** 2 + (p[1] - q[1]) ** 2 >= dmin * dmin for q in pts):
                    pts.append(p)
            for p in pts:
                src = group[int(rng.integers(len(group)))]
                S = src["shape"]
                out.append(blob_layer(p, S["r"][0], S["r"][1], S.get("rot", 0.0), src["fill"], src["alpha"]))
    return out


HP_SIGMAS = (4.0, 8.0, 16.0, 32.0, 64.0)
FILL_X = ((-900.0, 0.0), (800.0, 1700.0))     # blobs laid out past 32:9 ([-667, 1467]); further out the base alone


def ground_stats(L, mask):
    """The ground's texture as numbers: the std of its luminance above each cutoff (L - blur(L, sigma)), on pixels whose whole
    neighbourhood is visible ground, plus its mean."""
    M = mask.astype(float)
    out = []
    for s in HP_SIGMAS:
        den = _blur(M, s)
        lo = _blur(L * M, s) / np.maximum(den, 1e-6)
        ok = mask & (den > 0.92)
        if ok.sum() <= 500:
            # little plain ground on this screen: accept windows that are mostly visible
            ok = mask & (den > 0.6)
        out.append(float((L - lo)[ok].std()) if ok.sum() > 500 else float("nan"))
    return out, float(L[mask].mean())


def _noise_layers(inside, base_t, amp, coef, seed, xr, zoff=0.0, t_dark=120.0, t_light=236.0, q=1.9):
    """The continuation field: per scale s a jittered grid of blobs (spacing 1.25 s), each a circle or an ellipse of aspect q
    at a random angle in the share the inside blobs have, with signed contrast A = amp[s] (z + zoff), z drawn from all the
    inside blobs' contrasts (each scale standardised, then pooled: the art's lean to dark wisps). A blob of contrast A is the
    ramp colour at t_dark (A < 0) or t_light painted at alpha |A| / |t - base|."""
    rng = np.random.default_rng(seed)
    by = {}
    n_ell = 0
    for L in inside:
        s = round(min(L["shape"]["r"]), 2)
        tc = float(fit.lum(np.asarray(L["fill"], float)))
        by.setdefault(s, []).append(L["alpha"] * (tc - base_t))
        n_ell += L["shape"]["r"][0] != L["shape"]["r"][1]
    zs = []
    for v in by.values():
        v = np.array(v)
        if len(v) >= 5:
            zs.append(v / max(np.sqrt((v * v).mean()), 1e-6))     # unit rms, sign balance kept
    z = np.concatenate(zs)
    z = z - z.mean()                                               # the mean is set by zoff
    p_ell = n_ell / max(len(inside), 1)
    out = []
    for s in sorted(amp, reverse=True):
        if amp[s] <= 0:
            continue
        g = 1.25 * s
        for x0, x1 in xr:
            for gy in np.arange(-s, 600 + s, g):
                for gx in np.arange(x0 + g / 2, x1, g):
                    px, py = gx + rng.uniform(-0.45, 0.45) * g, gy + rng.uniform(-0.45, 0.45) * g
                    ell = rng.uniform() < p_ell
                    rot = float(rng.uniform(0, 180)) if ell else 0.0
                    A = amp[s] * (z[int(rng.integers(len(z)))] + zoff)
                    tc = t_dark if A < 0 else t_light
                    al = float(np.clip(A / (tc - base_t), 0, 0.95))
                    if al < 0.004:
                        continue
                    out.append(blob_layer((px, py), s * q if ell else s, s, rot, ramp(coef, tc), al))
    return out


def _lum_canvas(layers, base_t, x0, y0, w, h):
    C = np.full((h, w), float(base_t))
    ys, xs = np.mgrid[0:h, 0:w]
    for Lr in layers:
        bx = blob_box(Lr)
        i0, i1 = max(0, int(bx[0] - x0)), min(w, int(bx[2] - x0) + 1)
        j0, j1 = max(0, int(bx[1] - y0)), min(h, int(bx[3] - y0) + 1)
        if i1 <= i0 or j1 <= j0:
            continue
        x, y = xs[j0:j1, i0:i1] + x0 + 0.5, ys[j0:j1, i0:i1] + y0 + 0.5
        d = sdf.sd(Lr["shape"], x, y)
        u = np.clip(0.5 - d / (2 * Lr["soft"]), 0, 1)
        k = u * u * (3 - 2 * u) * Lr["alpha"]
        tc = float(fit.lum(np.asarray(Lr["fill"], float)))
        C[j0:j1, i0:i1] = C[j0:j1, i0:i1] * (1 - k) + tc * k
    return C


def continue_ground_noise(inside, coef, base_t, art_rgb, mask, seed, scales=(90, 64, 45, 32, 23, 16), tilt=1.0, log=print,
                          t_light=236.0):
    """Blobs outside the frame whose texture statistics (ground_stats) match the art's ground: contrast per scale
    amp_s = c (s / 32)^tilt (tilt > 0 leans to big soft clouds, as the art does; 1.0 chosen by eye against the art), the gain c
    fitted on a test patch to the art's std above the cutoffs 16 and 32 (the cutoff 8 and 4 hold the palette's dither, which
    the drawn ground drops), then a shared shift of the contrasts matches the art's mean. Returns (layers, report)."""
    target, tmean = ground_stats(fit.lum(art_rgb), mask)
    patch = ((-900.0, -100.0),)
    K = (2, 3)

    def run(c, zoff):
        amp = {s: c * (s / 32.0) ** tilt for s in scales}
        lay = _noise_layers(inside, base_t, amp, coef, seed, patch, zoff, t_light=t_light)
        C = _lum_canvas(lay, base_t, -900, 0, 800, 600)
        got, gmean = ground_stats(C, np.ones(C.shape, bool))
        return amp, got, gmean

    c, zoff, prev = 6.0, 0.0, None
    for r in range(8):
        amp, got, gmean = run(c, zoff)
        ks = [k for k in K if np.isfinite(target[k])] or [k for k in (1, 2, 3) if np.isfinite(target[k])]
        ratio = float(np.exp(np.mean([math.log(target[k] / max(got[k], 1e-6)) for k in ks])))
        log(f"  continuation {r}: gain {c:.2f} zoff {zoff:+.3f} got {np.round(got, 2)} target {np.round(target, 2)} "
            f"mean {gmean:.1f} (art {tmean:.1f})")
        if abs(ratio - 1) < 0.03 and abs(gmean - tmean) < 0.4:
            break
        c *= ratio
        if prev is None:
            step = 0.05 if gmean < tmean else -0.05
        else:
            dz, dm = zoff - prev[0], gmean - prev[1]
            step = float(np.clip((tmean - gmean) * dz / dm, -0.3, 0.3)) if abs(dm) > 1e-3 and abs(dz) > 1e-6 else 0.0
        prev = (zoff, gmean)
        zoff += step
    lay = _noise_layers(inside, base_t, amp, coef, seed, FILL_X, zoff, t_light=t_light)
    return lay, {"target": target, "got": got, "amp": amp, "gain": c, "tilt": tilt, "zoff": zoff, "mean": [tmean, gmean]}


def ground_entries(base_rgb, layers, name="ground", chunk=1):
    """The ground as span entries: one full-width base rectangle, then the blobs (one entry each, so its box is tight)."""
    ents = [{"name": name + " base", "anchor": "span", "box": [SPAN_X[0], -20.0, SPAN_X[1], 620.0],
             "layers": [{"shape": {"t": "rrect", "c": [400.0, 300.0], "half": [3000.0, 400.0], "r": 0.0},
                         "fill": [round(v, 1) for v in base_rgb]}]}]
    for i, L in enumerate(layers):
        ents.append({"name": f"{name} blob {i}", "anchor": "span", "box": [round(v, 2) for v in blob_box(L)], "layers": [L]})
    return ents


# ------------------------------------------------------------------------------------------------------------ deliverables
def save_png(img, path):
    Image.fromarray(np.asarray(img, np.uint8)).save(path)


def deliver(scene, out_dir, screen, art_rgb, refs=None, zoom_rows=(150, 420), log=print):
    """All the comparison images of one screen (WIDE_BRIEF 'Deliver')."""
    import time
    os.makedirs(out_dir, exist_ok=True)
    t = time.time()
    g = unders(scene, refs)
    a43 = render(scene, 4 / 3, 1200, refs, under=g)
    art2 = np.asarray(Image.fromarray(np.asarray(art_rgb, np.uint8)).resize((1600, 1200), Image.NEAREST))
    save_png(np.concatenate([art2, np.full((1200, 8, 3), 255, np.uint8), a43], 1), os.path.join(out_dir, f"{screen}_43.png"))
    log(f"  43 {time.time() - t:.0f}s")
    for tag, a, hgt, half in (("169", 16 / 9, 1080, False), ("219", 21 / 9, 1080, False), ("329", 32 / 9, 540, True)):
        img = render(scene, a, hgt, refs, under=g)
        save_png(ticks(img, a, hgt / 600.0), os.path.join(out_dir, f"{screen}_{tag}.png"))
        log(f"  {tag} {time.time() - t:.0f}s")
    # 8x crops: across x = 0 and x = 800 at two heights, and far out on both sides
    crops = []
    cw, ch = 50, 50
    for (cx, cy, lab) in [(0, zoom_rows[0], "x=0"), (800, zoom_rows[0], "x=800"), (0, zoom_rows[1], "x=0"),
                          (800, zoom_rows[1], "x=800"), (-560, 300, "far left"), (1360, 300, "far right")]:
        img = render(scene, 32 / 9, 600, refs, origin_x=cx - cw / 2, width=cw * 8, s=8.0, y0=cy - ch / 2, h_px=ch * 8, under=g)
        im = Image.fromarray(img)
        d = ImageDraw.Draw(im)
        if lab.startswith("x="):
            d.line([(cw * 4, 0), (cw * 4, 12)], fill=(255, 0, 0), width=2)
        d.text((4, ch * 8 - 14), f"{lab} y={cy}", fill=(255, 0, 0))
        crops.append(np.asarray(im))
    row1 = np.concatenate([crops[0], np.full((400, 6, 3), 255, np.uint8), crops[1], np.full((400, 6, 3), 255, np.uint8), crops[4]], 1)
    row2 = np.concatenate([crops[2], np.full((400, 6, 3), 255, np.uint8), crops[3], np.full((400, 6, 3), 255, np.uint8), crops[5]], 1)
    save_png(np.concatenate([row1, np.full((6, row1.shape[1], 3), 255, np.uint8), row2], 0), os.path.join(out_dir, f"{screen}_zoom.png"))
    log(f"  zoom {time.time() - t:.0f}s")
