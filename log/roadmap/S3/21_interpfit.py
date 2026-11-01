"""S3.21 (U-1392): the generic GL's colour interpolation, fitted on 21_modprobe.py's gour_untex scene (the real GL's frame from
glreplay.exe, port/corpus/gl/probe/s321_gour_untex/out.bin): every drawn pixel's 5-bit value against candidate interpolation
arithmetic over the exact snapped vertices (21_vcheck's viewport rule; the probe's projection is glOrtho(0, 800, 0, 600)).

    cd <tree> && ~/bin/py -3.12 ../../../log/roadmap/S3/21_interpfit.py
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
    # glOrtho(0, 800, 0, 600, -1, 1): m00 = 2/800, m03 = -1; float32 transform translation first, then the viewport rule
    m00, m03 = f32(2 / 800), f32(-1.0)
    m11, m13 = f32(2 / 600), f32(-1.0)
    cx = f32(m03 + f32(f32(x) * m00)); cy = f32(m13 + f32(f32(y) * m11))
    wx = float(f32(cx * f32(400 - 1 / 256))) + 400; wy = float(f32(cy * f32(300 - 1 / 256))) + 300
    return np.round(256 * wx) / 256, np.round(256 * wy) / 256


def tris(d):
    out = []
    cur = []
    col = None
    for ln in (d / "snaps" / "s1" / "pass.gl.txt").read_text().splitlines():
        t = ln.split(" ", 1)
        if t[0] == "glColor4f":
            col = [fbits(h) for h in t[1].split(",")]
        elif t[0] == "glVertex3f":
            x, y, _ = [fbits(h) for h in t[1].split(",")]
            cur.append((snapped(x, y), col))
        elif t[0] == "glEnd":
            out.append(cur); cur = []
    return out


def edge(ax, ay, bx, by, px, py):
    return (bx - ax) * (py - ay) - (by - ay) * (px - ax)


MODELS = {}


def model(f):
    MODELS[f.__name__] = f
    return f


@model
def exact(t, px, py, ch):
    (a, ca), (b, cb), (c, cc) = t
    ar = edge(*a, *b, *c)
    w0 = edge(*b, *c, px, py); w1 = edge(*c, *a, px, py); w2 = edge(*a, *b, px, py)
    return (w0 * ca[ch] + w1 * cb[ch] + w2 * cc[ch]) / ar


def grads(t, ch, scale, prec):
    """dc/dx, dc/dy from the snapped vertices: plane through the three (x, y, c)"""
    F = (lambda v: f32(v)) if prec == "f" else (lambda v: v)
    (a, ca), (b, cb), (c, cc) = t
    x0, y0 = a; x1, y1 = b; x2, y2 = c
    c0, c1, c2 = F(ca[ch] * scale), F(cb[ch] * scale), F(cc[ch] * scale)
    dx1, dy1, dx2, dy2 = F(x1 - x0), F(y1 - y0), F(x2 - x0), F(y2 - y0)
    area = F(F(dx1 * dy2) - F(dx2 * dy1))
    dc1, dc2 = F(c1 - c0), F(c2 - c0)
    ddx = F(F(F(dc1 * dy2) - F(dc2 * dy1)) / area)
    ddy = F(F(F(dc2 * dx1) - F(dc1 * dx2)) / area)
    return c0, x0, y0, ddx, ddy


def plane_model(scale, prec, anchor):
    def m(t, px, py, ch):
        F = (lambda v: f32(v)) if prec == "f" else (lambda v: v)
        c0, x0, y0, ddx, ddy = grads(t, ch, scale, prec)
        if anchor == "vertex":
            v = F(F(c0 + F(ddx * F(px - x0))) + F(ddy * F(py - y0)))
        else:   # dy first
            v = F(F(c0 + F(ddy * F(py - y0))) + F(ddx * F(px - x0)))
        return float(v) / scale
    m.__name__ = f"plane_s{scale}_{prec}_{anchor}"
    return m


for sc_ in (1.0, 31.0, 255.0):
    for pr in ("f", "d"):
        for an in ("vertex", "dyfirst"):
            model(plane_model(sc_, pr, an))


def main():
    d = P.PROBE / "s321_gour_untex"
    real = np.frombuffer((d / "out.bin").read_bytes(), "<u2").reshape(600, 800)
    R5 = np.stack([(real >> 10) & 31, (real >> 5) & 31, real & 31], -1)
    T = tris(d)
    # pixels each triangle covers (GlRaster's tie rule; the probe's triangles are counter-clockwise in window space)
    res = {k: [0, 0] for k in MODELS}
    for t in T:
        (a, _), (b, _), (c, _) = t
        ar = edge(*a, *b, *c)
        if ar < 0:
            t = [t[0], t[2], t[1]]
            (a, _), (b, _), (c, _) = t
            ar = -ar
        xs = [a[0], b[0], c[0]]; ys = [a[1], b[1], c[1]]
        for y in range(int(min(ys)), int(max(ys)) + 1):
            for x in range(int(min(xs)), int(max(xs)) + 1):
                px, py = x + 0.5, y + 0.5
                w = (edge(*b, *c, px, py), edge(*c, *a, px, py), edge(*a, *b, px, py))
                if min(w) <= 0:   # skip edge pixels (tie rules are not this probe's question)
                    continue
                k = (K[y & 3][x & 3] + 0.5) / 16
                for name, fn in MODELS.items():
                    bad = False
                    for ch in range(3):
                        v = fn(t, px, py, ch)
                        o = min(31, max(0, int(np.floor(v * 31 + k))))
                        if o != R5[y, x, ch]:
                            bad = True
                    res[name][0] += bad
                    res[name][1] += 1
    for name, (b, n) in sorted(res.items(), key=lambda kv: kv[1][0]):
        print(f"  {name:28s} wrong px {b:6d} of {n}")


if __name__ == "__main__" and len(sys.argv) == 1:
    main()


def span_models():
    """span-based: per row, the first covered pixel's value from the plane (start), then + (x - xs) * step; start and step
    quantised to 2^-F of the colour scaled by S (truncation `t` or rounding `r`)"""
    out = {}
    for S in (1.0, 31.0, 255.0):
        for F in (12, 14, 15, 16, 18, 20, 23):
            for q in ("t", "r"):
                out[f"span_S{S:g}_F{F}_{q}"] = (S, F, q)
    return out


def main_span():
    d = P.PROBE / "s321_gour_untex"
    real = np.frombuffer((d / "out.bin").read_bytes(), "<u2").reshape(600, 800)
    R5 = np.stack([(real >> 10) & 31, (real >> 5) & 31, real & 31], -1).astype(int)
    T = tris(d)
    SM = span_models()
    res = {k: 0 for k in SM}
    res["exact"] = 0
    n = 0
    for t in T:
        (a, _), (b, _), (c, _) = t
        ar = edge(*a, *b, *c)
        if ar < 0:
            t = [t[0], t[2], t[1]]
            (a, _), (b, _), (c, _) = t
            ar = -ar
        xs_ = [a[0], b[0], c[0]]; ys_ = [a[1], b[1], c[1]]
        X = np.arange(int(min(xs_)), int(max(xs_)) + 1)
        for y in range(int(min(ys_)), int(max(ys_)) + 1):
            px, py = X + 0.5, y + 0.5
            w0 = edge(*b, *c, px, py); w1 = edge(*c, *a, px, py); w2 = edge(*a, *b, px, py)
            cov = (w0 >= 0) & (w1 >= 0) & (w2 >= 0)
            inner = (w0 > 0) & (w1 > 0) & (w2 > 0)
            if not cov.any():
                continue
            xs0 = X[cov][0]
            k = (K[y & 3][X & 3] + 0.5) / 16
            ok = {m: np.ones(len(X), bool) for m in res}
            for ch in range(3):
                (_, ca), (_, cb), (_, cc) = t
                v = (w0 * ca[ch] + w1 * cb[ch] + w2 * cc[ch]) / ar
                ok["exact"] &= np.floor(v * 31 + k).astype(int) == R5[y, X, ch]
                # plane gradients (double)
                ddx = ((cb[ch] - ca[ch]) * (c[1] - a[1]) - (cc[ch] - ca[ch]) * (b[1] - a[1])) / ar
                v0 = v[X == xs0][0]
                for m, (S, F, q) in SM.items():
                    sc = 2.0 ** F
                    Q = (lambda z: np.floor(z * sc) / sc) if q == "t" else (lambda z: np.floor(z * sc + 0.5) / sc)
                    st, sp = Q(v0 * S), Q(ddx * S)
                    vv = (st + (X - xs0) * sp) / S
                    ok[m] &= np.floor(vv * 31 + k).astype(int) == R5[y, X, ch]
            for m in res:
                res[m] += int((~ok[m] & inner).sum())
            n += int(inner.sum())
    for m, b in sorted(res.items(), key=lambda kv: kv[1])[:12]:
        print(f"  {m:22s} wrong px {b:6d} of {n}")


if __name__ == "__main__" and len(sys.argv) > 1 and sys.argv[1] == "span":
    main_span()


def main_quant():
    """the exact colour, then the 5-bit-scaled value (or the 8-bit value) held with F fractional bits before the dither add"""
    d = P.PROBE / "s321_gour_untex"
    real = np.frombuffer((d / "out.bin").read_bytes(), "<u2").reshape(600, 800)
    R5 = np.stack([(real >> 10) & 31, (real >> 5) & 31, real & 31], -1).astype(int)
    T = tris(d)
    cfgs = [(S, F, q) for S in (31.0, 255.0, 1.0) for F in range(6, 24) for q in ("t", "r")]
    res = {c: 0 for c in cfgs}
    n = 0
    for t in T:
        (a, _), (b, _), (c, _) = t
        ar = edge(*a, *b, *c)
        if ar < 0:
            t = [t[0], t[2], t[1]]
            (a, _), (b, _), (c, _) = t
            ar = -ar
        xs_ = [a[0], b[0], c[0]]; ys_ = [a[1], b[1], c[1]]
        X = np.arange(int(min(xs_)), int(max(xs_)) + 1)
        for y in range(int(min(ys_)), int(max(ys_)) + 1):
            px, py = X + 0.5, y + 0.5
            w0 = edge(*b, *c, px, py); w1 = edge(*c, *a, px, py); w2 = edge(*a, *b, px, py)
            inner = (w0 > 0) & (w1 > 0) & (w2 > 0)
            if not inner.any():
                continue
            k = (K[y & 3][X & 3] + 0.5) / 16
            ok = {cf: np.ones(len(X), bool) for cf in cfgs}
            for ch in range(3):
                (_, ca), (_, cb), (_, cc) = t
                v = (w0 * ca[ch] + w1 * cb[ch] + w2 * cc[ch]) / ar
                for cf in cfgs:
                    S, F, q = cf
                    sc = 2.0 ** F
                    z = v * S
                    z = np.floor(z * sc) / sc if q == "t" else np.floor(z * sc + 0.5) / sc
                    ok[cf] &= np.floor(z / S * 31 + k).astype(int) == R5[y, X, ch]
            for cf in cfgs:
                res[cf] += int((~ok[cf] & inner).sum())
            n += int(inner.sum())
    for cf, b in sorted(res.items(), key=lambda kv: kv[1])[:10]:
        print(f"  S {cf[0]:g} F {cf[1]:2d} {cf[2]}: wrong px {b:6d} of {n}")


if __name__ == "__main__" and len(sys.argv) > 1 and sys.argv[1] == "quant":
    main_quant()


def unsnapped(x, y):
    m00, m03 = f32(2 / 800), f32(-1.0)
    m11, m13 = f32(2 / 600), f32(-1.0)
    cx = f32(m03 + f32(f32(x) * m00)); cy = f32(m13 + f32(f32(y) * m11))
    return float(f32(cx * f32(400 - 1 / 256))) + 400, float(f32(cy * f32(300 - 1 / 256))) + 300


def main_anchor():
    """attribute plane through other anchor positions than the snapped vertices; coverage from the snapped ones"""
    d = P.PROBE / "s321_gour_untex"
    real = np.frombuffer((d / "out.bin").read_bytes(), "<u2").reshape(600, 800)
    R5 = np.stack([(real >> 10) & 31, (real >> 5) & 31, real & 31], -1).astype(int)
    raw = []
    cur = []
    col = None
    for ln in (d / "snaps" / "s1" / "pass.gl.txt").read_text().splitlines():
        t = ln.split(" ", 1)
        if t[0] == "glColor4f":
            col = [fbits(h) for h in t[1].split(",")]
        elif t[0] == "glVertex3f":
            x, y, _ = [fbits(h) for h in t[1].split(",")]
            cur.append((snapped(x, y), unsnapped(x, y), col))
        elif t[0] == "glEnd":
            raw.append(cur); cur = []
    offs = [(0, 0)] + [(dx / 512, dy / 512) for dx in (-2, -1, 1, 2) for dy in (-2, -1, 0, 1, 2)] + [(0, dy / 512) for dy in (-2, -1, 1, 2)]
    cfgs = [("snap", o) for o in offs] + [("unsnap", (0, 0)), ("unsnap_x", (0, 0)), ("unsnap_y", (0, 0))]
    res = {c: 0 for c in cfgs}
    n = 0
    for t in raw:
        S = [v[0] for v in t]; U = [v[1] for v in t]; C = [v[2] for v in t]
        ar = edge(*S[0], *S[1], *S[2])
        if ar < 0:
            S = [S[0], S[2], S[1]]; U = [U[0], U[2], U[1]]; C = [C[0], C[2], C[1]]
            ar = -ar
        X = np.arange(int(min(s[0] for s in S)), int(max(s[0] for s in S)) + 1)
        for y in range(int(min(s[1] for s in S)), int(max(s[1] for s in S)) + 1):
            px, py = X + 0.5, y + 0.5
            w = [edge(*S[1], *S[2], px, py), edge(*S[2], *S[0], px, py), edge(*S[0], *S[1], px, py)]
            inner = (w[0] > 0) & (w[1] > 0) & (w[2] > 0)
            if not inner.any():
                continue
            k = (K[y & 3][X & 3] + 0.5) / 16
            for cf in cfgs:
                kind, (ox, oy) = cf
                A = S if kind == "snap" else [(U[i][0] if kind in ("unsnap", "unsnap_x") else S[i][0],
                                               U[i][1] if kind in ("unsnap", "unsnap_y") else S[i][1]) for i in range(3)]
                aa = edge(*A[0], *A[1], *A[2])
                qx, qy = px + ox, py + oy
                ww = [edge(*A[1], *A[2], qx, qy), edge(*A[2], *A[0], qx, qy), edge(*A[0], *A[1], qx, qy)]
                ok = np.ones(len(X), bool)
                for ch in range(3):
                    v = (ww[0] * C[0][ch] + ww[1] * C[1][ch] + ww[2] * C[2][ch]) / aa
                    ok &= np.floor(v * 31 + k).astype(int) == R5[y, X, ch]
                res[cf] += int((~ok & inner).sum())
            n += int(inner.sum())
    for cf, b in sorted(res.items(), key=lambda kv: kv[1])[:10]:
        print(f"  {cf[0]:9s} offset ({cf[1][0]*512:+.0f}, {cf[1][1]*512:+.0f})/512: wrong px {b:6d} of {n}")


if __name__ == "__main__" and len(sys.argv) > 1 and sys.argv[1] == "anchor":
    main_anchor()


def main_pertri():
    d = P.PROBE / "s321_gour_untex"
    real = np.frombuffer((d / "out.bin").read_bytes(), "<u2").reshape(600, 800)
    R5 = np.stack([(real >> 10) & 31, (real >> 5) & 31, real & 31], -1).astype(int)
    T = tris(d)
    offs = [(dx / 1024, dy / 1024) for dx in range(-6, 7) for dy in range(-6, 7)]
    tot0 = totb = 0
    rows = []
    for ti, t in enumerate(T):
        S = [v[0] for v in t]; C = [v[1] for v in t]
        ar = edge(*S[0], *S[1], *S[2])
        if ar < 0:
            S = [S[0], S[2], S[1]]; C = [C[0], C[2], C[1]]; ar = -ar
        X = np.arange(int(min(s[0] for s in S)), int(max(s[0] for s in S)) + 1)
        err = np.zeros(len(offs), int)
        for y in range(int(min(s[1] for s in S)), int(max(s[1] for s in S)) + 1):
            px, py = X + 0.5, y + 0.5
            w = [edge(*S[1], *S[2], px, py), edge(*S[2], *S[0], px, py), edge(*S[0], *S[1], px, py)]
            inner = (w[0] > 0) & (w[1] > 0) & (w[2] > 0)
            if not inner.any():
                continue
            k = (K[y & 3][X & 3] + 0.5) / 16
            for j, (ox, oy) in enumerate(offs):
                ww = [edge(*S[1], *S[2], px + ox, py + oy), edge(*S[2], *S[0], px + ox, py + oy), edge(*S[0], *S[1], px + ox, py + oy)]
                ok = np.ones(len(X), bool)
                for ch in range(3):
                    v = (ww[0] * C[0][ch] + ww[1] * C[1][ch] + ww[2] * C[2][ch]) / ar
                    ok &= np.floor(v * 31 + k).astype(int) == R5[y, X, ch]
                err[j] += int((~ok & inner).sum())
        j0 = offs.index((0, 0))
        jb = int(np.argmin(err))
        tot0 += err[j0]; totb += err[jb]
        rows.append((ti, err[j0], err[jb], offs[jb], S))
    print("total at centre", tot0, "with the best offset per triangle", totb)
    for ti, e0, eb, o, S in rows[:40]:
        fr = [((s[0] * 256) % 256 / 256, (s[1] * 256) % 256 / 256) for s in S]
        print(f"  tri {ti:3d} centre {e0:3d} best {eb:3d} at ({o[0]*1024:+.0f},{o[1]*1024:+.0f})/1024  verts {[(round(s[0],4), round(s[1],4)) for s in S]}")


if __name__ == "__main__" and len(sys.argv) > 1 and sys.argv[1] == "pertri":
    main_pertri()


def main_edge():
    """edge walking: per row (pixel-centre line py) the left edge's exact intersection xe and value ve; the span's pixels take
    ve + (px - xq) * dv/dx with xq = xe held to 2^-F (round / floor / ceil); also the value from the right edge"""
    d = P.PROBE / "s321_gour_untex"
    real = np.frombuffer((d / "out.bin").read_bytes(), "<u2").reshape(600, 800)
    R5 = np.stack([(real >> 10) & 31, (real >> 5) & 31, real & 31], -1).astype(int)
    T = tris(d)
    cfgs = [("exact", 0, "r")] + [(side, F, q) for side in ("left", "right") for F in (2, 4, 6, 8, 10, 12, 16) for q in ("r", "f", "c")]
    res = {c: 0 for c in cfgs}
    n = 0
    for t in T:
        S = [v[0] for v in t]; C = [v[1] for v in t]
        ar = edge(*S[0], *S[1], *S[2])
        if ar < 0:
            S = [S[0], S[2], S[1]]; C = [C[0], C[2], C[1]]; ar = -ar
        X = np.arange(int(min(s[0] for s in S)), int(max(s[0] for s in S)) + 1)
        es = [(S[i], S[(i + 1) % 3], C[i], C[(i + 1) % 3]) for i in range(3)]
        for y in range(int(min(s[1] for s in S)), int(max(s[1] for s in S)) + 1):
            px, py = X + 0.5, y + 0.5
            w = [edge(*S[1], *S[2], px, py), edge(*S[2], *S[0], px, py), edge(*S[0], *S[1], px, py)]
            inner = (w[0] > 0) & (w[1] > 0) & (w[2] > 0)
            if not inner.any():
                continue
            # the edges crossing this row
            cross = []
            for (pa, pb, ca, cb) in es:
                if (pa[1] - py) * (pb[1] - py) < 0:
                    u = (py - pa[1]) / (pb[1] - pa[1])
                    cross.append((pa[0] + u * (pb[0] - pa[0]), [ca[ch] + u * (cb[ch] - ca[ch]) for ch in range(3)]))
            if len(cross) != 2:
                continue
            cross.sort(key=lambda e: e[0])
            k = (K[y & 3][X & 3] + 0.5) / 16
            for cf in cfgs:
                side, F, q = cf
                ok = np.ones(len(X), bool)
                xe, ve = cross[0] if side != "right" else cross[1]
                sc = 2.0 ** F
                xq = xe if side == "exact" else (np.floor(xe * sc + 0.5) / sc if q == "r" else np.floor(xe * sc) / sc if q == "f" else np.ceil(xe * sc) / sc)
                for ch in range(3):
                    dvdx = ((C[1][ch] - C[0][ch]) * (S[2][1] - S[0][1]) - (C[2][ch] - C[0][ch]) * (S[1][1] - S[0][1])) / ar
                    v = ve[ch] + (px - xq) * dvdx
                    ok &= np.floor(v * 31 + k).astype(int) == R5[y, X, ch]
                res[cf] += int((~ok & inner).sum())
            n += int(inner.sum())
    for cf, b in sorted(res.items(), key=lambda kv: kv[1])[:10]:
        print(f"  {cf[0]:5s} F {cf[1]:2d} {cf[2]}: wrong px {b:6d} of {n}")


if __name__ == "__main__" and len(sys.argv) > 1 and sys.argv[1] == "edge":
    main_edge()


def fixed_plane(probe, S=31.0, F=16, ydir="away", anchor="maxy_last", xq="floor", yq="floor"):
    """the colour scaled by S, its x / y gradients held as multiples of 2^-F (xq / yq: floor, round, trunc) in the direction the
    walk goes from the anchor (the vertex of largest y; ties: last or first), the anchor's value exact:
    v = va + Gx * (px - xa) + Gy * (py - ya); returns wrong / inner px"""
    d = P.PROBE / probe
    real = np.frombuffer((d / "out.bin").read_bytes(), "<u2").reshape(600, 800)
    R5 = np.stack([(real >> 10) & 31, (real >> 5) & 31, real & 31], -1).astype(int)
    T = tris(d)
    sc = S * 2.0 ** F
    Qf = {"floor": np.floor, "round": lambda z: np.floor(z + 0.5), "trunc": np.trunc}
    bad = n = 0
    for t in T:
        Sv = [v[0] for v in t]; C = [v[1] for v in t]
        if anchor == "maxy_last":
            a = max(range(3), key=lambda i: (Sv[i][1], i))
        else:
            a = max(range(3), key=lambda i: (Sv[i][1], -i))
        ar = edge(*Sv[0], *Sv[1], *Sv[2])
        xs_ = [s[0] for s in Sv]; ys_ = [s[1] for s in Sv]
        X = np.arange(int(min(xs_)), int(max(xs_)) + 1)
        # the walk from the anchor: rows go towards the other vertices' y (smaller y), so the per-row step is -dv/dy
        G = []
        for ch in range(3):
            c = [C[i][ch] for i in range(3)]
            gxv = ((c[1] - c[0]) * (Sv[2][1] - Sv[0][1]) - (c[2] - c[0]) * (Sv[1][1] - Sv[0][1])) / ar
            gyv = ((c[2] - c[0]) * (Sv[1][0] - Sv[0][0]) - (c[1] - c[0]) * (Sv[2][0] - Sv[0][0])) / ar
            Gx = Qf[xq](gxv * sc) / sc
            if ydir == "away":
                Gy = -Qf[yq](-gyv * sc) / sc
            else:
                Gy = Qf[yq](gyv * sc) / sc
            G.append((c[a], Gx, Gy))
        for y in range(int(min(ys_)), int(max(ys_)) + 1):
            px, py = X + 0.5, y + 0.5
            w0 = edge(*Sv[1], *Sv[2], px, py); w1 = edge(*Sv[2], *Sv[0], px, py); w2 = edge(*Sv[0], *Sv[1], px, py)
            if ar < 0:
                w0, w1, w2 = -w0, -w1, -w2
            inner = (w0 > 0) & (w1 > 0) & (w2 > 0)
            if not inner.any():
                continue
            k = (K[y & 3][X & 3] + 0.5) / 16
            ok = np.ones(len(X), bool)
            for ch in range(3):
                va, Gx, Gy = G[ch]
                v = va + Gx * (px - Sv[a][0]) + Gy * (py - Sv[a][1])
                ok &= np.floor(v * 31 + k).astype(int) == R5[y, X, ch]
            bad += int((~ok & inner).sum()); n += int(inner.sum())
    return bad, n


if __name__ == "__main__" and len(sys.argv) > 1 and sys.argv[1] == "fixed":
    for probe in sys.argv[2:] or ["s321_ramp_dense", "s321_gour_untex"]:
        print(probe, "exact plane (F 60):", fixed_plane(probe, F=60))
        for F in (14, 15, 16, 17, 18):
            for ydir in ("away", "plain"):
                for xq in ("floor", "round", "trunc"):
                    print(f"  S 31 F {F} ydir {ydir} xq/yq {xq}: {fixed_plane(probe, F=F, ydir=ydir, xq=xq, yq=xq)}", flush=True)


def edgewalk(probe, S=31.0, F=16, q="floor", first="exact", xstep_q=True, step="joint", collect=None, grad="d", walk="up"):
    """incremental stepping: rows walk from the anchor (largest y) towards smaller y; each row's first covered pixel (left end) takes
    the previous row's start value plus a held step Q(-gy + gx * (ix - ix_prev)) (two steps precomputed: for the edge's floor and
    floor + 1 x steps), the first row's start is exact; along the span each pixel adds Q(gx). Q holds a value as a multiple of
    2^-F of the colour scaled by S (floor / round / trunc)."""
    d = P.PROBE / probe
    real = np.frombuffer((d / "out.bin").read_bytes(), "<u2").reshape(600, 800)
    R5 = np.stack([(real >> 10) & 31, (real >> 5) & 31, real & 31], -1).astype(int)
    T = tris(d)
    sc = S * 2.0 ** F
    Qf = {"floor": math.floor, "round": lambda z: math.floor(z + 0.5), "trunc": math.trunc}[q]
    Q = lambda v: Qf(v * sc) / sc
    bad = n = 0
    for t in T:
        Sv = [v[0] for v in t]; C = [v[1] for v in t]
        ar = edge(*Sv[0], *Sv[1], *Sv[2])
        xs_ = [s[0] for s in Sv]; ys_ = [s[1] for s in Sv]
        X = np.arange(int(min(xs_)), int(max(xs_)) + 1)
        gr = []
        for ch in range(3):
            c = [C[i][ch] for i in range(3)]
            if grad == "d":
                gxv = ((c[1] - c[0]) * (Sv[2][1] - Sv[0][1]) - (c[2] - c[0]) * (Sv[1][1] - Sv[0][1])) / ar
                gyv = ((c[2] - c[0]) * (Sv[1][0] - Sv[0][0]) - (c[1] - c[0]) * (Sv[2][0] - Sv[0][0])) / ar
            else:
                # float32, deltas from vertex `a` (grad = "f<a>"), colours scaled by S in float
                a = int(grad[1]) if grad[1:].isdigit() else max(range(3), key=lambda i: (Sv[i][1], i))
                o = [i for i in range(3) if i != a]
                A = [f32(v) for v in Sv[a]]; B = [f32(v) for v in Sv[o[0]]]; Cc = [f32(v) for v in Sv[o[1]]]
                dx1, dy1, dx2, dy2 = f32(B[0] - A[0]), f32(B[1] - A[1]), f32(Cc[0] - A[0]), f32(Cc[1] - A[1])
                area = f32(f32(dx1 * dy2) - f32(dx2 * dy1))
                ca, cb, cc = f32(f32(c[a]) * f32(S)), f32(f32(c[o[0]]) * f32(S)), f32(f32(c[o[1]]) * f32(S))
                d1, d2 = f32(cb - ca), f32(cc - ca)
                gxs = f32(f32(f32(d1 * dy2) - f32(d2 * dy1)) / area)
                gys = f32(f32(f32(d2 * dx1) - f32(d1 * dx2)) / area)
                gxv, gyv = float(gxs) / S, float(gys) / S
            gr.append((gxv, gyv, c))
        rows = []
        yr = range(int(max(ys_)), int(min(ys_)) - 1, -1) if walk == "up" else range(int(min(ys_)), int(max(ys_)) + 1)
        if walk == "auto":   # from the apex: the single vertex on its own y (a horizontal edge at the other end)
            ysort = sorted(ys_)
            yr = range(int(min(ys_)), int(max(ys_)) + 1) if ysort[0] != ysort[1] and ysort[1] == ysort[2] else                 range(int(max(ys_)), int(min(ys_)) - 1, -1)
        for y in yr:
            px, py = X + 0.5, y + 0.5
            w0 = edge(*Sv[1], *Sv[2], px, py); w1 = edge(*Sv[2], *Sv[0], px, py); w2 = edge(*Sv[0], *Sv[1], px, py)
            if ar < 0:
                w0, w1, w2 = -w0, -w1, -w2
            cov = (w0 >= 0) & (w1 >= 0) & (w2 >= 0)
            inner = (w0 > 0) & (w1 > 0) & (w2 > 0)
            if cov.any():
                rows.append((y, int(X[cov][0]), inner))
        state = None
        # the left edge's floor x step per row (dda variant): from the row starts themselves
        dmin = [None] * 3
        if step == "dda" and len(rows) > 2:
            steps = [rows[i + 1][1] - rows[i][1] for i in range(len(rows) - 1)]
            dmin = [min(steps)] * 3
        for (y, ix, inner) in rows:
            k = (K[y & 3][X & 3] + 0.5) / 16
            ok = np.ones(len(X), bool)
            vals = []
            for ch in range(3):
                gxv, gyv, c = gr[ch]
                exact0 = (edge(*Sv[1], *Sv[2], ix + 0.5, y + 0.5) * c[0] + edge(*Sv[2], *Sv[0], ix + 0.5, y + 0.5) * c[1] +
                          edge(*Sv[0], *Sv[1], ix + 0.5, y + 0.5) * c[2]) / ar
                if state is None:
                    if first == "exact":
                        vs = exact0
                    else:   # from the apex (largest-y vertex) through one held prestep
                        a = max(range(3), key=lambda i: (Sv[i][1], i if first == "prestep_last" else -i))
                        vs = c[a] + Q(-gyv * (Sv[a][1] - (y + 0.5)) + gxv * ((ix + 0.5) - Sv[a][0]))
                else:
                    py_, pix, pv = state
                    di = ix - pix
                    if step == "joint":
                        vs = pv[ch] + Q(-gyv * (py_ - y) + gxv * di)
                    elif step == "sep":
                        vs = pv[ch] + Q(-gyv) * (py_ - y) + Q(gxv) * di
                    else:   # "dda": base step for the edge's floor x step, plus Q(gx) per extra pixel
                        fl = dmin[ch] if dmin[ch] is not None else di
                        vs = pv[ch] + Q(-gyv + gxv * fl) + Q(gxv) * (di - fl)
                vals.append(vs)
                gq = Q(gxv) if xstep_q else gxv
                v = vs + (X - ix) * gq
                ok &= np.floor(v * 31 + k).astype(int) == R5[y, X, ch]
            state = (y, ix, vals)
            if inner.any():
                bad += int((~ok & inner).sum()); n += int(inner.sum())
                if collect is not None:
                    for xx in X[~ok & inner]:
                        collect.append((T.index(t), y, int(xx), ix, rows[0][0] - y, int(xx) - ix))
    return bad, n


if __name__ == "__main__" and len(sys.argv) > 1 and sys.argv[1] == "edgewalk":
    import math as _m  # noqa: F401
    for probe in sys.argv[2:] or ["s321_ramp_dense"]:
        print(probe, "no quantisation (F 50):", edgewalk(probe, F=50), flush=True)
        for S in (31.0, 255.0, 1.0):
            for F in ((14, 15, 16, 17) if S != 1.0 else (20, 21, 22, 23)):
                for q in ("floor", "round", "trunc"):
                    print(f"  S {S:g} F {F} {q}: {edgewalk(probe, S=S, F=F, q=q)}", flush=True)


def edgewalk_dda(probe, S=31.0, F=16, XF=16, xq="trunc"):
    """edgewalk (joint truncated steps) whose per-row x step follows a DDA of the left edge: x_dda(n) = x_e(first row) + n * dxq,
    dxq = the edge's x change per row held to 2^-XF (xq), the row's attribute start at the DDA pixel ceil(x_dda - 0.5), the
    covered pixels from there by the span step."""
    d = P.PROBE / probe
    real = np.frombuffer((d / "out.bin").read_bytes(), "<u2").reshape(600, 800)
    R5 = np.stack([(real >> 10) & 31, (real >> 5) & 31, real & 31], -1).astype(int)
    T = tris(d)
    sc = S * 2.0 ** F
    Q = lambda v: math.trunc(v * sc) / sc
    QX = {"trunc": math.trunc, "floor": math.floor, "round": lambda z: math.floor(z + 0.5)}[xq]
    bad = n = 0
    for t in T:
        Sv = [v[0] for v in t]; C = [v[1] for v in t]
        ar = edge(*Sv[0], *Sv[1], *Sv[2])
        xs_ = [s[0] for s in Sv]; ys_ = [s[1] for s in Sv]
        X = np.arange(int(min(xs_)), int(max(xs_)) + 1)
        gr = []
        for ch in range(3):
            c = [C[i][ch] for i in range(3)]
            gxv = ((c[1] - c[0]) * (Sv[2][1] - Sv[0][1]) - (c[2] - c[0]) * (Sv[1][1] - Sv[0][1])) / ar
            gyv = ((c[2] - c[0]) * (Sv[1][0] - Sv[0][0]) - (c[1] - c[0]) * (Sv[2][0] - Sv[0][0])) / ar
            gr.append((gxv, gyv, c))
        rows = []
        for y in range(int(max(ys_)), int(min(ys_)) - 1, -1):
            px, py = X + 0.5, y + 0.5
            w0 = edge(*Sv[1], *Sv[2], px, py); w1 = edge(*Sv[2], *Sv[0], px, py); w2 = edge(*Sv[0], *Sv[1], px, py)
            if ar < 0:
                w0, w1, w2 = -w0, -w1, -w2
            cov = (w0 >= 0) & (w1 >= 0) & (w2 >= 0)
            inner = (w0 > 0) & (w1 > 0) & (w2 > 0)
            if cov.any():
                rows.append((y, int(X[cov][0]), inner))
        if not rows:
            continue
        # the left edge: the edge whose x at the rows' centres is the covered start (pick the edge closest to the starts)
        es = [(Sv[i], Sv[(i + 1) % 3]) for i in range(3) if Sv[i][1] != Sv[(i + 1) % 3][1]]
        def ex(e, py):
            (ax, ay), (bx, by) = e
            return ax + (py - ay) * (bx - ax) / (by - ay)
        best = min(es, key=lambda e: sum(abs(math.ceil(ex(e, y + 0.5) - 0.5) - ix) for y, ix, _ in rows[1:-1] or rows))
        (ax, ay), (bx, by) = best
        dxdy = (bx - ax) / (by - ay)          # x change per +1 in y
        dxq = -QX(dxdy * 2.0 ** XF) / 2.0 ** XF  # rows walk towards smaller y
        x_d = ex(best, rows[0][0] + 0.5)
        state = None
        for r_i, (y, ix, inner) in enumerate(rows):
            xdda = x_d + r_i * dxq
            idd = math.ceil(xdda - 0.5)
            k = (K[y & 3][X & 3] + 0.5) / 16
            ok = np.ones(len(X), bool)
            vals = []
            for ch in range(3):
                gxv, gyv, c = gr[ch]
                if state is None:
                    vs = (edge(*Sv[1], *Sv[2], idd + 0.5, y + 0.5) * c[0] + edge(*Sv[2], *Sv[0], idd + 0.5, y + 0.5) * c[1] +
                          edge(*Sv[0], *Sv[1], idd + 0.5, y + 0.5) * c[2]) / ar
                else:
                    py_, pid, pv = state
                    vs = pv[ch] + Q(-gyv * (py_ - y) + gxv * (idd - pid))
                vals.append(vs)
                v = vs + (X - idd) * Q(gxv)
                ok &= np.floor(v * 31 + k).astype(int) == R5[y, X, ch]
            state = (y, idd, vals)
            if inner.any():
                bad += int((~ok & inner).sum()); n += int(inner.sum())
    return bad, n


if __name__ == "__main__" and len(sys.argv) > 1 and sys.argv[1] == "dda":
    for XF in (8, 12, 16, 20, 30):
        for xq in ("trunc", "floor", "round"):
            print(f"XF {XF} {xq}: gour_untex {edgewalk_dda('s321_gour_untex', XF=XF, xq=xq)} ramp {edgewalk_dda('s321_ramp_dense', XF=XF, xq=xq)}", flush=True)
