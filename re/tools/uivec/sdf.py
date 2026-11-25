"""uivec: the UI redrawn from authored shapes (P41, log/roadmap/P41/P41_audit.md).

A SCENE is a list of ITEMS; an item is {"box": [x0, y0, x1, y1], "layers": [...]} in 800 x 600 frame coordinates. A LAYER is
{"shape": S, "fill": P, "alpha": a, "soft": r, "clip": S, "falloff": {...}, "fx": [...]}, painted in order.

Shapes (plain dicts, so a scene is JSON and maps one to one onto the port's UiScene later):
  {"t": "circle", "c": [x, y], "r": r}
  {"t": "ellipse", "c": [x, y], "r": [rx, ry], "rot": degrees}
  {"t": "poly", "rings": [[[x, y], ...], ...]}          even-odd over all rings (holes)
  {"t": "seg", "a": [x, y], "b": [x, y], "w": width}
  {"t": "half", "p": [x, y], "n": [nx, ny]}             the half plane behind p along -n
  {"t": "union" | "inter", "of": [S, ...]}, {"t": "sub", "a": S, "b": S}, {"t": "grow", "of": S, "d": d}
  {"t": "stroke", "pts": [[x, y], ...], "w": width, "cap": "butt" | "flat" | "round", "closed": bool}    a line or a polyline
      (square ends: "butt" cuts the whole stroke at each end's plane, "flat" only the part near that end; use "flat" for curves)
  {"t": "arrow", "a": [x, y], "b": [x, y], "w": shaft width, "hl": head length, "hw": head width}   tip at b
  {"t": "rrect", "c": [x, y], "half": [hx, hy], "r": corner radius, "rot": degrees}
  {"t": "outline", "of": S, "w": width}                  the line along S's edge (a stroked shape)
  {"t": "qstroke", "p": [P0, P1, P2], "w": [w0, w1]}     a curved line (quadratic) whose width changes along it
  {"t": "move", "of": S, "d": [dx, dy]}
  {"t": "arc", "c": [x, y], "r": radius, "a0": degrees, "sweep": degrees, "w": width}     part of a circle's line, from
      angle a0 (0 = right, 90 = down on the screen) over `sweep` degrees, round ends
  {"t": "lathe", "x": axis, "prof": [[y, half width], ...], "smooth": n}     a turned object (a cup, a vase): its
      silhouette is one profile mirrored about a vertical axis
  {"t": "dimpled", "c": [x, y], "r": R, "spacing": s, "turn": degrees, "phase": [u, v], "light": [lx, ly], "lz": lz,
   "l0": l0, "k": k, "rim": w}                                               a dimpled ball lit from one side: a dark
      disc with a hexagonal lattice of dimples cut out of it; a dimple's size grows with how much its spot on the sphere
      faces the light (k x (facing - l0), in lattice spacings), so the shadow side stays solid, the lit side breaks into a
      thin web and then into nothing but the ball's outline (rim)
  a "poly" may carry "smooth": n (n rounds of corner cutting: a polygon drawn as a rounded blob)
Paints: [r, g, b] | {"radial": {"c": [x, y], "r": R, "stops": [[t, [r, g, b]], ...]}} | {"linear": {"a": .., "b": .., "stops": ..}}
  | {"bevel": {"L": scale, "stops": [u0, ...], "coef": [[[r, g, b] x 5] x stops], "of": S (default: the layer's shape)}}
    a lit surface: the colour depends on how deep the point is inside the shape (u = depth / L, piecewise linear between
    the stops) and on which way the nearest edge faces (n = the outward normal): colour = sum over stops of
    w_k(u) * (c_k0 + c_k1 nx + c_k2 ny + c_k3 (nx^2 - ny^2) + c_k4 2 nx ny). A sphere, a raised rim and a sunken well are
    all this paint with different numbers (accept.py measures them on the art).
Effects (change the layer's colour before it is composited):
  {"t": "edge_light", "w": w, "pow": p, "dir": [lx, ly], "light": [r, g, b], "dark": [r, g, b]}   a bevel lit from dir
  {"t": "inner_shadow", "w": w, "pow": p, "dir": [lx, ly], "rgb": [r, g, b], "k": k}             the wall's shadow inside
  {"t": "tone", "a": [x, y], "b": [x, y], "add": v}                                              +v at a to -v at b
No art is read here: a scene is numbers.
"""
import numpy as np


def _rot(x, y, c, deg):
    a = np.deg2rad(deg)
    px, py = x - c[0], y - c[1]
    return px * np.cos(a) + py * np.sin(a), -px * np.sin(a) + py * np.cos(a)


