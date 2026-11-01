"""S3.21 (U-1392): synthetic feedback probes of the real software GL's vertex arithmetic.

Writes a corpus-shaped directory (stream.gl.txt with a wglCreateContext line and the state calls, snaps/p/meta.txt, pass.gl.txt with
glVertex3fv points, empty vtx.bin / nrm.bin) under the scratch root, runs 21_glfeed/glfeed.exe on it with GLFEED_POINTS=1 and returns
the snapped window coordinates of the points the GL kept.

    ~/bin/py -3.12 log/roadmap/S3/21_probe.py viewport      # stage 1: identity matrices, points given in NDC
"""
import math
import os
import pathlib
import struct
import subprocess
import sys

import numpy as np

HERE = pathlib.Path(__file__).resolve().parent
EXE = HERE / "21_glfeed" / "glfeed.exe"
SCRATCH = pathlib.Path(os.environ.get("S321_SCRATCH", HERE / "21_probe_out"))
f32 = np.float32


def hf(x):
    return "0x%x" % struct.unpack("<I", struct.pack("<f", float(x)))[0]


def hd(x):
    lo, hi = struct.unpack("<II", struct.pack("<d", float(x)))
    return "0x%x,0x%x" % (lo, hi)


def run(name, state, points, vp=(0, 0, 800, 600)):
    """state: list of stream lines after the context; points: list of (x, y, z) float32 object coordinates"""
    root = SCRATCH / name
    sd = root / "snaps" / "p"
    sd.mkdir(parents=True, exist_ok=True)
    lines = ["wglCreateContext 0", "glViewport 0x%x,0x%x,0x%x,0x%x" % vp] + state
    (root / "stream.gl.txt").write_text("\n".join(lines) + "\n")
    (sd / "meta.txt").write_text(f"w 800\nh 600\nwb 1600\nbpp 16\nbiHeight 600\ncomp 3\nm0 31744\nm1 992\nm2 31\nstateIdx {len(lines)}\n")
    body = ["glBegin 0x0"] + ["glVertex3fv 0x0 v=%s,%s,%s" % (hf(x), hf(y), hf(z)) for x, y, z in points] + ["glEnd "]
    (sd / "pass.gl.txt").write_text("\n".join(body) + "\n")
    (sd / "vtx.bin").write_bytes(b"\0" * 12)
    (sd / "nrm.bin").write_bytes(b"\0" * 12)
    out = root / "pts.f32"
    env = dict(os.environ, GLFEED_POINTS="1")
    r = subprocess.run([str(EXE), str(root), "p", str(out)], env=env, capture_output=True, text=True)
    if r.returncode != 0:
        raise RuntimeError(r.stdout + r.stderr)
    b = out.read_bytes()
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


def ties_ndc(c, h, step=7):
    """float32 NDC values whose double window value c + ndc * (h - 2^-8) lies near a 1/256 rounding tie, with neighbours"""
    out = []
    for m in range(0, int(2 * h * 256), step * 97):
        t = (m + 0.5) / 256.0
        nd = (t - c) / (h - 1 / 256)
        if abs(nd) > 0.9999:
            continue
        v = f32(nd)
        for k in range(12):
            v = np.nextafter(v, f32(-10))
        for k in range(25):
            out.append(float(v))
            v = np.nextafter(v, f32(10))
    return sorted(set(out))


