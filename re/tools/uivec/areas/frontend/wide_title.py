"""P41W: the title menu, wide (log/roadmap/P41/WIDE_BRIEF.md). Backdrop drawn: the mottled lilac ground (span), the stem line
(span), the giant dimpled ball, the pads' and the centre ellipse's cast shadows; the approved pads, buttons and centre ellipse
composed on top unchanged; the publisher marks and the logo stay art (mattes in the scene).

Usage: py -3.12 re/tools/uivec/areas/frontend/wide_title.py [--refit] [--only ground|ball|all]
Writes log/roadmap/P41/wide/title/ (title-wide.json, title_43/169/219/329/zoom.png) and caches fits in .../title/_fit/.
"""
import json
import math
import os
import sys
import time

import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
UIVEC = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, UIVEC)
import fit      # noqa: E402
import sdf      # noqa: E402
import wide     # noqa: E402

ROOT = fit.ROOT
OUT = os.path.join(ROOT, "log", "roadmap", "P41", "wide", "title")
CACHE = os.path.join(OUT, "_fit")
os.makedirs(CACHE, exist_ok=True)

GEOM = json.load(open(os.path.join(UIVEC, "title_geom.json")))
ELL = json.load(open(os.path.join(UIVEC, "scenes", "frontend", "title-centre-ellipse.json")))["measured"]

# measured here (2026-11-25, robust circle fit of the dark outline pixels, 1,148 points all round the visible arcs)
BALL_C = (403.63, 296.05)
BALL_LINE_R = 245.13          # centre of the 2 px navy outline
STEM_Y = 277.0                # centre of the navy stem (rows 276-277 navy, 275 its anti-aliased top, 278-281 its shadow)
CONT_SCALES = tuple(int(v) for v in os.environ.get("WT_SCALES", "90,64,45,32,23").split(","))
CONT_TILT = float(os.environ.get("WT_TILT", "1.6"))   # by eye at 32:9: the art's ground leans to big soft clouds
MARKS = (10, 496, 240, 90)    # EA / Maxis / Firaxis marks and the legal lines: x, y, w, h on TitleBASE (= frame)
LOGO_CELL = (0xa8, 0xc6, 0x1d8, 0xb0)    # TitleMO cell 13 (kTitleButtons item 6), the SimGolf lettering, keyed


def art():
    i, p = fit.load_sheet("Interface/TitleBASE.pcx")
    return p[i].astype(float)


def screen_art():
    """What the game shows at 4:3 (idle): TitleBASE, the TitleUnSel button cells over it, the logo (TitleMO cell 13)."""
    a = art()
    ui, up = fit.load_sheet("Interface/TitleUnSel.pcx")
    m = ui < 0xfe
    a[m] = up[ui][m]
    mi, mp = fit.load_sheet("Interface/TitleMO.pcx")
    x, y, w, h = LOGO_CELL
    sub = mi[y:y + h, x:x + w]
    k = sub < 0xfe
    a[y:y + h, x:x + w][k] = mp[sub][k]
    return a


def grid():
    ys, xs = np.mgrid[0:600, 0:800]
    return xs + 0.5, ys + 0.5


def pad_shapes():
    out = {}
    for k, m in GEOM.items():
        if "outer" in m:
            o = m["outer"]
            out[k] = {"t": "ellipse", "c": o[:2], "r": o[2:4], "rot": o[4]}
    return out


def ellipse_shape(grow=0.0):
    c = ELL["line"]
    return {"t": "ellipse", "c": c[:2], "r": [c[2] + grow, c[3] + grow], "rot": c[4]}


def ground_mask(margin=12.0):
    """Pixels where the art shows plain ground: off the pads (and their cast shadows), the ball, the centre ellipse, the stem,
    the marks and the exit disc."""
    x, y = grid()
    ok = np.ones((600, 800), bool)
    for S in pad_shapes().values():
        ok &= sdf.sd(S, x, y) > margin
    ok &= sdf.sd(ellipse_shape(), x, y) > 8
    ok &= np.hypot(x - BALL_C[0], y - BALL_C[1]) > BALL_LINE_R + 26
    ok &= ~((np.abs(y - STEM_Y) < 7))
    mx, my, mw, mh = MARKS
    ok &= ~((x > mx) & (x < mx + mw) & (y > my) & (y < my + mh)) | marks_gaps()
    e = GEOM["exit"]["idle"]["c"]
    ok &= np.hypot(x - e[0], y - e[1]) > e[2] + 8
    return ok