EXTRA = {}      # more shape types, registered by other modules: EXTRA["myarea.name"] = function(S, x, y) -> distances


def sd(S, x, y):
    t = S["t"]
    if t in EXTRA:
        return EXTRA[t](S, x, y)
    if t == "circle":
        return np.hypot(x - S["c"][0], y - S["c"][1]) - S["r"]
    if t == "ellipse":
        px, py = _rot(x, y, S["c"], S.get("rot", 0.0))
        rx, ry = S["r"]
        k0 = np.sqrt((px / rx) ** 2 + (py / ry) ** 2)
        k1 = np.sqrt((px / (rx * rx)) ** 2 + (py / (ry * ry)) ** 2)
        return np.where(k0 < 0.05, -min(rx, ry), k0 * (k0 - 1.0) / np.maximum(k1, 1e-9))
    if t == "stroke":
        pts = S["pts"]
        for _ in range(int(S.get("smooth", 0))):      # corner cutting that keeps the two ends: a polyline drawn as a curve
            pts = [pts[0]] + [[a[0] * (1 - f) + b[0] * f, a[1] * (1 - f) + b[1] * f]
                              for a, b in zip(pts, pts[1:]) for f in (0.25, 0.75)] + [pts[-1]]
        segs = list(zip(pts, pts[1:])) + ([(pts[-1], pts[0])] if S.get("closed") else [])
        # A square end is a plane across that end. cap "butt" cuts the WHOLE stroke with it, so a curve that bends back past
        # the plane of its own end loses that part: 47 shapes of approved drawings depend on it, so it stays as it is.
        # cap "flat" is the same square end cutting only the segments within two widths of that end: use it for new curves.
        cap = S.get("cap", "butt")
        planes = []
        if cap in ("butt", "flat") and not S.get("closed"):
            ln = [float(np.hypot(b[0] - a[0], b[1] - a[1])) for a, b in segs]
            for p, q, order in ((pts[0], pts[1], range(len(segs))), (pts[-1], pts[-2], range(len(segs) - 1, -1, -1))):
                ux, uy = p[0] - q[0], p[1] - q[1]
                n = max(np.hypot(ux, uy), 1e-9)
                near, run = set(), 0.0
                for i in order:
                    near.add(i)
                    run += ln[i]
                    if cap == "flat" and run >= 2.0 * S["w"]:
                        break
                planes.append((near, (x - p[0]) * ux / n + (y - p[1]) * uy / n))
        d = np.full(x.shape, 1e18)
        for i, (a, b) in enumerate(segs):
            ex, ey = b[0] - a[0], b[1] - a[1]
            wx, wy = x - a[0], y - a[1]
            u = np.clip((wx * ex + wy * ey) / max(ex * ex + ey * ey, 1e-12), 0, 1)
            di = np.hypot(wx - ex * u, wy - ey * u) - S["w"] / 2
            for near, pl in planes:
                if i in near:
                    di = np.maximum(di, pl)
            d = np.minimum(d, di)
        return d
    if t == "arrow":
        a, b = S["a"], S["b"]
        ux, uy = b[0] - a[0], b[1] - a[1]
        n = max(np.hypot(ux, uy), 1e-9)
        ux, uy = ux / n, uy / n
        px, py = -uy, ux
        bx, by = b[0] - S["hl"] * ux, b[1] - S["hl"] * uy
        w, hw = S["w"] / 2, S["hw"] / 2
        # one outline (shaft and head as two shapes left a seam in the distance where they met, visible under a bevel or a soft edge)
        ring = [[a[0] + px * w, a[1] + py * w], [bx + px * w, by + py * w], [bx + px * hw, by + py * hw], [b[0], b[1]],
                [bx - px * hw, by - py * hw], [bx - px * w, by - py * w], [a[0] - px * w, a[1] - py * w]]
        return sd({"t": "poly", "rings": [ring]}, x, y)
    if t == "rrect":
        px, py = _rot(x, y, S["c"], S.get("rot", 0.0))
        r = min(S.get("r", 0.0), S["half"][0], S["half"][1])
        qx, qy = np.abs(px) - (S["half"][0] - r), np.abs(py) - (S["half"][1] - r)
        return np.hypot(np.maximum(qx, 0), np.maximum(qy, 0)) + np.minimum(np.maximum(qx, qy), 0) - r
    if t == "outline":
        return np.abs(sd(S["of"], x, y)) - S["w"] / 2
    if t == "qstroke":
        (ax, ay), (bx, by), (cx, cy) = S["p"]
        left, right = [], []
        for k in range(17):
            u = k / 16.0
            qx = (1 - u) ** 2 * ax + 2 * u * (1 - u) * bx + u * u * cx
            qy = (1 - u) ** 2 * ay + 2 * u * (1 - u) * by + u * u * cy
            tx = 2 * (1 - u) * (bx - ax) + 2 * u * (cx - bx)
            ty = 2 * (1 - u) * (by - ay) + 2 * u * (cy - by)
            n = max(np.hypot(tx, ty), 1e-9)
            h = (S["w"][0] * (1 - u) + S["w"][1] * u) / 2
            left.append([qx - ty / n * h, qy + tx / n * h])
            right.append([qx + ty / n * h, qy - tx / n * h])
        return sd({"t": "poly", "rings": [left + right[::-1]]}, x, y)
    if t == "move":
        return sd(S["of"], x - S["d"][0], y - S["d"][1])
    if t == "arc":
        dx, dy = x - S["c"][0], y - S["c"][1]
        rel = (np.degrees(np.arctan2(dy, dx)) - S["a0"]) % 360.0
        on = np.abs(np.hypot(dx, dy) - S["r"])
        ends = []
        for a in (S["a0"], S["a0"] + S["sweep"]):
            ex, ey = S["c"][0] + S["r"] * np.cos(np.deg2rad(a)), S["c"][1] + S["r"] * np.sin(np.deg2rad(a))
            ends.append(np.hypot(x - ex, y - ey))
        return np.where(rel <= S["sweep"], on, np.minimum(ends[0], ends[1])) - S["w"] / 2
    if t == "lathe":
        prof = S["prof"]
        ring = [[S["x"] - h, yy] for yy, h in prof] + [[S["x"] + h, yy] for yy, h in prof[::-1]]
        return sd({"t": "poly", "rings": [ring], "smooth": S.get("smooth", 0)}, x, y)
    if t == "dimpled":
        cx, cy = S["c"]
        R, sp = S["r"], S["spacing"]
        a = np.deg2rad(S["turn"])
        L = np.array([S["light"][0], S["light"][1], S["lz"]], float)
        L = L / max(np.linalg.norm(L), 1e-9)
        ball = np.hypot(x - cx, y - cy) - R
        holes = np.full(x.shape, 1e9)
        n = int(R / max(sp, 1.0)) + 2
        for j in range(-n, n + 1):
            for i in range(-n, n + 1):
                u = (i + 0.5 * (j % 2)) * sp + S["phase"][0]
                v = j * sp * 0.866 + S["phase"][1]
                px, py = u * np.cos(a) - v * np.sin(a), u * np.sin(a) + v * np.cos(a)
                rho = np.hypot(px, py) / R
                if rho >= 0.98:
                    continue
                nz = np.sqrt(1 - rho * rho)
                lit = px / R * L[0] + py / R * L[1] + nz * L[2]
                rd = min(max(S["k"] * (lit - S["l0"]), 0.0), S.get("cap", 0.8)) * sp
                if rd < 0.3:
                    continue
                if S.get("cell") == "square":
                    # the art's dimples: rounded squares in the lattice's own directions, a little smaller at the limb;
                    # "cap" under half a spacing keeps a dark line between two neighbours however lit they are
                    h = rd * max(nz, 0.5) ** 0.5
                    holes = np.minimum(holes, sd({"t": "rrect", "c": [cx + px, cy + py], "half": [h, h], "r": S.get("round", 0.45) * h,
                                                  "rot": S["turn"]}, x, y))
                    continue
                # a round dimple seen on a sphere: squashed along the direction away from the centre
                holes = np.minimum(holes, sd({"t": "ellipse", "c": [cx + px, cy + py], "r": [rd * max(nz, 0.35), rd],
                                              "rot": float(np.degrees(np.arctan2(py, px)))}, x, y))
        d = np.maximum(ball, -holes)
        rim = np.maximum(ball, -(ball + S["rim"]))
        if "rim_l" in S:
            # the ball's outline is drawn only on its shadow side: toward the light it dissolves (the art shows no line there)
            n2 = max(np.hypot(L[0], L[1]), 1e-9)
            facing = ((x - cx) * L[0] + (y - cy) * L[1]) / n2 / R
            rim = np.maximum(rim, (facing - S["rim_l"]) * R)
        return np.minimum(d, rim)
    if t == "poly":
        d = np.full(x.shape, 1e18)
        inside = np.zeros(x.shape, bool)
        rings = S["rings"]
        for _ in range(int(S.get("smooth", 0))):
            rings = [[[a[0] * (1 - f) + b[0] * f, a[1] * (1 - f) + b[1] * f]
                      for a, b in zip(r, r[1:] + r[:1]) for f in (0.25, 0.75)] for r in rings]
        for ring in rings:
            n = len(ring)
            for i in range(n):
                ax, ay = ring[i]
                bx, by = ring[(i + 1) % n]
                ex, ey = bx - ax, by - ay
                L = ex * ex + ey * ey
                if L < 1e-12:
                    continue
                wx, wy = x - ax, y - ay
                u = np.clip((wx * ex + wy * ey) / L, 0, 1)
                d = np.minimum(d, (wx - ex * u) ** 2 + (wy - ey * u) ** 2)
                if ey != 0:
                    c = ((ay <= y) & (by > y)) | ((by <= y) & (ay > y))
                    inside ^= c & (x < ax + (y - ay) * ex / ey)
        return np.where(inside, -1.0, 1.0) * np.sqrt(d)
    if t == "seg":
        ax, ay = S["a"]
        ex, ey = S["b"][0] - ax, S["b"][1] - ay
        wx, wy = x - ax, y - ay
        u = np.clip((wx * ex + wy * ey) / (ex * ex + ey * ey), 0, 1)
        return np.hypot(wx - ex * u, wy - ey * u) - S["w"] / 2
    if t == "half":
        return (x - S["p"][0]) * S["n"][0] + (y - S["p"][1]) * S["n"][1]
    if t == "union":
        d = sd(S["of"][0], x, y)
        for q in S["of"][1:]:
            d = np.minimum(d, sd(q, x, y))
        return d
    if t == "inter":
        d = sd(S["of"][0], x, y)
        for q in S["of"][1:]:
            d = np.maximum(d, sd(q, x, y))
        return d
    if t == "sub":
        return np.maximum(sd(S["a"], x, y), -sd(S["b"], x, y))
    if t == "grow":
        return sd(S["of"], x, y) - S["d"]
    raise ValueError(t)


