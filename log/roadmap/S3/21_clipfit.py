"""S3.21 (U-1392): the real GL's frustum clipping, fitted against its triangle feedback (21_glfeed/glfeed.exe, GL_TRIANGLES output).

The pass's vertex stream (21_vfit.build's matrices, texture coordinates from glTexCoord2f(v)) goes through the measured vertex
rule (21_vcheck.py); triangles of GL_TRIANGLES and GL_TRIANGLE_FAN blocks are culled (glFrontFace GL_CW, back faces culled) and
clipped by a candidate clipper; the output triangles are compared, in order, with the feedback polygons.

    ~/bin/py -3.12 log/roadmap/S3/21_clipfit.py <corpus> <step> [variant]
"""
import importlib.util
import os
import pathlib
import struct
import subprocess
import sys

import numpy as np

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parents[2]
_s = importlib.util.spec_from_file_location("v21", HERE / "21_vfit.py")
V = importlib.util.module_from_spec(_s)
_s.loader.exec_module(V)
f32 = np.float32
EXE = HERE / "21_glfeed" / "glfeed.exe"
OUT = pathlib.Path(os.environ.get("S321_SCRATCH", HERE / "21_probe_out")) / "clip"


def read_polys(path):
    b = pathlib.Path(path).read_bytes()
    n = int(struct.unpack_from("<f", b, 0)[0])
    f = np.frombuffer(b, "<f4", count=n, offset=4)
    polys, i = [], 0
    while i < n:
        tok = int(f[i])
        if tok == 0x0703:
            k = int(f[i + 1]); polys.append(f[i + 2:i + 2 + 11 * k].reshape(k, 11).copy()); i += 2 + 11 * k
        elif tok == 0x0700:
            i += 2
        else:
            raise ValueError(tok)
    return polys


def stream(corpus, step):
    """[(mode, [(obj xyz float32, s, t, mvp)])] for the pass's draw blocks"""
    verts, vp = V.build(corpus, step, "f32")
    sd = ROOT / "port" / "corpus" / "gl" / corpus / "snaps" / step
    prims = []
    k = 0
    cs, ct = f32(0), f32(0)
    cur = None
    for ln in (sd / "pass.gl.txt").read_text().splitlines():
        n, a, v = V.args(ln)
        if n == "glBegin":
            cur = (int(a[0], 16), []); prims.append(cur)
        elif n == "glTexCoord2fv":
            cs, ct = f32(V.fbits(v[0])), f32(V.fbits(v[1]))
        elif n == "glTexCoord2f":
            cs, ct = f32(V.fbits(a[0])), f32(V.fbits(a[1]))
        elif n in ("glArrayElement", "glVertex3fv", "glVertex3f"):
            o, mv, pr, mvp = verts[k]; k += 1
            cur[1].append((o.astype(np.float32), cs, ct, mvp))
    return prims, vp


def clip_of(o, mvp):
    c = []
    for r in range(4):
        s = f32(mvp[r, 3])
        for k in range(3):
            s = f32(s + f32(o[k] * f32(mvp[r, k])))
        c.append(s)
    return c


def window(c, vp):
    x0, y0, w, h = vp
    hx, hy = w / 2.0, h / 2.0
    nx, ny = c[0], c[1]
    if c[3] != f32(1):
        inv = f32(f32(1) / c[3]); nx, ny = f32(c[0] * inv), f32(c[1] * inv)
    wx = float(f32(nx * f32(hx - 1 / 256))) + x0 + hx
    wy = float(f32(ny * f32(hy - 1 / 256))) + y0 + hy
    return np.round(256 * wx) / 256, np.round(256 * wy) / 256


# frustum planes as (coefficients of x, y, z, w): distance >= 0 inside
PLANES_SI = [(1, 0, 0, 1), (-1, 0, 0, 1), (0, 1, 0, 1), (0, -1, 0, 1), (0, 0, 1, 1), (0, 0, -1, 1)]


def dist(c, p, variant):
    if variant.get("dist") == "seq":
        acc = f32(0)
        for k in range(4):
            if p[k]:
                acc = f32(acc + f32(f32(p[k]) * c[k]))
        return acc
    # w +- coordinate, one float op
    k = next(i for i in range(3) if p[i])
    return f32(c[3] + c[k]) if p[k] > 0 else f32(c[3] - c[k])


def lerp(a, b, t):
    return f32(a + f32(t * f32(b - a)))


