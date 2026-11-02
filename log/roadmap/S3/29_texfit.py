"""S3.29 (U-1309): textured interpolation models against the real GL's frame of a probe scene (port/corpus/gl/probe/<scene>).

Each model gives, per covered pixel of every triangle, the texture coordinates (texel units) and the colour; the texel filter
(GL_LINEAR, repeat) and GL_MODULATE then give the 8-bit value per channel, quantised as GlRaster does (floor(v * 31 / 255 +
(k + 0.5) / 16)). Coverage follows the real frame's drawn pixels per triangle (the scenes' triangles do not overlap; a pixel is
assigned to the triangle whose edge functions contain its centre, GlRaster's tie rule). Printed: wrong pixels per model.

    cd <tree> && ~/bin/py -3.12 ../../../log/roadmap/S3/29_texfit.py <scene> [model ...]
"""
import math
import pathlib
import struct
import sys

import numpy as np

sys.path.insert(0, str(pathlib.Path.cwd()))
from port.test import glprobe as P  # noqa: E402

f32 = np.float32
K = np.array([[15, 7, 13, 5], [3, 11, 1, 9], [12, 4, 14, 6], [0, 8, 2, 10]])


def fbits(h):
    return struct.unpack("<f", struct.pack("<I", int(h, 16)))[0]


def snapped(x, y):
    m00, m03 = f32(2 / 800), f32(-1.0)
    m11, m13 = f32(2 / 600), f32(-1.0)
    cx = f32(m03 + f32(f32(x) * m00)); cy = f32(m13 + f32(f32(y) * m11))
    wx = float(f32(cx * f32(400 - 1 / 256))) + 400; wy = float(f32(cy * f32(300 - 1 / 256))) + 300
    return np.round(256 * wx) / 256, np.round(256 * wy) / 256


def unsnapped(x, y, form="d"):
    """the window position before the 2^-8 rounding: 'd' f32(ndc * f32(h - 2^-8)) + c in double, 'f' the sum in float32"""
    m00, m03 = f32(2 / 800), f32(-1.0)
    m11, m13 = f32(2 / 600), f32(-1.0)
    cx = f32(m03 + f32(f32(x) * m00)); cy = f32(m13 + f32(f32(y) * m11))
    if form == "f":
        return float(f32(f32(cx * f32(400 - 1 / 256)) + f32(400))), float(f32(f32(cy * f32(300 - 1 / 256)) + f32(300)))
    return float(f32(cx * f32(400 - 1 / 256))) + 400, float(f32(cy * f32(300 - 1 / 256))) + 300


def load(scene):
    d = P.PROBE / scene
    tris, cur, col, tc = [], [], (1.0, 1.0, 1.0, 1.0), (0.0, 0.0)
    for ln in (d / "snaps" / "s1" / "pass.gl.txt").read_text().splitlines():
        t = ln.split(" ", 1)
        if t[0] == "glColor4f":
            col = tuple(fbits(h) for h in t[1].split(","))
        elif t[0] == "glTexCoord2f":
            tc = tuple(fbits(h) for h in t[1].split(","))
        elif t[0] == "glVertex3f":
            x, y, _ = [fbits(h) for h in t[1].split(",")]
            cur.append({"p": snapped(x, y), "c": col, "t": tc, "raw": (x, y)})
        elif t[0] == "glEnd":
            tris.append(cur); cur = []
    tex = None
    tdir = d / "tex"
    files = sorted(tdir.glob("*.bin"))
    if files:
        tex = np.frombuffer(files[0].read_bytes(), np.uint8).reshape(64, 64, 3).astype(np.float64)
    real = np.frombuffer((d / "snaps" / "s1" / "dib_post.bin").read_bytes(), "<u2").reshape(600, 800)
    return tris, tex, real


def edge(ax, ay, bx, by, px, py):
    return (bx - ax) * (py - ay) - (by - ay) * (px - ax)


def coverage(t):
    """rows of the triangle: (y, xs covered) with GlRaster's tie rule (edges pointing to +y, or +x when horizontal, own centres)"""
    P0, P1, P2 = [v["p"] for v in t]
    ar = edge(*P0, *P1, *P2)
    vs = [P0, P1, P2] if ar > 0 else [P0, P2, P1]

    def inc(p, q):
        ex, ey = q[0] - p[0], q[1] - p[1]
        return ey > 0 or (ey == 0 and ex > 0)
    c0, c1, c2 = vs
    i0, i1, i2 = inc(c1, c2), inc(c2, c0), inc(c0, c1)
    xs = [v[0] for v in vs]; ys = [v[1] for v in vs]
    X = np.arange(max(0, int(math.floor(min(xs)))), min(799, int(math.ceil(max(xs)))) + 1)
    rows = []
    for y in range(max(0, int(math.floor(min(ys)))), min(599, int(math.ceil(max(ys)))) + 1):
        px, py = X + 0.5, y + 0.5
        w0 = edge(*c1, *c2, px, py); w1 = edge(*c2, *c0, px, py); w2 = edge(*c0, *c1, px, py)
        ok = (w0 >= 0) & (w1 >= 0) & (w2 >= 0)
        ok &= ~((w0 == 0) & (not i0)) & ~((w1 == 0) & (not i1)) & ~((w2 == 0) & (not i2))
        if ok.any():
            rows.append((y, X[ok]))
    return rows