def _stops(u, stops):
    u = np.clip(u, stops[0][0], stops[-1][0])
    out = np.zeros(u.shape + (3,))
    for (t0, c0), (t1, c1) in zip(stops, stops[1:]):
        m = (u >= t0) & (u <= t1)
        k = ((u - t0) / max(t1 - t0, 1e-9))[..., None]
        out = np.where(m[..., None], np.asarray(c0, float) * (1 - k) + np.asarray(c1, float) * k, out)
    return out


def normal(S, x, y, d):
    """The outward unit normal of the shape's nearest edge at each point."""
    if S["t"] == "circle":
        px, py = x - S["c"][0], y - S["c"][1]
        n = np.maximum(np.hypot(px, py), 1e-9)
        return px / n, py / n
    if S["t"] == "ellipse":
        px, py = _rot(x, y, S["c"], S.get("rot", 0.0))
        gx, gy = px / S["r"][0] ** 2, py / S["r"][1] ** 2
        n = np.maximum(np.hypot(gx, gy), 1e-12)
        gx, gy = gx / n, gy / n
        a = np.deg2rad(S.get("rot", 0.0))
        return gx * np.cos(a) - gy * np.sin(a), gx * np.sin(a) + gy * np.cos(a)
    return _normal(d)


def bevel_terms(u, nx, ny, stops):
    """The K x 5 weights of the bevel paint (hat functions over the stops x the five direction terms)."""
    u = np.clip(u, stops[0], stops[-1])
    H = [np.ones_like(u), nx, ny, nx * nx - ny * ny, 2 * nx * ny]
    out = []
    for k, t in enumerate(stops):
        w = np.zeros_like(u)
        if k > 0:
            a = stops[k - 1]
            m = (u >= a) & (u <= t)
            w = np.where(m, (u - a) / max(t - a, 1e-9), w)
        if k < len(stops) - 1:
            b = stops[k + 1]
            m = (u > t) & (u <= b)
            w = np.where(m, (b - u) / max(b - t, 1e-9), w)
        if k == 0:
            w = np.where(u <= t, 1.0, w)
        if k == len(stops) - 1:
            w = np.where(u >= t, 1.0, w)
        out.append([w * h for h in H])
    return out