def viewport():
    state = ["glMatrixMode 0x1701", "glLoadIdentity ", "glMatrixMode 0x1700", "glLoadIdentity "]
    xs = ties_ndc(400.0, 400.0)
    ys = ties_ndc(300.0, 300.0)
    n = min(len(xs), len(ys))
    pts = [(xs[i], ys[i % len(ys)], 0.0) for i in range(len(xs))]
    got = run("viewport", state, pts)
    assert len(got) == len(pts), (len(got), len(pts))
    X = np.array([p[0] for p in pts], np.float64); Y = np.array([p[1] for p in pts], np.float64)
    gx = got[:, 0].astype(np.float64); gy = got[:, 1].astype(np.float64)
    np.save(SCRATCH / "viewport.npy", np.stack([X, Y, gx, gy]))
    cands = {
        "double c+n*(h-2^-8), even": (lambda n, c, h: np.round(256 * (c + n * (h - 1 / 256))) / 256),
        "double, half up": (lambda n, c, h: np.floor(256 * (c + n * (h - 1 / 256)) + 0.5) / 256),
        "f32 prod, f32 sum, even": (lambda n, c, h: np.round(256 * (f32(f32(n) * f32(h - 1 / 256)) + f32(c)).astype(np.float64)) / 256),
        "f32 prod, exact sum, even": (lambda n, c, h: np.round(256 * (f32(n) * f32(h - 1 / 256)).astype(np.float64) + 256 * c) / 256),
        "f32 prod, f32 sum, half up": (lambda n, c, h: np.floor(256 * (f32(f32(n) * f32(h - 1 / 256)) + f32(c)).astype(np.float64) + 0.5) / 256),
        "f32 prod, exact sum, half up": (lambda n, c, h: np.floor(256 * (f32(n) * f32(h - 1 / 256)).astype(np.float64) + 256 * c + 0.5) / 256),
    }
    for name, fn in cands.items():
        ex = int((fn(X, 400.0, 400.0) != gx).sum()); ey = int((fn(Y, 300.0, 300.0) != gy).sum())
        print(f"  {name:32s} x wrong {ex:5d} y wrong {ey:5d} of {len(X)}")




def snapw(ndc, c, h):
    """stage 1 (measured): f32 product, exact add, half to even at 1/256"""
    return np.round(256 * (f32(ndc) * f32(h - 1 / 256)).astype(np.float64) + 256 * c) / 256


def ortho_m(l, r, b, t, n, f, mode):
    if mode == "d":   # elements in double, stored as float
        return [f32(2 / (r - l)), f32(-(r + l) / (r - l)), f32(2 / (t - b)), f32(-(t + b) / (t - b))]
    # elements from float operands, each op rounded to float
    L, R, B, T = f32(l), f32(r), f32(b), f32(t)
    dx, dy = f32(R - L), f32(T - B)
    return [f32(f32(2) / dx), f32(f32(-f32(R + L)) / dx), f32(f32(2) / dy), f32(f32(-f32(T + B)) / dy)]


def ortho():
    l, r, b, t, n, f = -884.0, 883.0, 662.0, -663.0, 5000.0, -5000.0
    state = ["glMatrixMode 0x1701", "glLoadIdentity ",
             "glOrtho " + ",".join(hd(v) for v in (l, r, b, t, n, f)), "glMatrixMode 0x1700", "glLoadIdentity "]
    # object x near window ties under the double model
    m00, m30 = 2 / (r - l), -(r + l) / (r - l)
    m11, m31 = 2 / (t - b), -(t + b) / (t - b)
    xs, ys = [], []
    for m in range(1, 800 * 256, 263):
        tw = (m + 0.5) / 256.0
        nd = (tw - 400.0) / (400 - 1 / 256)
        v = f32((nd - m30) / m00)
        for k in range(8):
            v = np.nextafter(v, f32(-1e9))
        for k in range(17):
            xs.append(float(v)); v = np.nextafter(v, f32(1e9))
    for m in range(1, 600 * 256, 263):
        tw = (m + 0.5) / 256.0
        nd = (tw - 300.0) / (300 - 1 / 256)
        v = f32((nd - m31) / m11)
        for k in range(8):
            v = np.nextafter(v, f32(-1e9))
        for k in range(17):
            ys.append(float(v)); v = np.nextafter(v, f32(1e9))
    pts = [(xs[i], ys[i % len(ys)], 0.0) for i in range(len(xs))]
    got = run("ortho", state, pts)
    assert len(got) == len(pts), (len(got), len(pts))
    X = np.array([p[0] for p in pts], np.float32); Y = np.array([p[1] for p in pts], np.float32)
    gx = got[:, 0].astype(np.float64); gy = got[:, 1].astype(np.float64)
    for mm in ("d", "f"):
        a, a3, bb, b3 = ortho_m(l, r, b, t, n, f, mm)
        for tm in ("sse", "x87"):
            if tm == "sse":
                cx = f32(f32(X * a) + a3); cy = f32(f32(Y * bb) + b3)
            else:
                cx = f32(X.astype(np.float64) * float(a) + float(a3)); cy = f32(Y.astype(np.float64) * float(bb) + float(b3))
            ex = int((snapw(cx, 400.0, 400.0) != gx).sum()); ey = int((snapw(cy, 300.0, 300.0) != gy).sum())
            print(f"  ortho elements {mm} transform {tm}: x wrong {ex:5d} y wrong {ey:5d} of {len(X)}")