def plane(t, attr, px, py):
    """exact attribute plane over the snapped vertices (double)"""
    P0, P1, P2 = [v["p"] for v in t]
    ar = edge(*P0, *P1, *P2)
    a = [attr(v) for v in t]
    w0 = edge(*P1, *P2, px, py); w1 = edge(*P2, *P0, px, py); w2 = edge(*P0, *P1, px, py)
    return (w0 * a[0] + w1 * a[1] + w2 * a[2]) / ar


def grads(t, attr, ftype=float):
    P0, P1, P2 = [v["p"] for v in t]
    a = [attr(v) for v in t]
    if ftype is float:
        dx1, dy1, dx2, dy2 = P1[0] - P0[0], P1[1] - P0[1], P2[0] - P0[0], P2[1] - P0[1]
        ar = dx1 * dy2 - dx2 * dy1
        d1, d2 = a[1] - a[0], a[2] - a[0]
        return (d1 * dy2 - d2 * dy1) / ar, (d2 * dx1 - d1 * dx2) / ar
    F = f32
    dx1, dy1, dx2, dy2 = F(P1[0] - P0[0]), F(P1[1] - P0[1]), F(P2[0] - P0[0]), F(P2[1] - P0[1])
    ar = F(F(dx1 * dy2) - F(dx2 * dy1))
    d1, d2 = F(F(a[1]) - F(a[0])), F(F(a[2]) - F(a[0]))
    return F(F(F(d1 * dy2) - F(d2 * dy1)) / ar), F(F(F(d2 * dx1) - F(d1 * dx2)) / ar)


NEAREST = False
FINAL = "d"


def final32(tex, u, v, cols, k, mode):
    """the filter and GL_MODULATE in float32: modes 'f<w>' (w: weight form) with the texel as byte / 255 ('n') or byte ('b')"""
    F = np.float32
    fu, fv = (u - 0.5).astype(F), (v - 0.5).astype(F)
    i0, j0 = np.floor(fu).astype(np.int64), np.floor(fv).astype(np.int64)
    ax, ay = (fu - i0.astype(F)).astype(F), (fv - j0.astype(F)).astype(F)
    if "q" in mode:   # weights held to 2^-B
        B = int(mode.split("q")[1].split("_")[0])
        sc = F(2.0 ** B)
        ax = (np.floor(ax * sc) / sc).astype(F); ay = (np.floor(ay * sc) / sc).astype(F)
    i1, j1 = (i0 + 1) % 64, (j0 + 1) % 64
    i0, j0 = i0 % 64, j0 % 64
    one = F(1.0)
    oma, omb = (one - ax).astype(F), (one - ay).astype(F)
    m00, m10, m01, m11 = (oma * omb).astype(F), (ax * omb).astype(F), (oma * ay).astype(F), (ax * ay).astype(F)
    out = []
    for ch in range(3):
        if "b" in mode:
            t = [tex[j, i, ch].astype(F) for (j, i) in ((j0, i0), (j0, i1), (j1, i0), (j1, i1))]
        elif "d" in mode:   # byte / 255.0f (a float division): the generic GL's form (29_notes.md section 3)
            t = [(tex[j, i, ch].astype(F) / F(255.0)).astype(F) for (j, i) in ((j0, i0), (j0, i1), (j1, i0), (j1, i1))]
        else:
            t = [(tex[j, i, ch].astype(F) * F(1.0 / 255.0)).astype(F) for (j, i) in ((j0, i0), (j0, i1), (j1, i0), (j1, i1))]
        if "lerp" in mode:   # two-stage lerp
            a = (t[0] + ax * (t[1] - t[0])).astype(F); b = (t[2] + ax * (t[3] - t[2])).astype(F)
            r = (a + ay * (b - a)).astype(F)
        else:
            r = (((m00 * t[0]).astype(F) + (m10 * t[1]).astype(F)).astype(F) + (m01 * t[2]).astype(F)).astype(F)
            r = (r + (m11 * t[3]).astype(F)).astype(F)
        c = np.asarray(np.asarray(cols[ch], float) * 31.0, F)
        if "b" in mode:
            val = (c * r).astype(F) * F(1.0 / 255.0)
        else:
            val = (c * r).astype(F)
        out.append((val.astype(F) + np.asarray(k, F)).astype(F).astype(np.float64))
    return out


def nearest(tex, u, v):
    i, j = np.floor(u).astype(np.int64) % 64, np.floor(v).astype(np.int64) % 64
    return tex[j, i].astype(np.float64)


def bilinear(tex, u, v):
    if NEAREST:
        return nearest(tex, u, v)
    fu, fv = u - 0.5, v - 0.5
    i0, j0 = np.floor(fu).astype(np.int64), np.floor(fv).astype(np.int64)
    ax, ay = fu - i0, fv - j0
    i1, j1 = (i0 + 1) % 64, (j0 + 1) % 64
    i0, j0 = i0 % 64, j0 % 64
    out = np.zeros(u.shape + (3,))
    for (jj, ii, w) in ((j0, i0, (1 - ax) * (1 - ay)), (j0, i1, ax * (1 - ay)), (j1, i0, (1 - ax) * ay), (j1, i1, ax * ay)):
        out += w[..., None] * tex[jj, ii]
    return out


# ---- models: (t, y, xs) -> (u, v, colour[3] in 0..1) arrays over xs ----
MODELS = {}
SHOW = 0


def model(f):
    MODELS[f.__name__] = f
    return f


S_ = lambda v: v["t"][0] * 64.0
T_ = lambda v: v["t"][1] * 64.0