def marks_gaps(win=15, pct=80, tol=6.0):
    """Inside the marks' rectangle, the ground showing between the letters: the marks are darker lilac-grey strokes on the
    same hue, so a pixel is ground when its luminance is within tol of the local background (the pct-th percentile in a
    win x win window), and not next to a mark pixel."""
    a = art()
    mx, my, mw, mh = MARKS
    L = fit.lum(a)
    h = win // 2
    P = np.pad(L, h, mode="edge")
    sub = np.lib.stride_tricks.sliding_window_view(P, (win, win))[my:my + mh, mx:mx + mw]
    bg = np.percentile(sub.reshape(mh, mw, -1), pct, axis=-1)
    mark = L[my:my + mh, mx:mx + mw] < bg - tol
    near = mark.copy()
    for dy in (-1, 0, 1):
        for dx in (-1, 0, 1):
            near |= np.roll(np.roll(mark, dy, 0), dx, 1)
    out = np.zeros((600, 800), bool)
    out[my:my + mh, mx:mx + mw] = ~near
    return out


def ground_ramp(a, mask):
    L = fit.lum(a)
    m = mask & (L > 120)
    return wide.ramp_fit(a[m], L[m]), float(np.median(L[m]))


def fit_ground_title(refit=False, n1=300, n2=380, seed=41):
    path = os.path.join(CACHE, "ground.json")
    if os.path.exists(path) and not refit:
        return json.load(open(path))
    a = art()
    mask = ground_mask()
    coef, base_t = ground_ramp(a, mask)
    t = time.time()
    print(f"ground: {mask.sum()} visible px, base lum {base_t:.1f}")
    # pass 1: the inside alone, to learn the blob statistics
    p1 = os.path.join(CACHE, "ground_pass1.json")
    if os.path.exists(p1) and "--refit1" not in sys.argv:
        inside1 = json.load(open(p1))
    else:
        inside1, _ = wide.fit_ground(a, mask, coef, base_t, n_atoms=n1)
        json.dump(inside1, open(p1, "w"))
    outside, rep = wide.continue_ground_noise(inside1, coef, base_t, a, mask, seed, scales=CONT_SCALES, tilt=CONT_TILT)
    print(f"  pass 1 {time.time() - t:.0f}s, {len(outside)} continuation blobs")
    if "--cont-only" in sys.argv:
        return {"coef": coef, "base_t": base_t, "base_rgb": wide.ramp(coef, base_t), "outside": outside, "inside": inside1, "cont": rep}
    # pass 2: the inside again, over the continuation's tails
    inside, C = wide.fit_ground(a, mask, coef, base_t, fixed_layers=outside, n_atoms=n2)
    res = np.abs(fit.lum(a) - C)[mask]
    doc = {"coef": coef, "base_t": base_t, "base_rgb": wide.ramp(coef, base_t), "seed": seed, "cont": rep,
           "outside": outside, "inside": inside, "res_mean": float(res.mean()), "res_p95": float(np.percentile(res, 95))}
    json.dump(doc, open(path, "w"))
    print(f"  ground done {time.time() - t:.0f}s: {len(inside)} inside + {len(outside)} outside blobs, mean |res| {res.mean():.2f}")
    return doc


BALL_R = 257.5               # outer edge: the ground ends at row 38 above the centre (296.05 - 38.55); 256-258.5 all round
BALL_STOPS = [0.0, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0, 11.0, 11.8, 12.6, 13.4, 14.4, 15.5, 17.0, 19.0, 23.0,
              30.0, 45.0, 70.0, 110.0, 160.0]     # depth inside the outer edge, px (L = 1); deeper is hidden under the centre ellipse


def ground_doc():
    return json.load(open(os.path.join(CACHE, "ground.json")))


def drawn_ground(g=None):
    g = g or ground_doc()
    return wide.ground_1x({"draw": wide.ground_entries(g["base_rgb"], g["outside"] + g["inside"])})


def solve_bevel(rows, stops, lam_k=1e-3):
    """rows: (u, nx, ny, rgb) arrays; the bevel paint's coefficients by ridge least squares (accept.py's method)."""
    A = np.concatenate([np.stack([w for row in sdf.bevel_terms(u, nx, ny, stops) for w in row], axis=1) for u, nx, ny, _ in rows])
    Y = np.concatenate([rgb for _, _, _, rgb in rows])
    lam = lam_k * len(A) / A.shape[1]
    G = A.T @ A + lam * np.eye(A.shape[1])
    prior = np.zeros((A.shape[1], 3))
    prior[0::5] = Y.mean(0)
    X = np.linalg.solve(G, A.T @ Y + lam * prior)
    return [[[round(float(v), 2) for v in X[k * 5 + h]] for h in range(5)] for k in range(len(stops))]


