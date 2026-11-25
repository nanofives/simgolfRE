"""P41W: what every front-end screen's wide version shares, beyond re/tools/uivec/wide.py (which holds the canvas, anchors,
ground blobs, mattes and the deliverable images). The title menu's own module (wide_title.py) came first; the pieces here are
its methods with the screen as a parameter:

- art / approved-element coverage, the plain-ground mask (lilac cloud pixels clear of everything);
- the two-pass ground fit (inside blobs, statistical continuation outside), cached per screen;
- bevel least squares (accept.py's method) for slabs and rims;
- a lit sphere as a bevel paint: a shading quadratic in n.L (n the sphere's normal, L a 3D light) is exactly a bevel paint
  over the circle (per depth stop: constant + nx, ny + the two second harmonics), so a ball cut off by the art's edge is
  drawn whole at any width with the same light;
- dimples on a sphere (hex lattice unwarped azimuthal-equidistant, cups lit from one side), fitted like the title's;
- soft cast shadows.
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
sys.path.insert(0, HERE)
import fit      # noqa: E402
import sdf      # noqa: E402
import wide     # noqa: E402

ROOT = fit.ROOT
WIDE = os.path.join(ROOT, "log", "roadmap", "P41", "wide")
SHADOW_RGB = [24.0, 26.0, 70.0]


def dirs(screen):
    out = os.path.join(WIDE, screen)
    cache = os.path.join(out, "_fit")
    os.makedirs(cache, exist_ok=True)
    return out, cache


def sheet(rel):
    i, p = fit.load_sheet(rel)
    return p[i].astype(float), i


def grid():
    ys, xs = np.mgrid[0:600, 0:800]
    return xs + 0.5, ys + 0.5


def coverage(items, grow=0):
    """Pixels the items paint at 1x (alpha > 0.02), grown by `grow` px."""
    r0 = np.zeros((600, 800, 3))
    r1 = np.full((600, 800, 3), 255.0)
    sdf.render(items, r0, 1.0)
    sdf.render(items, r1, 1.0)
    A = (1 - (r1 - r0) / 255.0).max(-1) > 0.02
    return dilate(A, grow)


def overlay(items):
    r0 = np.zeros((600, 800, 3))
    r1 = np.full((600, 800, 3), 255.0)
    sdf.render(items, r0, 1.0)
    sdf.render(items, r1, 1.0)
    return 1 - (r1 - r0) / 255.0, r0


def dilate(m, r):
    m = m.copy()
    for _ in range(int(r)):
        n = m.copy()
        n[1:] |= m[:-1]
        n[:-1] |= m[1:]
        n[:, 1:] |= m[:, :-1]
        n[:, :-1] |= m[:, 1:]
        m = n
    return m


def cloud_pixels(a, lum_min=150.0, chroma_min=28.0):
    """The lilac cloud ground: light and blue-tinted (B - R large); navy slabs, cream panels and grey marks are not."""
    L = fit.lum(a)
    return (L > lum_min) & ((a[..., 2] - a[..., 0]) > chroma_min) & (a[..., 2] > 200)


# ------------------------------------------------------------------------------------------------------------ ground
def fit_ground(screen, a, mask, refit=False, n1=300, n2=380, seed=41, scales=(90, 64, 45, 32, 23), tilt=1.6, log=print):
    out, cache = dirs(screen)
    path = os.path.join(cache, "ground.json")
    if os.path.exists(path) and not refit:
        return json.load(open(path))
    L = fit.lum(a)
    m = mask & (L > 120)
    coef = wide.ramp_fit(a[m], L[m])
    base_t = float(np.median(L[m]))
    t = time.time()
    log(f"{screen} ground: {mask.sum()} visible px, base lum {base_t:.1f}")
    inside1, _ = wide.fit_ground(a, mask, coef, base_t, n_atoms=n1, log=log)
    t_light = float(min(254.0, max(236.0, np.percentile(L[mask], 99) + 6)))
    outside, rep = wide.continue_ground_noise(inside1, coef, base_t, a, mask, seed, scales=scales, tilt=tilt, log=lambda s: None,
                                              t_light=t_light)
    rep["t_light"] = t_light
    inside, C = wide.fit_ground(a, mask, coef, base_t, fixed_layers=outside, n_atoms=n2, log=lambda s: None)
    res = np.abs(L - C)[mask]
    doc = {"coef": coef, "base_t": base_t, "base_rgb": wide.ramp(coef, base_t), "seed": seed, "cont": rep, "outside": outside,
           "inside": inside, "res_mean": float(res.mean()), "res_p95": float(np.percentile(res, 95)), "mask_px": int(mask.sum())}
    json.dump(doc, open(path, "w"))
    log(f"  {screen} ground {time.time() - t:.0f}s: {len(inside)} inside + {len(outside)} outside blobs, mean |res| {res.mean():.2f}")
    return doc


def ground_entries(g):
    return wide.ground_entries(g["base_rgb"], g["outside"] + g["inside"])


def ground_meta(g):
    return {"ramp": g["coef"], "base_t": g["base_t"], "inside_blobs": len(g["inside"]), "outside_blobs": len(g["outside"]),
            "continuation": {k: v for k, v in g.get("cont", {}).items() if k != "amp"},
            "fit_residual_lum": [round(g["res_mean"], 2), round(g["res_p95"], 2)]}


# ------------------------------------------------------------------------------------------------------------ bevels
def solve_bevel(rows, stops, lam_k=1e-3):
    A = np.concatenate([np.stack([w for row in sdf.bevel_terms(u, nx, ny, stops) for w in row], axis=1) for u, nx, ny, _ in rows])
    Y = np.concatenate([rgb for _, _, _, rgb in rows])
    lam = lam_k * len(A) / A.shape[1]
    G = A.T @ A + lam * np.eye(A.shape[1])
    prior = np.zeros((A.shape[1], 3))
    prior[0::5] = Y.mean(0)
    X = np.linalg.solve(G, A.T @ Y + lam * prior)
    return [[[round(float(v), 2) for v in X[k * 5 + h]] for h in range(5)] for k in range(len(stops))]


def fit_bevel(shape, a, mask, stops):
    """The bevel paint of a shape fitted on the masked art pixels (depth in px: L = 1)."""
    x, y = grid()
    d = sdf.sd(shape, x, y)
    nx, ny = sdf.normal(shape, x, y, d)
    m = mask & (d < 0)
    return solve_bevel([(-d[m], nx[m], ny[m], a[m])], stops)


def bevel_render(shape, coef, stops, base):
    img = base.copy()
    sdf.render([{"box": [-10, -10, 810, 610], "layers": [{"shape": shape, "fill": {"bevel": {"L": 1.0, "stops": stops, "coef": coef}}}]}], img, 1.0)
    return img


# ------------------------------------------------------------------------------------------------------------ spheres
def sphere_terms(c, R, x, y):
    """3D unit normal of the sphere (centre c, radius R) seen from the front at each pixel inside it."""
    px, py = (x - c[0]) / R, (y - c[1]) / R
    rho2 = np.clip(px * px + py * py, 0, 1)
    return px, py, np.sqrt(1 - rho2)


def fit_sphere(a, mask, c, R):
    """Shading of a ball: colour = a0 + a1 t + a2 t^2 per channel with t = n.L (L a unit 3D light found by search). Returns
    (light, coefficients 3 x 3, rms)."""
    x, y = grid()
    nx, ny, nz = sphere_terms(c, R, x, y)
    m = mask & ((x - c[0]) ** 2 + (y - c[1]) ** 2 < (R - 2) ** 2)
    N = np.stack([nx[m], ny[m], nz[m]], 1)
    Y = a[m]
    best = None
    for az in range(0, 360, 10):
        for el in range(10, 90, 8):
            L = np.array([math.cos(math.radians(el)) * math.cos(math.radians(az)),
                          math.cos(math.radians(el)) * math.sin(math.radians(az)), math.sin(math.radians(el))])
            t = N @ L
            A = np.stack([np.ones_like(t), t, t * t], 1)
            X, res, *_ = np.linalg.lstsq(A, Y, rcond=None)
            e = float(((A @ X - Y) ** 2).mean())
            if best is None or e < best[0]:
                best = (e, L, X)
    e, L, X = best
    # refine the light by small steps
    for step in (4.0, 2.0, 1.0):
        az = math.degrees(math.atan2(L[1], L[0]))
        el = math.degrees(math.asin(L[2]))
        improved = True
        while improved:
            improved = False
            for daz, dele in ((step, 0), (-step, 0), (0, step), (0, -step)):
                a2, e2 = az + daz, min(89.0, max(1.0, el + dele))
                L2 = np.array([math.cos(math.radians(e2)) * math.cos(math.radians(a2)),
                               math.cos(math.radians(e2)) * math.sin(math.radians(a2)), math.sin(math.radians(e2))])
                t = N @ L2
                A = np.stack([np.ones_like(t), t, t * t], 1)
                X2 = np.linalg.lstsq(A, Y, rcond=None)[0]
                ee = float(((A @ X2 - Y) ** 2).mean())
                if ee < e:
                    e, L, X, az, el, improved = ee, L2, X2, a2, e2, True
    return [float(v) for v in L], [[float(v) for v in row] for row in X], math.sqrt(e)


def sphere_bevel(R, light, X, stops):
    """The bevel coefficients (L = 1, depth in px) that paint colour = X0 + X1 t + X2 t^2, t = n.L, on a circle of radius R.
    At depth u: rho = 1 - u / R, nz = sqrt(1 - rho^2), and with the circle's outward normal (nx, ny) the sphere normal is
    (rho nx, rho ny, nz): t = rho (Lx nx + Ly ny) + nz Lz; t^2 = rho^2 [(Lx^2 + Ly^2) / 2 + (Lx^2 - Ly^2) / 2 (nx^2 - ny^2)
    + Lx Ly 2 nx ny] + 2 rho nz Lz (Lx nx + Ly ny) + nz^2 Lz^2 (nx^2 + ny^2 = 1)."""
    Lx, Ly, Lz = light
    X = np.asarray(X)
    out = []
    for u in stops:
        rho = max(0.0, 1.0 - u / R)
        nz = math.sqrt(max(0.0, 1 - rho * rho))
        t_c, t_x, t_y = nz * Lz, rho * Lx, rho * Ly
        q_c = rho * rho * (Lx * Lx + Ly * Ly) / 2 + nz * nz * Lz * Lz
        q_x, q_y = 2 * rho * nz * Lz * Lx, 2 * rho * nz * Lz * Ly
        q_h1, q_h2 = rho * rho * (Lx * Lx - Ly * Ly) / 2, rho * rho * Lx * Ly
        terms = [X[0] + X[1] * t_c + X[2] * q_c, X[1] * t_x + X[2] * q_x, X[1] * t_y + X[2] * q_y, X[2] * q_h1, X[2] * q_h2]
        out.append([[round(float(v), 3) for v in tt] for tt in terms])
    return out


def sphere_stops(R):
    """Depth stops dense near the limb (where n.L changes fastest), sparse inside."""
    fr = [0, 0.004, 0.01, 0.02, 0.035, 0.055, 0.08, 0.11, 0.15, 0.2, 0.27, 0.35, 0.45, 0.57, 0.7, 0.85, 1.0]
    return [round(R * f, 2) for f in fr]


# ------------------------------------------------------------------------------------------------------------ dimples
def dimple_list(c, rs, phase, rd_k, a, turn, th_max=1.45):
    out = []
    t1, t2 = math.radians(turn), math.radians(turn - 60.0)
    e1 = np.array([math.cos(t1 + math.pi / 2), math.sin(t1 + math.pi / 2)]) * a
    e2 = np.array([math.cos(t2 + math.pi / 2), math.sin(t2 + math.pi / 2)]) * a
    n = int(rs * math.pi / 2 / a * 1.3) + 2
    rd = rd_k * a
    for i in range(-n, n + 1):
        for j in range(-n, n + 1):
            q = (i + phase[0]) * e1 + (j + phase[1]) * e2
            rho = float(np.hypot(*q))
            th = rho / rs
            if th > th_max:
                continue
            u = q / max(rho, 1e-9)
            r = rs * math.sin(th)
            out.append(((c[0] + u[0] * r, c[1] + u[1] * r), rd * math.cos(th), rd, math.degrees(math.atan2(u[1], u[0])), th))
    return out


def extent_along(ra, ta, rot, lx, ly):
    r = math.radians(rot)
    ux, uy = math.cos(r), math.sin(r)
    a1 = lx * ux + ly * uy
    a2 = -lx * uy + ly * ux
    return math.sqrt((ra * a1) ** 2 + (ta * a2) ** 2)


def dimple_offset(x, y, cups, g_top, g_bot, soft, light_deg, fall):
    off = np.zeros(x.shape)
    ld = math.radians(light_deg)
    lx, ly = math.cos(ld), math.sin(ld)
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
        t = ((xx - c[0]) * lx + (yy - c[1]) * ly) / max(h, 1e-6)
        k = math.cos(th) ** fall
        val = np.where(t > 0, -g_top * t, -g_bot * t) * k
        off[j0:j1, i0:i1] = off[j0:j1, i0:i1] * (1 - al) + val * al
    return off


def fit_dimples(hp, mask, c, rs, a_range=(28.0, 40.0), look=None, log=print):
    """Lattice and cup look of a ball's dimples fitted to the residual hp (art - smooth ball) on mask."""
    x, y = grid()
    tgt = hp[mask]
    look = look or {"rd_k": 0.42, "g_top": 10.0, "g_bot": 8.0, "soft": 4.0, "light": -90.0, "fall": 1.0}

    def err(p):
        cups = dimple_list(c, rs, (p["p0"], p["p1"]), p["rd_k"], p["a"], p["turn"])
        o = dimple_offset(x, y, cups, p["g_top"], p["g_bot"], p["soft"], p["light"], p["fall"])[mask]
        return float(((tgt - o) ** 2).mean())

    best = None
    for a in np.linspace(a_range[0], a_range[1], 5):
        for turn in range(90, 150, 10):
            for p0 in np.arange(0, 1, 0.125):
                for p1 in np.arange(0, 1, 0.125):
                    p = dict(look, a=float(a), turn=float(turn), p0=float(p0), p1=float(p1))
                    e = err(p)
                    if best is None or e < best[0]:
                        best = (e, p)
    e0, p = best
    log(f"  dimples coarse: {p} mse {e0:.2f} (var {tgt.var():.2f})")
    steps = {"p0": 0.04, "p1": 0.04, "a": 0.8, "turn": 2.0, "rd_k": 0.03, "g_top": 1.5, "g_bot": 1.5, "soft": 1.0,
             "light": 10.0, "fall": 0.3}
    for rnd in range(5):
        for k in steps:
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
    log(f"  dimples: { {k: round(float(v), 3) for k, v in p.items()} } mse {e0:.2f} (var {tgt.var():.2f})")
    out = {k: float(v) for k, v in p.items()}
    out.update(mse=e0, var=float(tgt.var()))
    return out