@model
def m_plane(t, y, xs, ctx):
    px, py = xs + 0.5, y + 0.5
    cols = [plane(t, (lambda v, k=k: v["c"][k]), px, py) for k in range(3)]
    return plane(t, S_, px, py), plane(t, T_, px, py), cols


def inc32(start, step, n):
    """float32 running sum: start, start + step, ... (n values)"""
    out = np.empty(n, np.float32)
    s = f32(start)
    st = f32(step)
    for i in range(n):
        out[i] = s
        s = f32(s + st)
    return out


@model
def m_f32inc_norm(t, y, xs, ctx):
    """s, t normalised, row start = f32 plane at the first pixel, + f32 gradient per pixel; u = f32(s * 64)"""
    px, py = xs + 0.5, y + 0.5
    s_ = lambda v: v["t"][0]
    t_ = lambda v: v["t"][1]
    gsx, _ = grads(t, s_, f32); gtx, _ = grads(t, t_, f32)
    s0 = plane(t, s_, px[0], py); t0 = plane(t, t_, px[0], py)
    ss = inc32(s0, gsx, len(xs)); tt = inc32(t0, gtx, len(xs))
    u = (ss * f32(64)).astype(np.float64); v = (tt * f32(64)).astype(np.float64)
    cols = [plane(t, (lambda vv, k=k: vv["c"][k]), px, py) for k in range(3)]
    return u, v, cols


@model
def m_f32inc_texel(t, y, xs, ctx):
    """u, v in texel units, row start = f32 plane, + f32 gradient per pixel"""
    px, py = xs + 0.5, y + 0.5
    gsx, _ = grads(t, S_, f32); gtx, _ = grads(t, T_, f32)
    s0 = plane(t, S_, px[0], py); t0 = plane(t, T_, px[0], py)
    u = inc32(s0, gsx, len(xs)).astype(np.float64); v = inc32(t0, gtx, len(xs)).astype(np.float64)
    cols = [plane(t, (lambda vv, k=k: vv["c"][k]), px, py) for k in range(3)]
    return u, v, cols


def evaluate(scene, names):
    tris, tex, real = load(scene)
    R5 = np.stack([(real >> 10) & 31, (real >> 5) & 31, real & 31], -1).astype(int)
    covs = [coverage(t) for t in tris]
    res = {}
    for nm in names:
        f = MODELS[nm]
        bad = n = 0
        badmap = []
        for t, rows in zip(tris, covs):
            ctx = {}
            for (y, xs) in rows:
                u, v, cols = f(t, y, xs, ctx)
                k = (K[y & 3][xs & 3] + 0.5) / 16
                ok = np.ones(len(xs), bool)
                if FINAL != "d" and tex is not None:
                    qs = final32(tex, np.asarray(u, float), np.asarray(v, float), cols, k, FINAL)
                    for ch in range(3):
                        ok &= np.clip(np.floor(qs[ch]).astype(int), 0, 31) == R5[y, xs, ch]
                else:
                    texel = bilinear(tex, np.asarray(u, float), np.asarray(v, float)) if tex is not None else np.full((len(xs), 3), 255.0)
                    for ch in range(3):
                        val = texel[:, ch] * np.asarray(cols[ch], float)
                        q = np.floor(val * 31 / 255 + k).astype(int)
                        q = np.clip(q, 0, 31)
                        ok &= q == R5[y, xs, ch]
                bad += int((~ok).sum()); n += len(xs)
                if (~ok).any():
                    badmap += [(y, int(x), tris.index(t), len(rows) and rows[0][0]) for x in xs[~ok]]
        res[nm] = (bad, n)
        if SHOW and badmap:
            print("   wrong at", badmap[:SHOW])
        print(f"{scene} {nm:20s} wrong {bad} of {n}", flush=True)
    return res




# ---- edge walk in float32 (rows from the largest y): the row start takes the previous start + one float step ----
def walk_starts(t, attr, rows, variant, ctx_key, ctx):
    """per row (in the walk order) the float32 attribute value at the row's first covered pixel"""
    key = (ctx_key, variant)
    if key in ctx:
        return ctx[key]
    gx, gy = grads(t, attr, f32 if "g32" in variant else float)
    gx, gy = float(gx), float(gy)
    order = sorted(rows, key=lambda r: -r[0])
    out = {}
    prev = None
    for (y, xs) in order:
        ix = int(xs[0])
        if prev is None:
            out[y] = f32(plane(t, attr, ix + 0.5, y + 0.5))
        else:
            py_, pix, pv = prev
            dy, di = py_ - y, ix - pix
            if "sep" in variant:
                st = f32(f32(f32(-gy) * f32(dy)) + f32(f32(gx) * f32(di)))
            else:
                st = f32(-gy * dy + gx * di)
            out[y] = f32(pv + st)
        prev = (y, ix, out[y])
    ctx[key] = (out, f32(gx))
    return ctx[key]


def make_walk(variant, unit="texel"):
    def m(t, y, xs, ctx):
        rows = ctx.get("rows")
        if rows is None:
            rows = ctx["rows"] = coverage(t)
        px, py = xs + 0.5, y + 0.5
        sc = 64.0 if unit == "texel" else 1.0
        sa = lambda v: v["t"][0] * sc
        ta = lambda v: v["t"][1] * sc
        (ss, gsx) = walk_starts(t, sa, rows, variant, "s", ctx)
        (tt, gtx) = walk_starts(t, ta, rows, variant, "t", ctx)
        u = inc32(ss[y], gsx, len(xs)); v = inc32(tt[y], gtx, len(xs))
        if unit != "texel":
            u = u * f32(64); v = v * f32(64)
        cols = [plane(t, (lambda vv, k=k: vv["c"][k]), px, py) for k in range(3)]
        return u.astype(np.float64), v.astype(np.float64), cols
    return m