def ball_free(x, y, pad_margin=15.0):
    """Ball pixels the art shows unshaded by anything in front (pads and their shadows, the centre ellipse, the load bubble)."""
    ok = np.hypot(x - BALL_C[0], y - BALL_C[1]) < BALL_R - 0.5
    for S in pad_shapes().values():
        ok &= sdf.sd(S, x, y) > pad_margin
    ok &= sdf.sd(ellipse_shape(), x, y) > 7
    return ok


def fit_ball_bevel():
    a = art()
    x, y = grid()
    S = {"t": "circle", "c": list(BALL_C), "r": BALL_R}
    d = sdf.sd(S, x, y)
    nx, ny = sdf.normal(S, x, y, d)
    m = ball_free(x, y)
    coef = solve_bevel([(-d[m], nx[m], ny[m], a[m])], BALL_STOPS)
    return S, coef, m


# ---------------------------------------------------------------------------------------------------------------- dimples
# The ball's dimples (read on the art, 2026-11-25): round cups in a regular hexagonal lattice ON THE SPHERE. Unwarping the
# ball (azimuthal equidistant about the disc centre: a point at arc distance rho from the centre of the visible face shows at
# screen radius Rs sin(rho / Rs)) turns them into a flat lattice of round cups; a regular hex lattice of neighbour distance
# 33.5 px with rows at 107 / 47 / 167 degrees fits the whole visible ball as well as the best skewed lattice does (fold
# variance 4.26 against 4.19). Each cup is lit from above: on screen a dark upper wall and a light lower wall, i.e. a linear
# ramp across the cup (a spherical cup's shading is linear in height), in its foreshortened outline (radial axis x cos).
DIMPLE_RS = 245.0
DIMPLE_A = 33.5
DIMPLE_TURN = 107.0


def dimple_list(phase, rd_k, a=DIMPLE_A, turn=DIMPLE_TURN, rs=DIMPLE_RS, th_max=1.45):
    """(screen centre, radial semi-axis, tangential semi-axis, rotation deg, theta) of every cup on the visible face."""
    out = []
    t1, t2 = np.deg2rad(turn), np.deg2rad(turn - 60.0)
    # lattice basis: neighbour vectors along the rows
    e1 = np.array([np.cos(t1 + np.pi / 2), np.sin(t1 + np.pi / 2)]) * a
    e2 = np.array([np.cos(t2 + np.pi / 2), np.sin(t2 + np.pi / 2)]) * a
    n = int(rs * np.pi / 2 / a * 1.3) + 2
    rd = rd_k * a
    for i in range(-n, n + 1):
        for j in range(-n, n + 1):
            q = (i + phase[0]) * e1 + (j + phase[1]) * e2
            rho = float(np.hypot(*q))
            th = rho / rs
            if th > th_max:
                continue
            u = q / max(rho, 1e-9)
            r = rs * np.sin(th)
            c = (BALL_C[0] + u[0] * r, BALL_C[1] + u[1] * r)
            rot = float(np.degrees(np.arctan2(u[1], u[0])))
            out.append((c, rd * np.cos(th), rd, rot, th))
    return out


def dimple_offset(x, y, cups, g_top, g_bot, soft, light_deg, fall):
    """The luminance offset the cups add to the body (what the scene layers paint), for fitting."""
    off = np.zeros(x.shape)
    ld = np.deg2rad(light_deg)
    lx, ly = np.cos(ld), np.sin(ld)          # direction toward the light (screen), -90 = straight up
    for c, ra, ta, rot, th in cups:
        if ra < 0.6:
            continue
        m = ta + soft + 1
        i0, i1 = max(0, int(c[0] - m)), min(x.shape[1], int(c[0] + m) + 1)
        j0, j1 = max(0, int(c[1] - m)), min(x.shape[0], int(c[1] + m) + 1)
        if i1 <= i0 or j1 <= j0:
            continue
        xx, yy = x[j0:j1, i0:i1], y[j0:j1, i0:i1]
        d = sdf.sd({"t": "ellipse", "c": list(c), "r": [ra, ta], "rot": rot}, xx, yy)
        al = np.clip(0.5 - d / (2 * soft), 0, 1)
        al = al * al * (3 - 2 * al)
        h = extent_along(ra, ta, rot, lx, ly)
        t = ((xx - c[0]) * lx + (yy - c[1]) * ly) / max(h, 1e-6)     # +1 at the wall facing away from the light
        k = np.cos(th) ** fall
        val = np.where(t > 0, -g_top * t, -g_bot * t) * k             # the wall nearer the light is in shade
        off[j0:j1, i0:i1] = off[j0:j1, i0:i1] * (1 - al) + val * al
    return off