def rot_elems(angle, ax, ay, az, mode):
    """3x3 rotation (column-vector form, m[row][col]) for glRotate(angle, axis) under `mode`"""
    if mode.startswith("dd"):    # everything in double, elements stored as float
        a = math.radians(angle); c, s = math.cos(a), math.sin(a)
        n = math.sqrt(ax * ax + ay * ay + az * az); x, y, z = ax / n, ay / n, az / n
        t = 1 - c
        m = [[t * x * x + c, t * x * y - s * z, t * x * z + s * y], [t * x * y + s * z, t * y * y + c, t * y * z - s * x],
             [t * x * z - s * y, t * y * z + s * x, t * z * z + c]]
        return [[f32(v) for v in r] for r in m]
    # float: angle in radians as float, sin / cos of it (double libm then cast, = correctly rounded sinf in practice), float elements
    if mode.startswith("fd"):    # radians in double, sin/cos double, cast; element ops in float
        a = math.radians(angle); c, s = f32(math.cos(a)), f32(math.sin(a))
    else:                        # radians as f32(angle) * f32(pi / 180)
        a = f32(f32(angle) * f32(math.pi / 180)); c, s = f32(math.cos(float(a))), f32(math.sin(float(a)))
    x, y, z = f32(ax), f32(ay), f32(az)
    n = f32(math.sqrt(float(f32(f32(f32(x * x) + f32(y * y)) + f32(z * z)))))
    if n != f32(1):
        x, y, z = f32(x / n), f32(y / n), f32(z / n)
    t = f32(f32(1) - c)
    m = [[f32(f32(f32(t * x) * x) + c), f32(f32(f32(t * x) * y) - f32(s * z)), f32(f32(f32(t * x) * z) + f32(s * y))],
         [f32(f32(f32(t * x) * y) + f32(s * z)), f32(f32(f32(t * y) * y) + c), f32(f32(f32(t * y) * z) - f32(s * x))],
         [f32(f32(f32(t * x) * z) - f32(s * y)), f32(f32(f32(t * y) * z) + f32(s * x)), f32(f32(f32(t * z) * z) + c)]]
    if mode.endswith("axis"):    # an axis-aligned rotation stored as the exact pattern (1 on the axis, 0 off it)
        k = [abs(ax), abs(ay), abs(az)].index(max(abs(ax), abs(ay), abs(az)))
        for i in range(3):
            m[i][k] = f32(1.0 if i == k else 0.0); m[k][i] = f32(1.0 if i == k else 0.0)
        # the 2x2 block keeps c / +-s
        o = [i for i in range(3) if i != k]
        sgn = 1 if k != 1 else -1
        m[o[0]][o[0]] = c; m[o[1]][o[1]] = c; m[o[0]][o[1]] = f32(-sgn * s); m[o[1]][o[0]] = f32(sgn * s)
    return m


def xf_sse(m, X, Y, Z, W=None):
    """rows of m (3 or 4 columns used) applied to points, x*m0 + y*m1 + z*m2 (+ w*m3), each op in float32"""
    out = []
    for r in m:
        acc = f32(X * r[0])
        acc = f32(acc + f32(Y * r[1]))
        acc = f32(acc + f32(Z * r[2]))
        if len(r) > 3:
            acc = f32(acc + (r[3] if W is None else f32(W * r[3])))
        out.append(acc)
    return out