def fold_variance(hp, mask, c, rs, a, turn, G=24):
    """How strongly the residual repeats on the hex lattice (a, turn) on the sphere: the variance of the mean residual per cell
    position after folding every pixel into one lattice cell (unwarped azimuthal-equidistant, as dimple_list lays the cups)."""
    ys, xs = np.nonzero(mask)
    X, Y = xs + 0.5 - c[0], ys + 0.5 - c[1]
    r = np.hypot(X, Y)
    rho = np.arcsin(np.clip(r / rs, 0, 1)) * rs
    k = np.where(r > 1e-6, rho / np.maximum(r, 1e-6), 1.0)
    qx, qy = X * k, Y * k
    t1, t2 = math.radians(turn), math.radians(turn - 60.0)
    B = np.array([[math.cos(t1 + math.pi / 2), math.cos(t2 + math.pi / 2)], [math.sin(t1 + math.pi / 2), math.sin(t2 + math.pi / 2)]]) * a
    Bi = np.linalg.inv(B)
    f1 = (Bi[0, 0] * qx + Bi[0, 1] * qy) % 1.0
    f2 = (Bi[1, 0] * qx + Bi[1, 1] * qy) % 1.0
    i = (f1 * G).astype(int)
    j = (f2 * G).astype(int)
    s = np.zeros((G, G))
    n = np.zeros((G, G))
    v = hp[ys, xs]
    np.add.at(s, (j, i), v)
    np.add.at(n, (j, i), 1)
    return float((s / np.maximum(n, 1)).var())