def clip_poly(poly, variant):
    """poly: list of dicts with 'c' (4 floats), 's', 't'. Sutherland-Hodgman per plane."""
    planes = variant.get("planes", PLANES_SI)
    for p in planes:
        if not poly:
            return poly
        d = [dist(v["c"], p, variant) for v in poly]
        if all(x >= 0 for x in d):
            continue
        out = []
        n = len(poly)
        if variant.get("order") == "classic":   # for each edge prev -> cur: the intersection when it crosses, then cur when inside
            for i in range(n):
                a, b = poly[i - 1], poly[i]
                da, db = d[i - 1], d[i]
                if (da >= 0) != (db >= 0):
                    if variant.get("from") == "in2out":
                        ii, oo, di, do = (a, b, da, db) if da >= 0 else (b, a, db, da)
                    elif variant.get("from") == "cur2prev":
                        ii, oo, di, do = b, a, db, da
                    else:
                        ii, oo, di, do = a, b, da, db
                    t = f32(di / f32(di - do))
                    out.append({"c": [lerp(ii["c"][k], oo["c"][k], t) for k in range(4)], "s": lerp(ii["s"], oo["s"], t),
                                "t": lerp(ii["t"], oo["t"], t)})
                if db >= 0:
                    out.append(b)
            poly = out
            continue
        for i in range(n):
            a, b = poly[i], poly[(i + 1) % n]
            da, db = d[i], d[(i + 1) % n]
            if da >= 0:
                out.append(a)
            if (da >= 0) != (db >= 0):
                # the new vertex interpolated from the inside vertex towards the outside one (variant "in2out"), or from a to b
                if variant.get("from") == "in2out" and da < 0:
                    ii, oo, di, do = b, a, db, da
                else:
                    ii, oo, di, do = a, b, da, db
                t = f32(di / f32(di - do))
                nv = {"c": [lerp(ii["c"][k], oo["c"][k], t) for k in range(4)], "s": lerp(ii["s"], oo["s"], t), "t": lerp(ii["t"], oo["t"], t)}
                out.append(nv)
        poly = out
    return poly


def predict(corpus, step, variant):
    prims, vp = stream(corpus, step)
    tris = []
    for mode, vs in prims:
        idx = [(i, i + 1, i + 2) for i in range(0, len(vs) - 2, 3)] if mode == 4 else [(0, i, i + 1) for i in range(1, len(vs) - 1)]
        rec = [{"c": clip_of(o, mvp), "s": s, "t": t} for o, s, t, mvp in vs]
        for ia, ib, ic in idx:
            poly = [rec[ia], rec[ib], rec[ic]]
            win = [window(v["c"], vp) for v in poly]
            ar = (win[1][0] - win[0][0]) * (win[2][1] - win[0][1]) - (win[2][0] - win[0][0]) * (win[1][1] - win[0][1])
            # glFrontFace(GL_CW) in window coordinates whose y is the memory row: the original culls back faces
            front = (ar <= 0) if variant.get("zero") == "keep" else (ar < 0)
            if variant.get("cull", True) and not (front if variant.get("front", "neg") == "neg" else ar > 0):
                continue
            inside = all(abs(v["c"][k]) <= v["c"][3] for v in poly for k in range(3))
            if not inside:
                poly = clip_poly(poly, variant)
            if len(poly) < 3:
                continue
            w = [window(v["c"], vp) + (float(v["s"]), float(v["t"])) for v in poly]
            for i in range(1, len(w) - 1):
                tris.append((w[0], w[i], w[i + 1]))
    return tris


def main():
    corpus, step = sys.argv[1], sys.argv[2]
    OUT.mkdir(parents=True, exist_ok=True)
    f = OUT / f"{corpus}_{step}.tri.f32"
    if not f.exists():
        subprocess.run([str(EXE), str(ROOT / "port/corpus/gl" / corpus), step, str(f)], check=True, capture_output=True)
    polys = read_polys(f)
    variants = {
        "si": {},
        "si_in2out": {"from": "in2out"},
        "si_front_pos": {"front": "pos"},
        "classic": {"order": "classic"},
        "classic_in2out": {"order": "classic", "from": "in2out"},
        "classic_in2out_zero": {"order": "classic", "from": "in2out", "zero": "keep"},
        "classic_cur2prev": {"order": "classic", "from": "cur2prev"},
    }
    want = sys.argv[3:] or list(variants)
    for name in want:
        tris = predict(corpus, step, variants[name])
        n = min(len(tris), len(polys))
        pos_bad = st_bad = 0
        first = None
        for k in range(n):
            p = polys[k]
            q = np.array(tris[k])
            pb = not (np.array_equal(p[:, 0].astype(np.float64), q[:, 0]) and np.array_equal(p[:, 1].astype(np.float64), q[:, 1]))
            sb = not (np.array_equal(p[:, 7], q[:, 2].astype(np.float32)) and np.array_equal(p[:, 8], q[:, 3].astype(np.float32)))
            pos_bad += pb; st_bad += sb
            if (pb or sb) and first is None:
                first = k
        print(f"{name:14s} predicted {len(tris)} feedback {len(polys)}  position-different {pos_bad}  texcoord-different {st_bad}  first {first}")
        if first is not None:
            print("   feedback ", np.round(polys[first][:, [0, 1, 7, 8]], 6).tolist())
            print("   predicted", np.round(np.array(tris[first]), 6).tolist())


if __name__ == "__main__":
    main()