def rotation(spec="f", angle=45.0, axis=(0.0, 1.0, 0.0), cmd="glRotatef", nm="rot"):
    state = ["glMatrixMode 0x1701", "glLoadIdentity ", "glMatrixMode 0x1700", "glLoadIdentity "]
    if cmd == "glRotatef":
        state.append("glRotatef " + ",".join(hf(v) for v in (angle,) + tuple(axis)))
    else:
        state.append("glRotated " + ",".join(hd(v) for v in (angle,) + tuple(axis)))
    rng = np.random.default_rng(321)
    P = rng.uniform(-0.55, 0.55, size=(400000, 3)).astype(np.float32)
    md = rot_elems(angle, *axis, "dd")
    cx = sum(P[:, k].astype(np.float64) * float(md[0][k]) for k in range(3))
    cy = sum(P[:, k].astype(np.float64) * float(md[1][k]) for k in range(3))
    fx = 256 * (400 + cx * (400 - 1 / 256)); fy = 256 * (300 + cy * (300 - 1 / 256))
    near = (np.abs(fx - np.floor(fx) - 0.5) < 0.02) | (np.abs(fy - np.floor(fy) - 0.5) < 0.02)
    P = P[near][:12000]
    got = run(nm, state, [tuple(map(float, p)) for p in P])
    assert len(got) == len(P), (len(got), len(P))
    gx = got[:, 0].astype(np.float64); gy = got[:, 1].astype(np.float64)
    X, Y, Z = P[:, 0], P[:, 1], P[:, 2]
    for mode in ("dd", "fd", "ff", "fdaxis", "ffaxis", "ddaxis"):
        m = rot_elems(angle, *axis, mode)
        ox, oy, oz = xf_sse(m, X, Y, Z)
        ex = int((snapw(ox, 400.0, 400.0) != gx).sum()); ey = int((snapw(oy, 300.0, 300.0) != gy).sum())
        print(f"  {cmd} {angle} {axis} elements {mode:7s}: x wrong {ex:5d} y wrong {ey:5d} of {len(P)}   m {[[float(v) for v in r] for r in m]}")


def rotations():
    rotation()
    rotation("f", 38.68528366088867, (1.0, 0.0, 0.0), "glRotated", "rotx")




ORDERS = {
    "l2r ((a+b)+c)+d": lambda a, b, c, d: f32(f32(f32(a + b) + c) + d),
    "d first ((d+a)+b)+c": lambda a, b, c, d: f32(f32(f32(d + a) + b) + c),
    "pairs (a+b)+(c+d)": lambda a, b, c, d: f32(f32(a + b) + f32(c + d)),
    "r2l a+(b+(c+d))": lambda a, b, c, d: f32(a + f32(b + f32(c + d))),
    "(a+(b+c))+d": lambda a, b, c, d: f32(f32(a + f32(b + c)) + d),
    "x87 exact-ish": lambda a, b, c, d: f32(a.astype(np.float64) + b + c + d),
}