def fit_dimples_fast(hp, mask, c, rs, a_range=(28.0, 46.0), look=None, sub=3, log=print):
    """fit_dimples for a big ball: spacing and turn by fold variance, then phase and the cup look by error on every sub-th
    pixel of the mask."""
    best = None
    for a in np.arange(a_range[0], a_range[1] + 0.01, 0.5):
        for turn in np.arange(60.0, 120.0, 1.0):
            v = fold_variance(hp, mask, c, rs, a, turn)
            if best is None or v > best[0]:
                best = (v, float(a), float(turn))
    _, a, turn = best
    log(f"  dimples lattice: a {a} turn {turn} fold var {best[0]:.3f}")
    x, y = grid()
    m2 = mask.copy()
    m2[::sub, :] &= True
    keep = np.zeros_like(mask)
    keep[::sub, ::sub] = True
    m2 = mask & keep
    tgt = hp[m2]
    look = dict(look or {"rd_k": 0.42, "g_top": 8.0, "g_bot": 6.0, "soft": 4.0, "light": -90.0, "fall": 1.0})

    def err(p):
        cups = dimple_list(c, rs, (p["p0"], p["p1"]), p["rd_k"], p["a"], p["turn"])
        o = dimple_offset(x, y, cups, p["g_top"], p["g_bot"], p["soft"], p["light"], p["fall"])[m2]
        return float(((tgt - o) ** 2).mean())

    bp = None
    for p0 in np.arange(0, 1, 0.125):
        for p1 in np.arange(0, 1, 0.125):
            p = dict(look, a=a, turn=turn, p0=float(p0), p1=float(p1))
            e = err(p)
            if bp is None or e < bp[0]:
                bp = (e, p)
    e0, p = bp
    steps = {"p0": 0.04, "p1": 0.04, "rd_k": 0.04, "g_top": 2.0, "g_bot": 2.0, "soft": 1.0, "light": 15.0, "fall": 0.3, "a": 0.4, "turn": 1.0}
    for rnd in range(3):
        for k in steps:
            for sgn in (-1, 1):
                for _ in range(6):
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
    log(f"  dimples: { {k: round(float(v), 3) for k, v in p.items()} } mse {e0:.2f} (var {tgt.var():.2f})")
    out = {k: float(v) for k, v in p.items()}
    out.update(mse=e0, var=float(tgt.var()))
    return out