def extent_along(ra, ta, rot, lx, ly):
    """Half the extent of an ellipse (semi-axes ra along rot, ta across) along the unit direction (lx, ly)."""
    r = np.deg2rad(rot)
    ux, uy = np.cos(r), np.sin(r)
    a1 = lx * ux + ly * uy
    a2 = -lx * uy + ly * ux
    return float(np.sqrt((ra * a1) ** 2 + (ta * a2) ** 2))


def fit_dimples(log=print):
    """Lattice (spacing, turn, phase) and cup look fitted to the ball's residual after the bevel (art - bevel), on ball pixels
    clear of everything in front and 10 px inside the outline (the rim's own residual is not dimples)."""
    hp = np.load(os.path.join(CACHE, "ball_hp.npy"))
    x, y = grid()
    m = ball_free(x, y) & (np.hypot(x - BALL_C[0], y - BALL_C[1]) < BALL_LINE_R - 10)
    tgt = hp[m]
    names = ["p0", "p1", "a", "turn", "rd_k", "g_top", "g_bot", "soft", "light", "fall"]

    def err(p):
        cups = dimple_list((p["p0"], p["p1"]), p["rd_k"], a=p["a"], turn=p["turn"])
        o = dimple_offset(x, y, cups, p["g_top"], p["g_bot"], p["soft"], p["light"], p["fall"])[m]
        return float(((tgt - o) ** 2).mean())

    look = {"rd_k": 0.38, "g_top": 10.0, "g_bot": 8.0, "soft": 7.5, "light": -75.0, "fall": 1.0}
    best = None
    for a in (33.0, 34.0, 35.0):
        for turn in (104.0, 107.0, 110.0):
            for p0 in np.arange(0, 1, 0.1):
                for p1 in np.arange(0, 1, 0.1):
                    p = dict(look, a=a, turn=turn, p0=p0, p1=p1)
                    e = err(p)
                    if best is None or e < best[0]:
                        best = (e, p)
        log(f"  dimples coarse a={a}: best {best[1]} mse {best[0]:.2f} (var {tgt.var():.2f})")
    e0, p = best
    steps = {"p0": 0.04, "p1": 0.04, "a": 0.4, "turn": 1.0, "rd_k": 0.03, "g_top": 1.5, "g_bot": 1.5, "soft": 1.0,
             "light": 5.0, "fall": 0.3}
    for rnd in range(5):
        for k in names:
            for sgn in (-1, 1):
                for _ in range(8):
                    q = dict(p)
                    q[k] = p[k] + sgn * steps[k]
                    if k == "soft" and q[k] < 0.5:
                        break
                    e = err(q)
                    if e < e0:
                        e0, p = e, q
                    else:
                        break
        steps = {k: v / 2 for k, v in steps.items()}
        log(f"  dimples round {rnd}: { {k: round(float(v), 3) for k, v in p.items()} } mse {e0:.2f}")
    out = {k: float(v) for k, v in p.items()}
    out.update(mse=e0, var=float(tgt.var()))
    return out


