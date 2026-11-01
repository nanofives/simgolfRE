"""S3.21 (U-1392): which vertex arithmetic gives the real GL's snapped window coordinates.

Ground truth: 21_glfeed/glfeed.exe with GLFEED_POINTS=1 (one GL_POINT_TOKEN per vertex the real GL keeps, in order, window x / y on
the 1/256 grid). This script replays the pass's matrix calls and its vertex stream (glArrayElement over vtx.bin, glVertex3fv),
computes each vertex's window position under candidate pipelines, pairs the emitted vertices with the feedback points in order
(a vertex outside the view volume yields no point) and counts, per candidate, the coordinates whose rounding to 1/256 equals the
feedback value.

    ~/bin/py -3.12 log/roadmap/S3/21_vfit.py <corpus> <step> <points.f32>
"""
import math
import pathlib
import struct
import sys

import numpy as np

ROOT = pathlib.Path(__file__).resolve().parents[3]
f32 = np.float32


def fbits(h):
    return struct.unpack("<f", struct.pack("<I", int(h, 16)))[0]


def dbits(lo, hi):
    return struct.unpack("<d", struct.pack("<II", int(lo, 16), int(hi, 16)))[0]


def args(line):
    sp = line.split(" ", 1)
    if len(sp) < 2:
        return sp[0], [], []
    a, v = [], []
    for tok in sp[1].split(" "):
        if tok.startswith("v="):
            v = tok[2:].split(",")
        elif "=" in tok:
            continue
        elif tok:
            a += tok.split(",")
    return sp[0], a, v


def read_points(path):
    b = pathlib.Path(path).read_bytes()
    n = int(struct.unpack_from("<f", b, 0)[0])
    f = np.frombuffer(b, "<f4", count=n, offset=4)
    pts = []
    i = 0
    while i < n:
        tok = int(f[i])
        if tok == 0x0701:
            pts.append(f[i + 1:i + 12].copy())
            i += 12
        elif tok == 0x0700:
            i += 2
        else:
            raise ValueError(tok)
    return np.array(pts)


# ---- matrices, column-major 4x4 as GL keeps them (m[col*4 + row]); `prec` builds them in float32 or double
def ident():
    return np.eye(4)


def mul(a, b, prec):
    """a * b (GL post-multiplication: current = current * b), each element a dot product of 4 terms"""
    r = np.zeros((4, 4))
    for i in range(4):
        for j in range(4):
            if prec == "f32":
                s = f32(0)
                for k in range(4):
                    s = f32(s + f32(f32(a[i, k]) * f32(b[k, j])))
                r[i, j] = s
            elif prec == "f32x":   # products and sums in double (x87 53-bit), stored as float
                r[i, j] = f32(sum(float(f32(a[i, k])) * float(f32(b[k, j])) for k in range(4)))
            else:
                r[i, j] = sum(a[i, k] * b[k, j] for k in range(4))
    return r


def rot(angle, x, y, z, prec):
    n = math.sqrt(x * x + y * y + z * z)
    x, y, z = x / n, y / n, z / n
    a = math.radians(angle)
    if prec in ("f32", "f32x", "f32trig"):
        c, s = float(f32(math.cos(float(f32(a))))), float(f32(math.sin(float(f32(a)))))
    else:
        c, s = math.cos(a), math.sin(a)
    t = 1 - c
    m = np.array([[t * x * x + c, t * x * y - s * z, t * x * z + s * y, 0],
                  [t * x * y + s * z, t * y * y + c, t * y * z - s * x, 0],
                  [t * x * z - s * y, t * y * z + s * x, t * z * z + c, 0],
                  [0, 0, 0, 1]])
    if prec != "f64":
        m = m.astype(np.float32).astype(np.float64)
    return m


def trans(x, y, z):
    m = ident()
    m[0, 3], m[1, 3], m[2, 3] = x, y, z
    return m