def dimple_entries(c, rs, dp, paint, ball_shape, clip_r, name="ball dimple", anchor="centre", min_ra=0.8):
    """One entry per cup: a linear ramp (shaded wall toward the light, lit wall away) over the body's own colour at that
    spot (paint evaluated at the cup centre), soft edge, clipped to the circle clip_r."""
    cups = dimple_list(c, rs, (dp["p0"], dp["p1"]), dp["rd_k"], dp["a"], dp["turn"])
    ld = math.radians(dp["light"])
    lx, ly = math.cos(ld), math.sin(ld)
    clip = {"t": "circle", "c": list(c), "r": clip_r}
    ents = []
    for cc, ra, ta, rot, th in cups:
        if ra < min_ra or math.hypot(cc[0] - c[0], cc[1] - c[1]) > clip_r + ta:
            continue
        body = np.asarray(sdf.paint(paint, np.array([cc[0]]), np.array([cc[1]]), None, ball_shape))[0]
        Lb = float(fit.lum(body))
        k = math.cos(th) ** dp["fall"]
        dark = np.clip(body * (Lb - dp["g_top"] * k) / Lb, 0, 255)
        light = np.clip(body * (Lb + dp["g_bot"] * k) / Lb, 0, 255)
        h = extent_along(ra, ta, rot, lx, ly)
        L = {"shape": {"t": "ellipse", "c": [round(cc[0], 2), round(cc[1], 2)], "r": [round(ra, 2), round(ta, 2)], "rot": round(rot, 2)},
             "soft": round(dp["soft"], 2), "clip": clip,
             "fill": {"linear": {"a": [round(cc[0] + lx * h, 2), round(cc[1] + ly * h, 2)], "b": [round(cc[0] - lx * h, 2), round(cc[1] - ly * h, 2)],
                                 "stops": [[0.0, [round(v, 1) for v in dark]], [0.5, [round(v, 1) for v in body]], [1.0, [round(v, 1) for v in light]]]}}}
        m = ta + dp["soft"] + 1
        ents.append({"name": f"{name} {len(ents)}", "anchor": anchor, "box": [cc[0] - m, cc[1] - m, cc[0] + m, cc[1] + m], "layers": [L]})
    return ents