def ball_entries():
    """The giant ball as scene entries: the lit disc (one bevel layer: raised rim lit from above, the navy outline with its
    light inner edge, the body shading) and one layer per dimple (a linear ramp from the shaded upper wall to the lit lower
    wall, in the cup's foreshortened outline, soft edge, painted over the body's own colour at that spot; clipped to the
    body inside the outline)."""
    bv = json.load(open(os.path.join(CACHE, "ball_bevel.json")))
    dp = json.load(open(os.path.join(CACHE, "dimples.json")))
    S = bv["shape"]
    paint = {"bevel": {"L": 1.0, "stops": bv["stops"], "coef": bv["coef"]}}
    R = S["r"]
    disc = {"name": "giant ball", "anchor": "centre", "box": [S["c"][0] - R - 2, S["c"][1] - R - 2, S["c"][0] + R + 2, S["c"][1] + R + 2],
            "layers": [{"shape": S, "fill": paint}]}
    body_clip = {"t": "circle", "c": S["c"], "r": BALL_LINE_R - 2.2}
    cups = dimple_list((dp["p0"], dp["p1"]), dp["rd_k"], a=dp["a"], turn=dp["turn"])
    ld = np.deg2rad(dp["light"])
    lx, ly = float(np.cos(ld)), float(np.sin(ld))
    layers = []
    for c, ra, ta, rot, th in cups:
        if ra < 0.8 or np.hypot(c[0] - S["c"][0], c[1] - S["c"][1]) > BALL_LINE_R + ta:
            continue
        body = np.asarray(sdf.paint(paint, np.array([c[0]]), np.array([c[1]]), None, S))[0]
        Lb = float(fit.lum(body))
        k = float(np.cos(th) ** dp["fall"])
        dark = body * (Lb - dp["g_top"] * k) / Lb
        light = body * (Lb + dp["g_bot"] * k) / Lb
        h = extent_along(ra, ta, rot, lx, ly)
        layers.append({"shape": {"t": "ellipse", "c": [round(c[0], 2), round(c[1], 2)], "r": [round(ra, 2), round(ta, 2)], "rot": round(rot, 2)},
                       "soft": round(dp["soft"], 2), "clip": body_clip,
                       "fill": {"linear": {"a": [round(c[0] + lx * h, 2), round(c[1] + ly * h, 2)],
                                           "b": [round(c[0] - lx * h, 2), round(c[1] - ly * h, 2)],
                                           "stops": [[0.0, [round(v, 1) for v in dark]], [0.5, [round(v, 1) for v in body]],
                                                     [1.0, [round(v, 1) for v in light]]]}}})
    ents = [disc]
    for i, L in enumerate(layers):
        S2 = L["shape"]
        m = S2["r"][1] + L["soft"] + 1
        ents.append({"name": f"ball dimple {i}", "anchor": "centre",
                     "box": [S2["c"][0] - m, S2["c"][1] - m, S2["c"][0] + m, S2["c"][1] + m], "layers": [L]})
    return ents


# ---------------------------------------------------------------------------------------------------------------- shadows
SHADOW_RGB = [24.0, 26.0, 70.0]


def shadow_layer(shape, p):
    """A soft cast shadow: the caster's shape moved by (dx, dy), grown by g, its edge softened over +-soft, navy at alpha."""
    return {"shape": {"t": "grow", "of": {"t": "move", "of": shape, "d": [round(p["dx"], 2), round(p["dy"], 2)]}, "d": round(p["g"], 2)},
            "soft": round(max(p["soft"], 0.3), 2), "fill": SHADOW_RGB, "alpha": round(p["alpha"], 3)}


def under_1x(with_ball=True):
    img = drawn_ground()
    if with_ball:
        sdf.render(ball_entries(), img, 1.0)
    return img


def title_refs():
    """The approved scenes this screen composes: title.py's six pads and buttons (idle) and the centre ellipse."""
    import title
    return {"title.py:idle": title.scene(title.measure(verbose=False), False),
            "title.py:pads": title.scene(title.measure(verbose=False), False, parts=("pad",))}


def overlay_1x(items):
    """(alpha, premultiplied colour) of items rendered at 1x, from renders over black and white."""
    r0 = np.zeros((600, 800, 3))
    r1 = np.full((600, 800, 3), 255.0)
    sdf.render(items, r0, 1.0)
    sdf.render(items, r1, 1.0)
    A = 1 - (r1 - r0) / 255.0
    return A, r0


def stem_layer():
    """The stem: the approved centre ellipse's own stem segments (navy 33,43,79, 2 px at y 277) run on to x = +-2000."""
    return {"shape": {"t": "seg", "a": [wide.SPAN_X[0], STEM_Y], "b": [wide.SPAN_X[1], STEM_Y], "w": 2.0}, "fill": [33, 43, 79]}


def fit_shadow(shapes, band, U, a, p0, over, log=print, name=""):
    """Shared shadow parameters for a family of casters, fitted on the band pixels: art against U with the shadows, then
    the casters themselves (over = their 1x alpha and premultiplied colour) on top."""
    ys, xs = np.nonzero(band)
    X, Y = xs + 0.5, ys + 0.5
    Ub, Ab = U[band], a[band]
    OA, OC = over[0][band], over[1][band]

    def render(p):
        acc = Ub.copy()
        for S in shapes:
            L = shadow_layer(S, p)
            d = sdf.sd(L["shape"], X, Y)
            u = np.clip(0.5 - d / (2 * L["soft"]), 0, 1)
            k = (u * u * (3 - 2 * u) * L["alpha"])[:, None]
            acc = acc * (1 - k) + np.asarray(SHADOW_RGB) * k
        return acc * (1 - OA) + OC

    def err(p):
        return float(((render(p) - Ab) ** 2).mean())

    p, e0 = dict(p0), err(p0)
    steps = {"dx": 1.0, "dy": 1.0, "g": 1.0, "soft": 1.0, "alpha": 0.08}
    for rnd in range(5):
        for k in steps:
            for sgn in (-1, 1):
                for _ in range(10):
                    q = dict(p)
                    q[k] = p[k] + sgn * steps[k]
                    if q["soft"] < 0.3 or not (0 <= q["alpha"] <= 1):
                        break
                    e = err(q)
                    if e < e0:
                        e0, p = e, q
                    else:
                        break
        steps = {k: v / 2 for k, v in steps.items()}
    log(f"  shadow {name}: {{{', '.join(f'{k}: {v:.2f}' for k, v in p.items())}}} rms {np.sqrt(e0):.2f} (no shadow {np.sqrt(err(dict(p, alpha=0.0))):.2f}, {band.sum()} px)")
    return p