for _v in ("j", "sep", "j_g32", "sep_g32"):
    for _u in ("texel", "norm"):
        MODELS[f"walk_{_v}_{_u}"] = make_walk(_v, _u)




def grads32(t, attr, anchor=0, recip=False, order=None):
    """float32 gradients with deltas from vertex `anchor` ('top' = largest y, then stream order); recip: multiply by f32(1 / area)"""
    F = f32
    pts = [v["p"] for v in t]
    a = [attr(v) for v in t]
    if anchor == "top":
        anchor = max(range(3), key=lambda i: (pts[i][1], -i))
    o = [(anchor + 1) % 3, (anchor + 2) % 3]
    A, B, C = pts[anchor], pts[o[0]], pts[o[1]]
    dx1, dy1, dx2, dy2 = F(F(B[0]) - F(A[0])), F(F(B[1]) - F(A[1])), F(F(C[0]) - F(A[0])), F(F(C[1]) - F(A[1]))
    ar = F(F(dx1 * dy2) - F(dx2 * dy1))
    d1, d2 = F(F(a[o[0]]) - F(a[anchor])), F(F(a[o[1]]) - F(a[anchor]))
    nx, ny = F(F(d1 * dy2) - F(d2 * dy1)), F(F(d2 * dx1) - F(d1 * dx2))
    if recip:
        r = F(F(1.0) / ar)
        return F(nx * r), F(ny * r)
    return F(nx / ar), F(ny / ar)


def walk_starts2(t, attr, rows, gfun, ctx_key, ctx, first="plane"):
    key = ctx_key
    if key in ctx:
        return ctx[key]
    gx, gy = gfun(t, attr)
    order = sorted(rows, key=lambda r: -r[0])
    out = {}
    prev = None
    for (y, xs) in order:
        ix = int(xs[0])
        if prev is None:
            if first == "plane":
                out[y] = f32(plane(t, attr, ix + 0.5, y + 0.5))
            else:   # f32 prestep from the top vertex
                pts = [v["p"] for v in t]
                ti = max(range(3), key=lambda i: (pts[i][1], -i))
                av = f32(attr(t[ti]))
                out[y] = f32(av + f32(f32(gx * f32(f32(ix + 0.5) - f32(pts[ti][0]))) + f32(gy * f32(f32(y + 0.5) - f32(pts[ti][1])))))
        else:
            py_, pix, pv = prev
            st = f32(f32(f32(-gy) * f32(py_ - y)) + f32(gx * f32(ix - pix)))
            out[y] = f32(pv + st)
        prev = (y, ix, out[y])
    ctx[key] = (out, gx)
    return ctx[key]


def make_walk2(anchor, recip, first="plane"):
    gfun = lambda t, attr: grads32(t, attr, anchor, recip)

    def m(t, y, xs, ctx):
        rows = ctx.get("rows")
        if rows is None:
            rows = ctx["rows"] = coverage(t)
        px, py = xs + 0.5, y + 0.5
        (ss, gsx) = walk_starts2(t, S_, rows, gfun, "s", ctx, first)
        (tt, gtx) = walk_starts2(t, T_, rows, gfun, "t", ctx, first)
        u = inc32(ss[y], gsx, len(xs)); v = inc32(tt[y], gtx, len(xs))
        cols = [plane(t, (lambda vv, k=k: vv["c"][k]), px, py) for k in range(3)]
        return u.astype(np.float64), v.astype(np.float64), cols
    return m


for _a in (0, 1, 2, "top"):
    for _r in (False, True):
        for _f in ("plane", "prestep"):
            MODELS[f"w2_a{_a}_{'rcp' if _r else 'div'}_{_f}"] = make_walk2(_a, _r, _f)



def make_walk3(anchor, recip, cscale, prod, canchor=None):
    """texture coordinates as make_walk2; colour walked the same way in float32 at scale cscale (1, 31, 255); prod: how the
    filtered texel (8-bit) and the colour combine into the 5-bit value before the dither ('d' double, 'f' float32 chain)"""
    gfun = lambda t, attr: grads32(t, attr, anchor, recip)
    cfun = lambda t, attr: grads32(t, attr, anchor if canchor is None else canchor, recip)

    def m(t, y, xs, ctx):
        rows = ctx.get("rows")
        if rows is None:
            rows = ctx["rows"] = coverage(t)
        (ss, gsx) = walk_starts2(t, S_, rows, gfun, "s", ctx)
        (tt, gtx) = walk_starts2(t, T_, rows, gfun, "t", ctx)
        u = inc32(ss[y], gsx, len(xs)); v = inc32(tt[y], gtx, len(xs))
        cols = []
        for k in range(3):
            (cs, gcx) = walk_starts2(t, (lambda vv, k=k: vv["c"][k] * cscale), rows, cfun, "c%d" % k, ctx)
            cols.append(inc32(cs[y], gcx, len(xs)).astype(np.float64) / cscale)
        return u.astype(np.float64), v.astype(np.float64), cols
    return m


for _cs in (1.0, 31.0, 255.0):
    for _a in (0, "top"):
        MODELS[f"w3_a{_a}_c{int(_cs)}"] = make_walk3(_a, False, _cs, "d")
        MODELS[f"w3_a{_a}_rcp_c{int(_cs)}"] = make_walk3(_a, True, _cs, "d")