# ------------------------------------------------------------------------------------------------------------ paths
# A navy line made of straight runs and circle arcs joined by true fillets (round corners of radius rf, tangent to both
# sides): a union of "seg" and "arc" shapes, all analytic (no long polylines), so the joins are exact and the port draws it
# cheaply.
def _ang(v):
    return math.degrees(math.atan2(v[1], v[0])) % 360.0


def arc_short(c, r, a1, a2, w):
    """The shorter arc of circle (c, r) between angles a1 and a2 (degrees, screen: 90 = down)."""
    s = (a2 - a1) % 360.0
    if s <= 180.0:
        return {"t": "arc", "c": [round(c[0], 3), round(c[1], 3)], "r": round(r, 3), "a0": round(a1 % 360.0, 3), "sweep": round(s, 3), "w": w}
    return {"t": "arc", "c": [round(c[0], 3), round(c[1], 3)], "r": round(r, 3), "a0": round(a2 % 360.0, 3), "sweep": round(360.0 - s, 3), "w": w}


def fillet_hline_circle(Y0, s, C, R, rf):
    """Fillet between the horizontal line y = Y0 and the circle (C, R) seen from inside the circle, the fillet's centre on
    side s of the line (+1 below). Returns (tangent on the line, fillet centre, tangent on the circle, angle on the big circle)."""
    fy = Y0 + s * rf
    fx = C[0] - math.sqrt((R - rf) ** 2 - (fy - C[1]) ** 2)
    v = (fx - C[0], fy - C[1])
    n = math.hypot(*v)
    tc = (C[0] + R * v[0] / n, C[1] + R * v[1] / n)
    return (fx, Y0), (fx, fy), tc, _ang(v)