ELL_CASTER = {"t": "ellipse", "c": [400.11, 275.63 + 0.45], "r": [315.24 + 1.2, 104.61 + 1.2], "rot": -0.04}   # the approved outline


def fit_shadows(log=print):
    a = art()
    x, y = grid()
    U = under_1x()
    refs = title_refs()
    ell_items = [it for it in json.loads(json.dumps(wide.load_ref("title-centre-ellipse")))]
    for it in ell_items:
        it["layers"] = it["layers"][2:]          # its stem segments are replaced by the span stem
    over = overlay_1x([{"box": [-10, 260, 810, 295], "layers": [stem_layer()]}] + ell_items + refs["title.py:pads"])
    pads = pad_shapes()
    ell = ELL_CASTER
    mx, my, mw, mh = MARKS
    marks = (x > mx - 4) & (x < mx + mw + 4) & (y > my - 4) & (y < my + mh + 4)
    d_pads = np.min([sdf.sd(S, x, y) for S in pads.values()], 0)
    d_ell = sdf.sd(ell, x, y)
    e = GEOM["exit"]["idle"]["c"]
    clear = ~marks & (np.hypot(x - e[0], y - e[1]) > e[2] + 6)
    out = {}
    band = (d_pads > -0.5) & (d_pads < 14) & (d_ell > 14) & clear
    out["pads"] = fit_shadow(list(pads.values()), band, U, a, {"dx": 2.0, "dy": 3.0, "g": 0.0, "soft": 4.0, "alpha": 0.5}, over, log, "pads")
    band = (d_ell > 2.5) & (d_ell < 12) & (d_pads > 14) & clear & (np.abs(y - STEM_Y) > 8)   # the art's own outline lies 1-2 px outside the approved one
    out["ellipse"] = fit_shadow([ell], band, U, a, {"dx": 0.0, "dy": 2.0, "g": 0.0, "soft": 2.0, "alpha": 0.5}, over, log, "centre ellipse")
    stem = stem_layer()["shape"]
    band = (np.abs(y - STEM_Y) < 9) & (d_ell > 3) & clear
    out["stem"] = fit_shadow([stem], band, U, a, {"dx": 0.0, "dy": 2.0, "g": 0.0, "soft": 2.0, "alpha": 0.5}, over, log, "stem")
    ball = {"t": "circle", "c": list(BALL_C), "r": BALL_R}
    d_ball = sdf.sd(ball, x, y)
    band = (d_ball > 0.7) & (d_ball < 14) & (d_pads > 16) & (d_ell > 10) & clear
    out["ball"] = fit_shadow([ball], band, drawn_ground(), a, {"dx": 2.0, "dy": 2.0, "g": 0.0, "soft": 3.0, "alpha": 0.3},
                             (np.zeros_like(a), np.zeros_like(a)), log, "ball")
    out["casters"] = {"ellipse": ell, "stem": stem, "ball": ball}
    return out


# The navy lines (stem, centre ellipse outline) cast the same short shadow below them, read off the art's rows (stem at x
# 10 / 40 / 760 / 790, ellipse at x 400): alpha toward SHADOW_RGB about .37 / .33 / .17 / .06 on the four rows under the
# line's lower edge, nothing above it. The numeric fit chased the art's own ellipse outline (1-2 px outside the approved one).
LINE_SHADOW = {"dx": 0.3, "dy": 1.6, "g": 0.0, "soft": 1.6, "alpha": 0.45}


if __name__ == "__main__" and "--shadows" in sys.argv:
    r = fit_shadows()
    r["fit_ellipse"], r["fit_stem"] = r["ellipse"], r["stem"]
    r["ellipse"], r["stem"] = dict(LINE_SHADOW), dict(LINE_SHADOW)
    json.dump(r, open(os.path.join(CACHE, "shadows.json"), "w"), indent=1)
    sys.exit(0)