def paint(P, x, y, d=None, S=None):
    if isinstance(P, dict) and "bevel" in P:
        b = P["bevel"]
        S2 = b.get("of") or S
        dd = sd(S2, x, y) if ("of" in b or d is None) else d
        nx, ny = normal(S2, x, y, dd)
        T = bevel_terms(-dd / b["L"], nx, ny, b["stops"])
        out = np.zeros(x.shape + (3,))
        for k, row in enumerate(T):
            for h, w in enumerate(row):
                out += w[..., None] * np.asarray(b["coef"][k][h], float)
        return out
    if isinstance(P, dict) and "radial" in P:
        r = P["radial"]
        return _stops(np.hypot(x - r["c"][0], y - r["c"][1]) / r["r"], r["stops"])
    if isinstance(P, dict) and "linear" in P:
        l = P["linear"]
        ex, ey = l["b"][0] - l["a"][0], l["b"][1] - l["a"][1]
        return _stops(((x - l["a"][0]) * ex + (y - l["a"][1]) * ey) / (ex * ex + ey * ey), l["stops"])
    out = np.empty(x.shape + (3,))
    out[:] = P
    return out


def _normal(d):
    gy, gx = np.gradient(d)
    n = np.maximum(np.hypot(gx, gy), 1e-9)
    return gx / n, gy / n