def pick(t, anchor):
    pts = [v["p"] for v in t]
    if isinstance(anchor, int):
        return anchor
    order = sorted(range(3), key=lambda i: (pts[i][1], i))   # by y, then stream order
    if anchor == "miny":
        return order[0]
    if anchor == "midy":
        return order[1]
    if anchor == "maxy":
        return order[2]
    if anchor == "maxy_last":
        return sorted(range(3), key=lambda i: (pts[i][1], -i))[2]
    if anchor == "miny_last":
        return sorted(range(3), key=lambda i: (pts[i][1], -i))[0]
    raise ValueError(anchor)


def grads_t14(t, attr, anchor):
    """dXdx = dA_AC * t2 - dA_BC * t1, dXdy = dA_BC * t3 - dA_AC * t4 with t1 = dyAC / area ... (c = the anchor), all float32"""
    F = f32
    c = pick(t, anchor)
    a_, b_ = (c + 1) % 3, (c + 2) % 3
    pts = [v["p"] for v in t]
    A, B, C = pts[a_], pts[b_], pts[c]
    dxAC, dxBC = F(F(A[0]) - F(C[0])), F(F(B[0]) - F(C[0]))
    dyAC, dyBC = F(F(A[1]) - F(C[1])), F(F(B[1]) - F(C[1]))
    area = F(F(dxAC * dyBC) - F(dxBC * dyAC))
    r = F(F(1.0) / area)
    t1, t2, t3, t4 = F(dyAC * r), F(dyBC * r), F(dxAC * r), F(dxBC * r)
    va = [F(attr(v)) for v in t]
    dAC, dBC = F(va[a_] - va[c]), F(va[b_] - va[c])
    return F(F(dAC * t2) - F(dBC * t1)), F(F(dBC * t3) - F(dAC * t4))


def make_walk4(gform, anchor, first="plane", cscale=31.0):
    if gform == "t14":
        gfun = lambda t, attr: grads_t14(t, attr, anchor)
    else:
        gfun = lambda t, attr: grads32(t, attr, pick(t, anchor), gform == "rcp")

    def m(t, y, xs, ctx):
        rows = ctx.get("rows")
        if rows is None:
            rows = ctx["rows"] = coverage(t)
        (ss, gsx) = walk_starts3(t, S_, rows, gfun, "s", ctx, first, anchor)
        (tt, gtx) = walk_starts3(t, T_, rows, gfun, "t", ctx, first, anchor)
        u = inc32(ss[y], gsx, len(xs)); v = inc32(tt[y], gtx, len(xs))
        cols = []
        for k in range(3):
            (cs, gcx) = walk_starts3(t, (lambda vv, k=k: vv["c"][k] * cscale), rows, gfun, "c%d" % k, ctx, first, anchor)
            cols.append(inc32(cs[y], gcx, len(xs)).astype(np.float64) / cscale)
        return u.astype(np.float64), v.astype(np.float64), cols
    return m


def walk_starts3(t, attr, rows, gfun, key, ctx, first, anchor):
    if key in ctx:
        return ctx[key]
    gx, gy = gfun(t, attr)
    order = sorted(rows, key=lambda r: -r[0])
    out = {}
    prev = None
    F = f32
    for (y, xs) in order:
        ix = int(xs[0])
        if prev is None:
            if first == "plane":
                out[y] = F(plane(t, attr, ix + 0.5, y + 0.5))
            else:   # f32 from a vertex: 'v:<anchor>'
                pts = [v["p"] for v in t]
                vi = pick(t, first[2:]) if first[2:] not in ("0", "1", "2") else int(first[2:])
                av = F(attr(t[vi]))
                ddx, ddy = F(F(ix + 0.5) - F(pts[vi][0])), F(F(y + 0.5) - F(pts[vi][1]))
                out[y] = F(av + F(F(gx * ddx) + F(gy * ddy)))
        else:
            py_, pix, pv = prev
            st = F(F(F(-gy) * F(py_ - y)) + F(gx * F(ix - pix)))
            out[y] = F(pv + st)
        prev = (y, ix, out[y])
    ctx[key] = (out, gx)
    return ctx[key]


for _g in ("t14", "rcp", "div"):
    for _a in ("miny", "midy", "maxy", 0, 1, 2):
        MODELS[f"w4_{_g}_{_a}"] = make_walk4(_g, _a)
        MODELS[f"w4_{_g}_{_a}_vtop"] = make_walk4(_g, _a, "v:maxy")
        MODELS[f"w4_{_g}_{_a}_vanc"] = make_walk4(_g, _a, "v:" + str(_a))



def split_info(t):
    pts = [v["p"] for v in t]
    o = sorted(range(3), key=lambda i: (pts[i][1], i))
    vmin, vmid, vmax = [pts[i] for i in o]
    if vmin[1] == vmax[1]:
        return False, None, o
    xl = vmax[0] + (vmin[0] - vmax[0]) * (vmid[1] - vmax[1]) / (vmin[1] - vmax[1])
    return vmid[0] < xl, vmid, o