def fillet_hline_line(Y0, m, b, rf):
    """Fillet where a run heading +x along y = Y0 turns clockwise onto y = m x + b (m > 0, heading down-right)."""
    k = math.sqrt(m * m + 1)
    yc = Y0 + rf
    xc = (yc - b - rf * k) / m
    # foot of the perpendicular from the centre onto m x - y + b = 0
    d = (m * xc - yc + b) / (k * k)
    t2 = (xc - m * d, yc + d)
    return (xc, Y0), (xc, yc), t2


def path_hline_circle_hline(x_start, Y_top, C, R, Y_bot, x_end, w, rf, tail=None):
    """Top run from x_start (under the element it leaves) to a fillet, down the circle's left side, a fillet, a bottom run to
    x_end; tail = (m, b, x_last, rf2) turns the bottom run clockwise onto y = m x + b down to x_last."""
    t1, f1, c1, a_top = fillet_hline_circle(Y_top, +1, C, R, rf)
    t2, f2, c2, a_bot = fillet_hline_circle(Y_bot, -1, C, R, rf)
    parts = [{"t": "seg", "a": [x_start, Y_top], "b": [round(t1[0], 3), Y_top], "w": w},
             arc_short(f1, rf, 270.0, _ang((c1[0] - f1[0], c1[1] - f1[1])), w),
             arc_short(C, R, a_top, a_bot, w),
             arc_short(f2, rf, 90.0, _ang((c2[0] - f2[0], c2[1] - f2[1])), w)]
    if tail is None:
        parts.append({"t": "seg", "a": [round(t2[0], 3), Y_bot], "b": [x_end, Y_bot], "w": w})
    else:
        m, b, x_last, rf2 = tail
        u1, g, u2 = fillet_hline_line(Y_bot, m, b, rf2)
        parts.append({"t": "seg", "a": [round(t2[0], 3), Y_bot], "b": [round(u1[0], 3), Y_bot], "w": w})
        parts.append(arc_short(g, rf2, 270.0, _ang((u2[0] - g[0], u2[1] - g[1])), w))
        parts.append({"t": "seg", "a": [round(u2[0], 3), round(u2[1], 3)], "b": [x_last, round(m * x_last + b, 3)], "w": w})
    return {"t": "union", "of": parts}