def ortho(l, r, b, t, n, f, prec):
    m = np.array([[2 / (r - l), 0, 0, -(r + l) / (r - l)],
                  [0, 2 / (t - b), 0, -(t + b) / (t - b)],
                  [0, 0, -2 / (f - n), -(f + n) / (f - n)],
                  [0, 0, 0, 1]])
    if prec != "f64":
        m = m.astype(np.float32).astype(np.float64)
    return m


def build(corpus, step, prec):
    root = ROOT / "port" / "corpus" / "gl" / corpus
    lines = (root / "stream.gl.txt").read_text().splitlines()
    sd = root / "snaps" / step
    meta = dict(ln.split(" ", 1) for ln in (sd / "meta.txt").read_text().splitlines() if " " in ln)
    sidx = int(meta["stateIdx"])
    ctx = next(i for i, ln in enumerate(lines) if ln.startswith("wglCreateContext"))
    state = {"mode": 0x1700, "mv": ident(), "pr": ident(), "stack_mv": [], "stack_pr": []}

    state["mvp"] = ident()
    state["stack_mvp"] = []

    def post(cur, X):
        """GL post-multiplication of the current matrix; a modelview op is applied to the MVP too (measured, 21_notes.md),
        a projection op recomputes MVP = P * MV"""
        state[cur] = mul(state[cur], X, prec)
        if cur == "mv":
            state["mvp"] = mul(state["mvp"], X, prec)
        else:
            state["mvp"] = mul(state["pr"], state["mv"], prec)

    def apply(name, a):
        cur = "pr" if state["mode"] == 0x1701 else "mv"
        if name == "glMatrixMode":
            state["mode"] = int(a[0], 16)
        elif name == "glLoadIdentity":
            state[cur] = ident()
            state["mvp"] = state["pr"].copy() if cur == "mv" else mul(state["pr"], state["mv"], prec)
        elif name == "glOrtho":
            vals = [dbits(a[2 * i], a[2 * i + 1]) for i in range(6)]
            post(cur, ortho(*vals, prec))
        elif name == "glRotated":
            vals = [dbits(a[2 * i], a[2 * i + 1]) for i in range(4)]
            post(cur, rot(*vals, prec))
        elif name == "glRotatef":
            vals = [fbits(x) for x in a[:4]]
            post(cur, rot(*vals, prec))
        elif name == "glTranslatef":
            vals = [fbits(x) for x in a[:3]]
            post(cur, trans(*vals))
        elif name == "glPushMatrix":
            state["stack_" + cur].append(state[cur].copy())
            if cur == "mv":
                state["stack_mvp"].append(state["mvp"].copy())
        elif name == "glPopMatrix":
            state[cur] = state["stack_" + cur].pop()
            state["mvp"] = state["stack_mvp"].pop() if cur == "mv" else mul(state["pr"], state["mv"], prec)
        elif name == "glViewport":
            state["vp"] = [int(x, 16) for x in a[:4]]

    inb = False
    for ln in lines[ctx + 1:sidx]:
        n, a, v = args(ln)
        if n == "glBegin": inb = True
        elif n == "glEnd": inb = False
        elif not inb: apply(n, a)
    vtx = np.frombuffer((sd / "vtx.bin").read_bytes(), "<f4").reshape(-1, 3)
    out = []   # (object xyz, mv, pr)
    for ln in (sd / "pass.gl.txt").read_text().splitlines():
        n, a, v = args(ln)
        if n == "glArrayElement":
            out.append((vtx[int(a[0], 16)].astype(np.float64), state["mv"], state["pr"], state["mvp"]))
        elif n == "glVertex3fv":
            out.append((np.array([fbits(x) for x in v[:3]]), state["mv"], state["pr"], state["mvp"]))
        elif n == "glVertex3f":
            out.append((np.array([fbits(x) for x in a[:3]]), state["mv"], state["pr"], state["mvp"]))
        elif n in ("glBegin", "glEnd", "glBindTexture", "glTexCoord2fv", "glTexCoord2f", "glNormal3fv", "glFlush", "glEnable",
                   "glDisable", "glBlendFunc", "glColor4f", "glLineWidth"):
            continue
        else:
            apply(n, a)
    return out, state.get("vp", [0, 0, 800, 600])