if __name__ == "__main__" and "--ballview" in sys.argv:
    a = art()
    img = drawn_ground()
    sdf.render(ball_entries(), img, 1.0)
    from PIL import Image as _I
    big = lambda z, s=3: np.asarray(_I.fromarray(z.clip(0, 255).astype(np.uint8)).resize((800 * s, 600 * s), _I.NEAREST))
    img3 = np.zeros((1800, 2400, 3))
    sdf.render([{"box": [-50, -50, 900, 700], "layers": [{"shape": {"t": "circle", "c": [0, 0], "r": 3000}, "fill": [0, 0, 0]}]}], img3, 3.0)
    img3[:] = np.asarray(_I.fromarray(drawn_ground().clip(0, 255).astype(np.uint8)).resize((2400, 1800), _I.BILINEAR), float)
    sdf.render(ball_entries(), img3, 3.0)
    A = big(a)
    wide.save_png(np.concatenate([A[1100:1650, 450:1350], img3[1100:1650, 450:1350].clip(0, 255).astype(np.uint8)], 1), os.path.join(OUT, "_ball_bot3.png"))
    wide.save_png(np.concatenate([A[90:520, 700:1500], img3[90:520, 700:1500].clip(0, 255).astype(np.uint8)], 1), os.path.join(OUT, "_ball_top3.png"))
    wide.save_png(np.concatenate([a, img], 1).clip(0, 255).astype(np.uint8), os.path.join(OUT, "_ball_cmp.png"))
    sys.exit(0)


if __name__ == "__main__" and "--dimples" in sys.argv:
    t0 = time.time()
    r = fit_dimples()
    json.dump(r, open(os.path.join(CACHE, "dimples.json"), "w"))
    print(r, f"{time.time() - t0:.0f}s")
    sys.exit(0)


if __name__ == "__main__" and "--ball" in sys.argv:
    S, coef, m = fit_ball_bevel()
    json.dump({"shape": S, "stops": BALL_STOPS, "coef": coef}, open(os.path.join(CACHE, "ball_bevel.json"), "w"))
    a = art()
    G = drawn_ground()
    img = G.copy()
    sdf.render([{"box": [130, 30, 680, 560], "layers": [{"shape": S, "fill": {"bevel": {"L": 1.0, "stops": BALL_STOPS, "coef": coef}}}]}], img, 1.0)
    vis = np.where(m[..., None], a, img)
    res = np.abs(fit.lum(a) - fit.lum(img))[m]
    print("ball bevel: mean |res|", res.mean().round(2), "p95", np.percentile(res, 95).round(2))
    from PIL import Image as _I
    big = lambda z: np.asarray(_I.fromarray(z.clip(0, 255).astype(np.uint8)).resize((1600, 1200), _I.NEAREST))
    wide.save_png(np.concatenate([big(a)[0:500, 500:1100], big(img)[0:500, 500:1100]], 1), os.path.join(OUT, "_ball_top.png"))
    wide.save_png(np.concatenate([a, img], 1).clip(0, 255).astype(np.uint8), os.path.join(OUT, "_ball_cmp.png"))
    hp = (fit.lum(a) - fit.lum(img)) * m
    np.save(os.path.join(CACHE, "ball_hp.npy"), hp)
    sys.exit(0)


MARKS_MATTE = {"kind": "ground_diff", "t0": 9.0, "t1": 30.0, "feather": 4.0}