def transform():
    ang, ax = 37.0, (0.3, 0.7, 0.5)
    tx, ty, tz = 0.11, -0.07, 0.05
    state = ["glMatrixMode 0x1701", "glLoadIdentity ", "glMatrixMode 0x1700", "glLoadIdentity ",
             "glRotatef " + ",".join(hf(v) for v in (ang,) + ax), "glTranslatef " + ",".join(hf(v) for v in (tx, ty, tz))]
    R = rot_elems(ang, *ax, "ff")
    Rd = rot_elems(ang, *ax, "dd")
    rng = np.random.default_rng(7)
    P = rng.uniform(-0.45, 0.45, size=(600000, 3)).astype(np.float32)
    cx = sum(P[:, k].astype(np.float64) * float(R[0][k]) for k in range(3)) + sum(float(R[0][k]) * t for k, t in enumerate((tx, ty, tz)))
    cy = sum(P[:, k].astype(np.float64) * float(R[1][k]) for k in range(3)) + sum(float(R[1][k]) * t for k, t in enumerate((tx, ty, tz)))
    fx = 256 * (400 + cx * (400 - 1 / 256)); fy = 256 * (300 + cy * (300 - 1 / 256))
    near = (np.abs(fx - np.floor(fx) - 0.5) < 0.01) | (np.abs(fy - np.floor(fy) - 0.5) < 0.01)
    P = P[near][:14000]
    got = run("transform", state, [tuple(map(float, p)) for p in P])
    assert len(got) == len(P), (len(got), len(P))
    gx = got[:, 0].astype(np.float64); gy = got[:, 1].astype(np.float64)
    X, Y, Z = P[:, 0], P[:, 1], P[:, 2]
    T = [f32(tx), f32(ty), f32(tz)]
    res = []
    for rname, RR in (("ff", R), ("dd", Rd)):
        for tname, tord in ORDERS.items():   # column 3 of R * T: R[i][0]*tx + R[i][1]*ty + R[i][2]*tz + 0 (the old m3 = 0)
            m3 = [tord(f32(RR[i][0] * T[0]), f32(RR[i][1] * T[1]), f32(RR[i][2] * T[2]), f32(0)) for i in range(2)]
            for oname, o in ORDERS.items():
                ox = o(f32(X * RR[0][0]), f32(Y * RR[0][1]), f32(Z * RR[0][2]), m3[0])
                oy = o(f32(X * RR[1][0]), f32(Y * RR[1][1]), f32(Z * RR[1][2]), m3[1])
                ex = int((snapw(ox, 400.0, 400.0) != gx).sum()); ey = int((snapw(oy, 300.0, 300.0) != gy).sum())
                res.append((ex + ey, ex, ey, rname, tname, oname))
    for tot, ex, ey, rname, tname, oname in sorted(res)[:12]:
        print(f"  R {rname} translate {tname:22s} transform {oname:22s}: x wrong {ex:5d} y wrong {ey:5d} of {len(P)}")




def ulpn(v, k):
    v = f32(v)
    for _ in range(abs(k)):
        v = np.nextafter(v, f32(1e9) if k > 0 else f32(-1e9))
    return v


def transform_fit():
    ang, ax = 37.0, (0.3, 0.7, 0.5)
    tx, ty, tz = 0.11, -0.07, 0.05
    root = SCRATCH / "transform" / "snaps" / "p"
    import re
    pts = []
    for ln in (root / "pass.gl.txt").read_text().splitlines():
        if ln.startswith("glVertex3fv"):
            v = ln.split("v=")[1].split(",")
            pts.append([struct.unpack("<f", struct.pack("<I", int(h, 16)))[0] for h in v])
    P = np.array(pts, np.float32)
    b = (SCRATCH / "transform" / "pts.f32").read_bytes()
    n = int(struct.unpack_from("<f", b, 0)[0])
    f = np.frombuffer(b, "<f4", count=n, offset=4).reshape(-1, 12)
    gx = f[:, 1].astype(np.float64); gy = f[:, 2].astype(np.float64)
    R = rot_elems(ang, *ax, "ff")
    T = [f32(tx), f32(ty), f32(tz)]
    X, Y, Z = P[:, 0], P[:, 1], P[:, 2]
    o = ORDERS["d first ((d+a)+b)+c"]
    best = []
    for row, g, c, h in ((0, gx, 400.0, 400.0), (1, gy, 300.0, 300.0)):
        res = []
        for k0 in range(-3, 4):
            for k1 in range(-3, 4):
                for k2 in range(-3, 4):
                    r = [ulpn(R[row][0], k0), ulpn(R[row][1], k1), ulpn(R[row][2], k2)]
                    for tname, tord in ORDERS.items():
                        m3 = tord(f32(r[0] * T[0]), f32(r[1] * T[1]), f32(r[2] * T[2]), f32(0))
                        for k3 in range(-2, 3):
                            mm3 = ulpn(m3, k3)
                            v = o(f32(X * r[0]), f32(Y * r[1]), f32(Z * r[2]), mm3)
                            e = int((snapw(v, c, h) != g).sum())
                            res.append((e, (k0, k1, k2), tname, k3))
        res.sort()
        print("row", row, "best:", res[:6])


if __name__ == "__main__":
    globals()[sys.argv[1]]()