def walk_starts5(t, attr, rows, gfun, key, ctx, first, restart):
    if key in ctx:
        return ctx[key]
    gx, gy = gfun(t, attr)
    midleft, vmid, o = split_info(t)
    order = sorted(rows, key=lambda r: -r[0])
    out = {}
    prev = None
    F = f32
    switched = False
    for (y, xs) in order:
        ix = int(xs[0])
        lower = midleft and (y + 0.5) < vmid[1]
        if prev is None or (lower and not switched and restart != "none"):
            mode = first if prev is None else restart
            if mode == "plane":
                out[y] = F(plane(t, attr, ix + 0.5, y + 0.5))
            else:   # 'v:<vertex role>' f32 prestep from that vertex (maxy / midy / miny)
                pts = [v["p"] for v in t]
                vi = pick(t, mode[2:])
                av = F(attr(t[vi]))
                ddx, ddy = F(F(ix + 0.5) - F(pts[vi][0])), F(F(y + 0.5) - F(pts[vi][1]))
                if mode.endswith("_s"):
                    pass
                out[y] = F(av + F(F(gx * ddx) + F(gy * ddy)))
            if lower:
                switched = True
        else:
            py_, pix, pv = prev
            st = F(F(F(-gy) * F(py_ - y)) + F(gx * F(ix - pix)))
            out[y] = F(pv + st)
            if lower:
                switched = True
        prev = (y, ix, out[y])
    ctx[key] = (out, gx)
    return ctx[key]


def make_walk5(anchor, first="plane", restart="plane", cscale=31.0):
    gfun = lambda t, attr: grads_t14(t, attr, anchor)

    def m(t, y, xs, ctx):
        rows = ctx.get("rows")
        if rows is None:
            rows = ctx["rows"] = coverage(t)
        (ss, gsx) = walk_starts5(t, S_, rows, gfun, "s", ctx, first, restart)
        (tt, gtx) = walk_starts5(t, T_, rows, gfun, "t", ctx, first, restart)
        u = inc32(ss[y], gsx, len(xs)); v = inc32(tt[y], gtx, len(xs))
        cols = []
        for k in range(3):
            (cs, gcx) = walk_starts5(t, (lambda vv, k=k: vv["c"][k] * cscale), rows, gfun, "c%d" % k, ctx, first, restart)
            cols.append(inc32(cs[y], gcx, len(xs)).astype(np.float64) / cscale)
        return u.astype(np.float64), v.astype(np.float64), cols
    return m


for _a in ("miny", "maxy", "midy"):
    for _f in ("plane", "v:maxy"):
        for _r in ("none", "plane", "v:midy", "v:maxy", "v:miny"):
            MODELS[f"w5_{_a}_{_f}_{_r}"] = make_walk5(_a, _f, _r)



def walk_starts6(t, attr, rows, gfun, key, ctx, first, restart, stepform):
    """walk_starts5 with the row step taken from two precomputed float32 steps of the current left edge: little for its floor x
    step per row, big = f32(little + dA/dx) (stepform 'lb'), or big = f32(-dA/dy + dA/dx * (little + 1)) ('lb2'); 'free' = per-row"""
    if key in ctx:
        return ctx[key]
    gx, gy = gfun(t, attr)
    F = f32
    midleft, vmid, o = split_info(t)
    pts = [v["p"] for v in t]
    vmin, vmax = pts[o[0]], pts[o[2]]
    segs = [(vmax, vmid), (vmid, vmin)] if midleft else [(vmax, vmin)]

    def little_of(seg):
        (xt, yt), (xb, yb) = seg
        if yt == yb:
            return 0
        return int(np.floor((xb - xt) / (yt - yb)))
    order = sorted(rows, key=lambda r: -r[0])
    out = {}
    prev = None
    switched = False
    for (y, xs) in order:
        ix = int(xs[0])
        lower = midleft and (y + 0.5) < vmid[1]
        seg = segs[1] if lower else segs[0]
        if prev is None or (lower and not switched and restart != "none"):
            mode = first if prev is None else restart
            if mode == "plane":
                out[y] = F(plane(t, attr, ix + 0.5, y + 0.5))
            else:
                vi = pick(t, mode[2:])
                av = F(attr(t[vi]))
                ddx, ddy = F(F(ix + 0.5) - F(pts[vi][0])), F(F(y + 0.5) - F(pts[vi][1]))
                out[y] = F(av + F(F(gx * ddx) + F(gy * ddy)))
            switched = switched or lower
        else:
            py_, pix, pv = prev
            di = ix - pix
            lt = little_of(seg)
            if stepform != "free" and py_ - y == 1 and di in (lt, lt + 1):
                sl = F(F(-gy) + F(gx * F(lt)))
                sb = F(sl + gx) if stepform == "lb" else F(F(-gy) + F(gx * F(lt + 1)))
                st = sl if di == lt else sb
            else:
                st = F(F(F(-gy) * F(py_ - y)) + F(gx * F(di)))
            out[y] = F(pv + st)
            switched = switched or lower
        prev = (y, ix, out[y])
    ctx[key] = (out, gx)
    return ctx[key]


def make_walk6(anchor="miny", first="plane", restart="plane", stepform="lb", cscale=31.0):
    gfun = lambda t, attr: grads_t14(t, attr, anchor)

    def m(t, y, xs, ctx):
        rows = ctx.get("rows")
        if rows is None:
            rows = ctx["rows"] = coverage(t)
        (ss, gsx) = walk_starts6(t, S_, rows, gfun, "s", ctx, first, restart, stepform)
        (tt, gtx) = walk_starts6(t, T_, rows, gfun, "t", ctx, first, restart, stepform)
        u = inc32(ss[y], gsx, len(xs)); v = inc32(tt[y], gtx, len(xs))
        cols = []
        for k in range(3):
            (cs, gcx) = walk_starts6(t, (lambda vv, k=k: vv["c"][k] * cscale), rows, gfun, "c%d" % k, ctx, first, restart, stepform)
            cols.append(inc32(cs[y], gcx, len(xs)).astype(np.float64) / cscale)
        return u.astype(np.float64), v.astype(np.float64), cols
    return m


