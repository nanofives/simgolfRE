"""S3.21: candidate arithmetic for the MVP translation column against values fitted from feedback (21_mvpfit.py).

For a pass, the fitted MVP[r][3] (r = 0, 1) is the float32 the brute force found; every candidate builds MV and MVP from the same
calls and reports its own MVP[r][3] in ulps from the fitted value.
    ~/bin/py -3.12 log/roadmap/S3/21_transfit.py   (passes and fitted ulp offsets are listed in FITS)
"""
import importlib.util
import math
import pathlib
import struct

import numpy as np

HERE = pathlib.Path(__file__).resolve().parent
_s = importlib.util.spec_from_file_location("v21", HERE / "21_vfit.py")
V = importlib.util.module_from_spec(_s)
_s.loader.exec_module(V)
f32 = np.float32


def ulps(a, b):
    ia = struct.unpack("<i", struct.pack("<f", float(a)))[0]; ib = struct.unpack("<i", struct.pack("<f", float(b)))[0]
    return ia - ib


def pass_args(corpus, step):
    root = V.ROOT / "port" / "corpus" / "gl" / corpus / "snaps" / step
    rd = rf = tr = None
    for ln in (root / "pass.gl.txt").read_text().splitlines():
        n, a, v = V.args(ln)
        if n == "glRotated": rd = [V.dbits(a[2 * i], a[2 * i + 1]) for i in range(4)]
        elif n == "glRotatef": rf = [V.fbits(x) for x in a[:4]]
        elif n == "glTranslatef": tr = [V.fbits(x) for x in a[:3]]
    return rd, rf, tr


def cands(corpus, step):
    verts, vp = V.build(corpus, step, "f32")
    pr = verts[0][2]
    rd, rf, tr = pass_args(corpus, step)
    R = V.mul(V.rot(*rd, "f32"), V.rot(*rf, "f32"), "f32")   # the 3x3 part (exact: single products)
    P00, P03, P11, P13 = (f32(pr[0, 0]), f32(pr[0, 3]), f32(pr[1, 1]), f32(pr[1, 3]))
    tx, ty, tz = (f32(t) for t in tr)
    out = {}
    for r, (Pd, Pt) in ((0, (P00, P03)), (1, (P11, P13))):
        m = [f32(R[r, k]) for k in range(3)]
        a, b, c = f32(m[0] * tx), f32(m[1] * ty), f32(m[2] * tz)
        sse = f32(f32(f32(a + b) + c) + f32(0))
        d = {}
        d["sse mv, sse mvp"] = f32(f32(Pd * sse) + Pt)
        x87mv = f32(float(m[0]) * float(tx) + float(m[1]) * float(ty) + float(m[2]) * float(tz))
        d["x87 mv, sse mvp"] = f32(f32(Pd * x87mv) + Pt)
        d["x87 mv, x87 mvp"] = f32(float(Pd) * float(x87mv) + float(Pt))
        d["sse mv, x87 mvp"] = f32(float(Pd) * float(sse) + float(Pt))
        mvd = float(m[0]) * float(tx) + float(m[1]) * float(ty) + float(m[2]) * float(tz)
        d["double mv kept, x87 mvp"] = f32(float(Pd) * mvd + float(Pt))
        # translate as T * M ? (pre-multiplication: translation in eye space, not object space) not the GL order; skip
        # MVP built by applying the translation to P*R directly: MVP3 = (P*R) row . t + P3
        pr0 = [f32(Pd * m[k]) for k in range(3)]
        d["mvp = (P*R)*T sse"] = f32(f32(f32(f32(pr0[0] * tx) + f32(pr0[1] * ty)) + f32(pr0[2] * tz)) + Pt)
        d["mvp = (P*R)*T sse, t3 first"] = f32(f32(f32(Pt + f32(pr0[0] * tx)) + f32(pr0[1] * ty)) + f32(pr0[2] * tz))
        d["mvp = (P*R)*T x87"] = f32(float(pr0[0]) * float(tx) + float(pr0[1]) * float(ty) + float(pr0[2]) * float(tz) + float(Pt))
        out[r] = d
    return out


# fitted ulp offsets of MVP[0][3], MVP[1][3] from the "sse mv, sse mvp" value (21_mvpfit.py)
FITS = {("c7", "zoom1"): (-2, 1)}


def main():
    for (corpus, step), (u0, u1) in FITS.items():
        out = cands(corpus, step)
        base0, base1 = out[0]["sse mv, sse mvp"], out[1]["sse mv, sse mvp"]
        t0, t1 = V_ulp(base0, u0), V_ulp(base1, u1)
        print(corpus, step, "target", float(t0), float(t1))
        for k in out[0]:
            print(f"  {k:30s} row0 {ulps(out[0][k], t0):+d} ulp  row1 {ulps(out[1][k], t1):+d} ulp")


def V_ulp(v, k):
    v = f32(v)
    for _ in range(abs(k)):
        v = np.nextafter(v, f32(1e9) if k > 0 else f32(-1e9))
    return v


if __name__ == "__main__":
    main()