def clip_coords(o, mv, pr, mode):
    """clip-space x, y, z, w of object point o under `mode`"""
    if mode == "f64":
        e = mv @ np.append(o, 1.0)
        return pr @ e
    if mode == "f32sep":   # eye then clip, every op in float32
        def xf(m, p):
            r = []
            for i in range(4):
                s = f32(0)
                for k in range(4):
                    s = f32(s + f32(f32(m[i, k]) * f32(p[k])))
                r.append(s)
            return np.array(r, dtype=np.float64)
        return xf(pr, xf(mv, np.append(o, 1.0)))
    if mode == "f32sepx":  # eye then clip, products and sums in double, each result stored as float
        def xf(m, p):
            return np.array([float(f32(sum(float(f32(m[i, k])) * float(f32(p[k])) for k in range(4)))) for i in range(4)])
        return xf(pr, xf(mv, np.append(o, 1.0)))
    if mode in ("f32mvp", "f32mvpx", "f32mvpx_order2"):   # combined matrix P * MV in float32 (x: double accumulate)
        mvp = mul(pr, mv, "f32x" if mode != "f32mvp" else "f32")
        p = np.append(o, 1.0)
        if mode == "f32mvp":
            r = []
            for i in range(4):
                s = f32(0)
                for k in range(4):
                    s = f32(s + f32(f32(mvp[i, k]) * f32(p[k])))
                r.append(float(s))
            return np.array(r)
        if mode == "f32mvpx_order2":   # x*m0 + y*m4 + z*m8 + m12 summed left to right, then stored
            return np.array([float(f32(((float(f32(mvp[i, 0])) * p[0] + float(f32(mvp[i, 1])) * p[1]) + float(f32(mvp[i, 2])) * p[2])
                                       + float(f32(mvp[i, 3])))) for i in range(4)])
        return np.array([float(f32(sum(float(f32(mvp[i, k])) * float(f32(p[k])) for k in range(4)))) for i in range(4)])
    if mode in ("sep_d", "mvp_d", "mvpx_d"):   # the transform adds the translation first: m3 + x*m0 + y*m1 + z*m2, each op float32
        def xfd(m, p):
            r = []
            for i in range(4):
                s = f32(m[i, 3]) if p[3] == 1.0 else f32(f32(m[i, 3]) * f32(p[3]))
                for k in range(3):
                    s = f32(s + f32(f32(m[i, k]) * f32(p[k])))
                r.append(float(s))
            return np.array(r)
        if mode == "sep_d":
            return xfd(pr, xfd(mv, np.append(o, 1.0)))
        mvp = mul(pr, mv, "f32" if mode == "mvp_d" else "f32x")
        return xfd(mvp, np.append(o, 1.0))
    raise ValueError(mode)


def window(c, vp, wmode):
    x0, y0, w, h = vp
    hx, hy = w / 2.0, h / 2.0
    sx, sy = hx - 1 / 256, hy - 1 / 256
    cx, cy = x0 + hx, y0 + hy
    nx, ny = c[0] / c[3], c[1] / c[3]
    if wmode == "f64":
        return cx + nx * sx, cy + ny * sy
    if wmode == "f32":   # ndc, product and sum each rounded to float
        nx, ny = float(f32(nx)), float(f32(ny))
        return float(f32(f32(f32(nx) * f32(sx)) + f32(cx))), float(f32(f32(f32(ny) * f32(sy)) + f32(cy)))
    if wmode == "f32x":  # one double expression, stored as float
        return float(f32(cx + nx * sx)), float(f32(cy + ny * sy))
    if wmode == "f32prod":   # product rounded to float, sum exact (biased add rounds directly to 1/256)
        return float(f32(f32(nx) * f32(sx))) + cx, float(f32(f32(ny) * f32(sy))) + cy
    raise ValueError(wmode)