for _r in ("plane", "v:midy", "v:miny"):
    for _sf in ("lb", "lb2", "free"):
        MODELS[f"w6_{_r}_{_sf}"] = make_walk6("miny", "plane", _r, _sf)



def walk_starts7(t, attr, rows, gfun, key, ctx, variant):
    """edge walk: the attribute is carried along the left edge (at the edge's x on each row centre) with one float32 step per row,
    dA/dy_edge = f32(-dA/dy + dA/dx * dxdy_row), and each span starts at f32(A_edge + f32(dA/dx * f32(ix + 0.5 - x_edge)));
    x_edge itself float32-walked ('xf') or exact ('xe'); the edge value restarts at the middle vertex (left turn) and at the top as
    f32(plane) at (x_edge, yc) ('p') or from the vertex ('v')"""
    if key in ctx:
        return ctx[key]
    F = f32
    gx, gy = gfun(t, attr)
    midleft, vmid, o = split_info(t)
    pts = [v["p"] for v in t]
    vmin, vmax = pts[o[0]], pts[o[2]]
    segs = [(vmax, vmid), (vmid, vmin)] if midleft else [(vmax, vmin)]
    order = sorted(rows, key=lambda r: -r[0])
    out = {}
    cur_seg = None
    ae = xe = None
    for (y, xs) in order:
        ix = int(xs[0])
        yc = y + 0.5
        lower = midleft and yc < vmid[1]
        seg = segs[1] if lower else segs[0]
        (xt, yt), (xb, yb) = seg
        dxdy = (xb - xt) / (yb - yt) if yb != yt else 0.0     # x per unit y along the edge
        if seg is not cur_seg:
            cur_seg = seg
            x_exact = xt + dxdy * (yc - yt)
            xe = F(x_exact)
            if "v" in variant:
                vi = [k for k in range(3) if pts[k] == (xt, yt)][0]
                ae = F(F(attr(t[vi])) + F(F(gy * F(F(yc) - F(yt))) + F(gx * F(xe - F(xt)))))
            else:
                ae = F(plane(t, attr, x_exact, yc))
            step_e = F(F(-gy) + F(gx * F(-dxdy)))
            step_x = F(-dxdy)
            prev_y = y
        else:
            n = prev_y - y
            for _ in range(n):
                ae = F(ae + step_e)
                xe = F(xe + step_x) if "xf" in variant else F(xt + dxdy * (yc - yt))
            prev_y = y
        if "xe" in variant:
            xe = F(xt + dxdy * (yc - yt))
        out[y] = F(ae + F(gx * F(F(ix + 0.5) - xe)))
    ctx[key] = (out, gx)
    return ctx[key]


def make_walk7(variant, anchor="miny", cscale=31.0):
    gfun = lambda t, attr: grads_t14(t, attr, anchor)

    def m(t, y, xs, ctx):
        rows = ctx.get("rows")
        if rows is None:
            rows = ctx["rows"] = coverage(t)
        (ss, gsx) = walk_starts7(t, S_, rows, gfun, "s", ctx, variant)
        (tt, gtx) = walk_starts7(t, T_, rows, gfun, "t", ctx, variant)
        u = inc32(ss[y], gsx, len(xs)); v = inc32(tt[y], gtx, len(xs))
        cols = []
        for k in range(3):
            (cs, gcx) = walk_starts7(t, (lambda vv, k=k: vv["c"][k] * cscale), rows, gfun, "c%d" % k, ctx, variant)
            cols.append(inc32(cs[y], gcx, len(xs)).astype(np.float64) / cscale)
        return u.astype(np.float64), v.astype(np.float64), cols
    return m


for _v in ("p_xf", "p_xe", "v_xf", "v_xe"):
    MODELS["w7_" + _v] = make_walk7(_v)



def walk_starts8(t, attr, rows, gfun, key, ctx, form):
    if key in ctx:
        return ctx[key]
    F = f32
    gx, gy = gfun(t, attr)
    midleft, vmid, o = split_info(t)
    order = sorted(rows, key=lambda r: -r[0])
    out = {}
    prev = None
    switched = False
    for (y, xs) in order:
        ix = int(xs[0])
        lower = midleft and (y + 0.5) < vmid[1]
        if prev is None or (lower and not switched):
            out[y] = F(plane(t, attr, ix + 0.5, y + 0.5))
        else:
            py_, pix, pv = prev
            dy, di = py_ - y, ix - pix
            if form == "two_yx":
                v = F(pv + F(F(-gy) * F(dy))); v = F(v + F(gx * F(di)))
            elif form == "two_xy":
                v = F(pv + F(gx * F(di))); v = F(v + F(F(-gy) * F(dy)))
            elif form == "rep":
                v = pv
                for _ in range(dy):
                    v = F(v + F(-gy))
                for _ in range(abs(di)):
                    v = F(v + (gx if di > 0 else F(-gx)))
            elif form == "rep_xy":
                v = pv
                for _ in range(abs(di)):
                    v = F(v + (gx if di > 0 else F(-gx)))
                for _ in range(dy):
                    v = F(v + F(-gy))
            else:
                v = F(pv + F(F(F(-gy) * F(dy)) + F(gx * F(di))))
            out[y] = v
        switched = switched or lower
        prev = (y, ix, out[y])
    ctx[key] = (out, gx)
    return ctx[key]