# ------------------------------------------------------------------------------------------------------------ shadows
def shadow_layer(shape, p, rgb=None):
    return {"shape": {"t": "grow", "of": {"t": "move", "of": shape, "d": [round(p["dx"], 2), round(p["dy"], 2)]}, "d": round(p["g"], 2)},
            "soft": round(max(p["soft"], 0.3), 2), "fill": rgb or SHADOW_RGB, "alpha": round(p["alpha"], 3)}


def fit_shadow(shapes, band, U, a, p0, over=None, log=print, name="", rgb=None):
    """Shared soft-shadow parameters (move dx, dy; grow g; soft; alpha) of a family of casters, fitted on the band pixels.
    rgb: the layer's colour (default SHADOW_RGB; a light colour fits a lit lip instead)."""
    col = np.asarray(rgb or SHADOW_RGB, float)
    ys, xs = np.nonzero(band)
    X, Y = xs + 0.5, ys + 0.5
    Ub, Ab = U[band], a[band]
    OA = over[0][band] if over else 0.0
    OC = over[1][band] if over else 0.0

    def render(p):
        acc = Ub.copy()
        for S in shapes:
            L = shadow_layer(S, p)
            d = sdf.sd(L["shape"], X, Y)
            u = np.clip(0.5 - d / (2 * L["soft"]), 0, 1)
            k = (u * u * (3 - 2 * u) * L["alpha"])[:, None]
            acc = acc * (1 - k) + col * k
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
    log(f"  shadow {name}: {{{', '.join(f'{k}: {v:.2f}' for k, v in p.items())}}} rms {math.sqrt(e0):.2f} (none {math.sqrt(err(dict(p, alpha=0.0))):.2f})")
    return p


# ------------------------------------------------------------------------------------------------------------ misc
def load_refs(ids, state="idle"):
    return {r: wide.load_ref(r, state) for r in ids}


def zoom_png(img, x, y, w, h, s, path):
    Image.fromarray(np.clip(img[y:y + h, x:x + w], 0, 255).astype(np.uint8)).resize((w * s, h * s), Image.NEAREST).save(path)


def write_results(screen, rows):
    out, _ = dirs(screen)
    with open(os.path.join(out, "results.tsv"), "w", encoding="utf-8") as f:
        f.write("id\tstatus\twhat\tunsure\n")
        for r in rows:
            f.write("\t".join(r) + "\n")