def snap(v, rmode):
    f = v * 256.0
    if rmode == "even":
        return np.round(f) / 256.0
    if rmode == "up":
        return math.floor(f + 0.5) / 256.0
    if rmode == "trunc":
        return math.floor(f) / 256.0
    raise ValueError(rmode)


CMS = ("f64", "f32sep", "f32sepx", "f32mvp", "f32mvpx", "f32mvpx_order2", "sep_d", "mvp_d", "mvpx_d")
WMS = ("f64", "f32", "f32x", "f32prod")


def main():
    corpus, step, ptsf = sys.argv[1:4]
    pts = read_points(ptsf)
    print("feedback points", len(pts))
    results = []
    for prec in ("f64", "f32", "f32x", "f32trig"):
        verts, vp = build(corpus, step, prec)
        for cm in CMS:
            if prec == "f64" and cm != "f64":
                continue
            cl = [clip_coords(o, mv, pr, cm) for o, mv, pr in verts]
            for wm in WMS:
                # pair in order: a vertex inside the view volume gives one point
                win = [window(c, vp, wm) for c in cl]
                inside = [abs(c[0]) <= c[3] and abs(c[1]) <= c[3] and abs(c[2]) <= c[3] for c in cl]
                k = 0
                pairs = []
                for (wx, wy), ins in zip(win, inside):
                    if not ins:
                        continue
                    if k >= len(pts):
                        break
                    pairs.append((wx, wy, float(pts[k][0]), float(pts[k][1])))
                    k += 1
                pairs = np.array(pairs)
                if abs(len(pairs) - len(pts)) > 0 or np.abs(pairs[:, 0] - pairs[:, 2]).max() > 0.01:
                    results.append((prec, cm, wm, "PAIRING FAILED", len(pairs), len(pts)))
                    continue
                for rm in ("even", "up"):
                    sx = np.array([snap(v, rm) for v in pairs[:, 0]]); sy = np.array([snap(v, rm) for v in pairs[:, 1]])
                    bx = int((sx != pairs[:, 2]).sum()); by = int((sy != pairs[:, 3]).sum())
                    results.append((prec, cm, wm, rm, bx, by))
                    print(f"  matrices {prec:7s} transform {cm:15s} window {wm:7s} round {rm:5s}: x wrong {bx:5d}  y wrong {by:5d}  of {len(pairs)}", flush=True)
    return results


if __name__ == "__main__" and len(sys.argv) == 4:
    main()


def dump(corpus, step, ptsf, prec, cm, wm):
    pts = read_points(ptsf)
    verts, vp = build(corpus, step, prec)
    cl = [clip_coords(o, mv, pr, cm) for o, mv, pr in verts]
    k = 0
    rows = []
    for (o, mv, pr), c in zip(verts, cl):
        if not (abs(c[0]) <= c[3] and abs(c[1]) <= c[3] and abs(c[2]) <= c[3]):
            continue
        wx, wy = window(c, vp, wm)
        fx, fy = float(pts[k][0]), float(pts[k][1])
        k += 1
        cd = clip_coords(o, mv, pr, "f64")
        dx, dy = window(cd, vp, "f64")
        rows.append((o, c, wx, wy, fx, fy, dx, dy))
    import collections
    bad = [r for r in rows if snap(r[2], "even") != r[4]]
    print(len(bad), "wrong x of", len(rows))
    seen = collections.Counter()
    for o, c, wx, wy, fx, fy, dx, dy in bad:
        key = (round(fx, 4), tuple(o))
        if seen[key]:
            continue
        seen[key] += 1
        print(f"  obj {o.tolist()} ndc {c[0]:.9f} model x {wx:.7f} (256x frac {wx*256 % 1:.4f}) dbl {dx:.7f} ({dx*256 % 1:.4f}) feedback {fx:.7f} -> {fx*256:.1f}")
        if len(seen) > 40:
            break


if __name__ == "__main__" and len(sys.argv) > 4 and sys.argv[4] == "dump":
    dump(*sys.argv[1:4], *sys.argv[5:8])