def make_walk8(form, anchor="miny", cscale=31.0):
    gfun = lambda t, attr: grads_t14(t, attr, anchor)

    def m(t, y, xs, ctx):
        rows = ctx.get("rows")
        if rows is None:
            rows = ctx["rows"] = coverage(t)
        (ss, gsx) = walk_starts8(t, S_, rows, gfun, "s", ctx, form)
        (tt, gtx) = walk_starts8(t, T_, rows, gfun, "t", ctx, form)
        u = inc32(ss[y], gsx, len(xs)); v = inc32(tt[y], gtx, len(xs))
        cols = []
        for k in range(3):
            (cs, gcx) = walk_starts8(t, (lambda vv, k=k: vv["c"][k] * cscale), rows, gfun, "c%d" % k, ctx, form)
            cols.append(inc32(cs[y], gcx, len(xs)).astype(np.float64) / cscale)
        return u.astype(np.float64), v.astype(np.float64), cols
    return m


for _f in ("joint", "two_yx", "two_xy", "rep", "rep_xy"):
    MODELS["w8_" + _f] = make_walk8(_f)



def grads_x87(t, attr, anchor, form):
    """t14 gradients with double intermediates rounded to float only at the named stores (x87 at 53-bit precision): form is a set
    of letters: 'a' area stored, 't' t1..t4 stored, 'd' dAC / dBC stored, 'g' the results stored (always)"""
    F = f32
    D = float
    c = pick(t, anchor)
    a_, b_ = (c + 1) % 3, (c + 2) % 3
    pts = [v["p"] for v in t]
    A, B, C = pts[a_], pts[b_], pts[c]
    dxAC, dxBC = F(F(A[0]) - F(C[0])), F(F(B[0]) - F(C[0]))
    dyAC, dyBC = F(F(A[1]) - F(C[1])), F(F(B[1]) - F(C[1]))
    area = D(dxAC) * D(dyBC) - D(dxBC) * D(dyAC)
    if "a" in form:
        area = D(F(area))
    r = 1.0 / area
    if "r" in form:
        r = D(F(r))
    t1, t2, t3, t4 = D(dyAC) * r, D(dyBC) * r, D(dxAC) * r, D(dxBC) * r
    if "t" in form:
        t1, t2, t3, t4 = D(F(t1)), D(F(t2)), D(F(t3)), D(F(t4))
    va = [F(attr(v)) for v in t]
    dAC, dBC = D(va[a_]) - D(va[c]), D(va[b_]) - D(va[c])
    if "d" in form:
        dAC, dBC = D(F(dAC)), D(F(dBC))
    return F(dAC * t2 - dBC * t1), F(dBC * t3 - dAC * t4)


def walk_starts9(t, attr, rows, gfun, key, ctx, step):
    if key in ctx:
        return ctx[key]
    F = f32
    gx, gy = gfun(t, attr)
    midleft, vmid, o = split_info(t)
    order = sorted(rows, key=lambda r: -r[0])
    out = {}
    prev = None
    switched = False
    for (y, xs) in order:
        ix = int(xs[0])
        lower = midleft and (y + 0.5) < vmid[1]
        if prev is None or (lower and not switched):
            out[y] = F(plane(t, attr, ix + 0.5, y + 0.5))
        else:
            py_, pix, pv = prev
            dy, di = py_ - y, ix - pix
            if step == "f":
                out[y] = F(pv + F(F(F(-gy) * F(dy)) + F(gx * F(di))))
            elif step == "j":
                out[y] = F(pv + F(-float(gy) * dy + float(gx) * di))
            else:   # "n": no store of the step
                out[y] = F(float(pv) + (-float(gy) * dy + float(gx) * di))
        switched = switched or lower
        prev = (y, ix, out[y])
    ctx[key] = (out, gx)
    return ctx[key]


def make_walk9(gform, step, anchor="miny", cscale=31.0):
    gfun = (lambda t, attr: grads_t14(t, attr, anchor)) if gform == "f32" else (lambda t, attr: grads_x87(t, attr, anchor, gform))

    def m(t, y, xs, ctx):
        rows = ctx.get("rows")
        if rows is None:
            rows = ctx["rows"] = coverage(t)
        (ss, gsx) = walk_starts9(t, S_, rows, gfun, "s", ctx, step)
        (tt, gtx) = walk_starts9(t, T_, rows, gfun, "t", ctx, step)
        u = inc32(ss[y], gsx, len(xs)); v = inc32(tt[y], gtx, len(xs))
        cols = []
        for k in range(3):
            (cs, gcx) = walk_starts9(t, (lambda vv, k=k: vv["c"][k] * cscale), rows, gfun, "c%d" % k, ctx, step)
            cols.append(inc32(cs[y], gcx, len(xs)).astype(np.float64) / cscale)
        return u.astype(np.float64), v.astype(np.float64), cols
    return m


for _g in ("f32", "atd", "at", "ad", "td", "a", "t", "d", "", "artd", "ar"):
    for _st in ("f", "j", "n"):
        MODELS[f"w9_{_g or 'none'}_{_st}"] = make_walk9(_g, _st)


if __name__ == "__main__":
    a = sys.argv[1:]
    if "--final" in a:
        i = a.index("--final"); FINAL = a[i + 1]; del a[i:i + 2]
    if "--nearest" in a:
        a.remove("--nearest"); NEAREST = True
    if "--show" in a:
        i = a.index("--show"); SHOW = int(a[i + 1]); del a[i:i + 2]
    evaluate(a[0], a[1:] or list(MODELS))