def render_layer(img, L, x, y, s, d=None):
    if d is None:
        d = sd(L["shape"], x, y)
    col = paint(L["fill"], x, y, d, L["shape"])
    for f in L.get("fx", ()):
        if f["t"] == "edge_light":
            nx, ny = _normal(d)
            lit = nx * f["dir"][0] + ny * f["dir"][1]
            e = np.clip(1 + d / f["w"], 0, 1) ** f.get("pow", 1.0)
            col = col + (e * np.clip(lit, 0, 1))[..., None] * np.asarray(f["light"], float)
            col = col - (e * np.clip(-lit, 0, 1))[..., None] * np.asarray(f["dark"], float)
        elif f["t"] == "inner_shadow":
            nx, ny = _normal(d)
            wall = np.clip(nx * f["dir"][0] + ny * f["dir"][1], 0, 1)
            e = np.clip(1 + d / f["w"], 0, 1) ** f.get("pow", 2.0)
            k = (f.get("k", 0.8) * wall * e)[..., None]
            col = col * (1 - k) + np.asarray(f["rgb"], float) * k
        elif f["t"] == "tone":
            ex, ey = f["b"][0] - f["a"][0], f["b"][1] - f["a"][1]
            u = np.clip(((x - f["a"][0]) * ex + (y - f["a"][1]) * ey) / (ex * ex + ey * ey), 0, 1)
            col = col + ((1 - 2 * u) * f["add"])[..., None]
    if "soft" in L:
        u = np.clip(0.5 - d / (2 * L["soft"]), 0, 1)
        a = u * u * (3 - 2 * u)
    else:
        a = np.clip(0.5 - d * s, 0, 1)
    if "falloff" in L:
        f = L["falloff"]
        a = a * np.clip(1 - np.hypot(x - f["c"][0], y - f["c"][1]) / f["r"], 0, 1) ** f.get("pow", 2.0)
    if "clip" in L:
        a = a * np.clip(0.5 - sd(L["clip"], x, y) * s, 0, 1)
    a = (a * L.get("alpha", 1.0))[..., None]
    img[:] = img * (1 - a) + np.clip(col, 0, 255) * a
    return d


def render(scene, img, s, origin=(0.0, 0.0), cache=None):
    """Paint `scene` over img (float RGB, H x W x 3) whose pixel (i, j) centre is the frame point origin + (i + 0.5, j + 0.5) / s.
    cache (a dict): the distance fields of layers that carry a "key" are kept in it per (item, layer) and reused, for callers
    that render the same shapes many times on the same grid with only the style changing (accept.py's fit)."""
    H, W = img.shape[:2]
    for item in scene:
        x0, y0, x1, y1 = item["box"]
        i0 = max(0, int(np.floor((x0 - origin[0]) * s)))
        i1 = min(W, int(np.ceil((x1 - origin[0]) * s)))
        j0 = max(0, int(np.floor((y0 - origin[1]) * s)))
        j1 = min(H, int(np.ceil((y1 - origin[1]) * s)))
        if i1 <= i0 or j1 <= j0:
            continue
        ys, xs = np.mgrid[j0:j1, i0:i1]
        x = (xs + 0.5) / s + origin[0]
        y = (ys + 0.5) / s + origin[1]
        sub = img[j0:j1, i0:i1]
        for n, L in enumerate(item["layers"]):
            k = (scene.index(item), n, L.get("key"), i0, j0, i1, j1, s) if cache is not None and "key" in L else None
            d = render_layer(sub, L, x, y, s, cache.get(k) if k else None)
            if k:
                cache[k] = d
    return img