def build_scene():
    """The title menu's wide scene (WIDE_BRIEF): every entry's anchor, the approved elements by reference."""
    g = ground_doc()
    sh = json.load(open(os.path.join(CACHE, "shadows.json")))
    pads = pad_shapes()
    draw = wide.ground_entries(g["base_rgb"], g["outside"] + g["inside"])
    stem_sh = shadow_layer(stem_layer()["shape"], sh["stem"])
    draw.append({"name": "stem (shadow, line)", "anchor": "span", "box": [wide.SPAN_X[0], 270.0, wide.SPAN_X[1], 286.0],
                 "layers": [stem_sh, stem_layer()]})
    R = BALL_R
    draw.append({"name": "giant ball shadow", "anchor": "centre", "box": [BALL_C[0] - R - 12, BALL_C[1] - R - 12, BALL_C[0] + R + 14, BALL_C[1] + R + 14],
                 "layers": [shadow_layer(sh["casters"]["ball"], sh["ball"])]})
    draw += ball_entries()
    for k, S in pads.items():
        c, r = S["c"], S["r"]
        m = max(r) + 16
        draw.append({"name": f"{k} pad shadow", "anchor": "centre", "box": [c[0] - m, c[1] - m, c[0] + m, c[1] + m],
                     "layers": [shadow_layer(S, sh["pads"])]})
    E = ELL_CASTER
    draw.append({"name": "centre ellipse shadow", "anchor": "centre",
                 "box": [E["c"][0] - E["r"][0] - 8, E["c"][1] - E["r"][1] - 8, E["c"][0] + E["r"][0] + 8, E["c"][1] + E["r"][1] + 8],
                 "layers": [shadow_layer(E, sh["ellipse"])]})
    draw.append({"name": "centre ellipse", "anchor": "centre", "ref": "title-centre-ellipse", "omit": [[0, 0], [0, 1]],
                 "note": "layers 0 and 1 are its two stem segments (0..800 at y 277); the span stem above is the same line run on"})
    mx, my, mw, mh = MARKS
    draw.append({"name": "publisher marks", "anchor": "centre",
                 "picture": {"sheet": "Interface/TitleBASE.pcx", "rect": [mx, my, mw, mh], "at": [mx, my], "matte": dict(MARKS_MATTE)}})
    draw.append({"name": "pads and buttons", "anchor": "centre", "ref": "title.py:idle",
                 "note": "title.scene(title.measure(), hover) for the six approved pads and bubbles (hover state: title.py:hover)"})
    x, y, w, h = LOGO_CELL
    draw.append({"name": "SimGolf logo", "anchor": "centre",
                 "picture": {"sheet": "Interface/TitleMO.pcx", "rect": [x, y, w, h], "at": [x, y], "matte": {"kind": "key"}}})
    return {"id": "title-wide", "screen": "Title menu", "design_x": list(wide.DESIGN_X), "frame": [800, 600],
            "ground": {"ramp": g["coef"], "base_t": g["base_t"], "inside_blobs": len(g["inside"]), "outside_blobs": len(g["outside"]),
                       "continuation": {k: v for k, v in g.get("cont", {}).items() if k != "amp"},
                       "fit_residual_lum": [round(g["res_mean"], 2), round(g["res_p95"], 2)]},
            "draw": draw}


if __name__ == "__main__" and "--matte" in sys.argv:
    sc = build_scene()
    refs = title_refs()
    i = [k for k, e in enumerate(sc["draw"]) if e.get("name") == "publisher marks"][0]
    U = wide.under_1x(sc, refs, i)
    a = art()
    mx, my, mw, mh = MARKS
    A, G = a[my:my + mh, mx:mx + mw], U[my:my + mh, mx:mx + mw]
    d = np.sqrt(((A - G) ** 2).sum(-1))
    print("dist percentiles", np.percentile(d, [10, 25, 50, 75, 90, 95, 99]).round(1))
    al, col = wide.matte(A, G, MARKS_MATTE["t0"], MARKS_MATTE["t1"])
    from PIL import Image as _I
    z = lambda im: np.asarray(_I.fromarray(np.clip(im, 0, 255).astype(np.uint8)).resize((mw * 4, mh * 4), _I.NEAREST))
    comp = G * (1 - al[..., None]) + col * al[..., None]
    wide.save_png(np.concatenate([z(A), z(np.repeat(al[..., None] * 255, 3, -1)), z(comp)], 0), os.path.join(OUT, "_marks_matte.png"))
    sys.exit(0)


if __name__ == "__main__" and "--deliver" in sys.argv:
    t0 = time.time()
    sc = build_scene()
    json.dump(sc, open(os.path.join(OUT, "title-wide.json"), "w"))
    print(f"scene: {len(sc['draw'])} entries, {os.path.getsize(os.path.join(OUT, 'title-wide.json')) // 1024} KB")
    wide.deliver(sc, OUT, "title", screen_art(), title_refs(), zoom_rows=(140, 470))
    print(f"delivered in {time.time() - t0:.0f}s")
    sys.exit(0)


if __name__ == "__main__":
    args = sys.argv[1:]
    g = fit_ground_title("--refit" in args or "--cont-only" in args)
    ents = wide.ground_entries(g["base_rgb"], g["outside"] + g["inside"])
    scene = {"id": "t", "draw": ents}
    img = wide.render(scene, 21 / 9, 600)
    wide.save_png(wide.ticks(img, 21 / 9, 1.0), os.path.join(OUT, "_ground_219.png"))
    a = art()
    g1 = wide.ground_1x(scene)
    m = ground_mask()
    vis = np.where(m[..., None], a, g1)
    wide.save_png(np.concatenate([a, g1, vis], 1).clip(0, 255).astype(np.uint8), os.path.join(OUT, "_ground_cmp.png"))
