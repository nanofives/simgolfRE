"""S3.21: which float cos / sin the real GL uses for a glRotatef angle: brute force over float32 neighbours on one pass.

    ~/bin/py -3.12 log/roadmap/S3/21_trigfit.py <corpus> <step> <angle>
"""
import importlib.util
import math
import os
import pathlib
import sys

import numpy as np

HERE = pathlib.Path(__file__).resolve().parent
_s = importlib.util.spec_from_file_location("c21", HERE / "21_vcheck.py")
C = importlib.util.module_from_spec(_s)
_s.loader.exec_module(C)
V = C.V
f32 = np.float32


def ulpn(v, k):
    v = f32(v)
    for _ in range(abs(k)):
        v = np.nextafter(v, f32(9) if k > 0 else f32(-9))
    return v


def main():
    corpus, step, angle = sys.argv[1], sys.argv[2], float(sys.argv[3])
    f = C.OUT / f"{corpus}_{step}.f32"
    pts = V.read_points(f)
    a = math.radians(angle)
    c0, s0 = f32(math.cos(a)), f32(math.sin(a))
    af = f32(f32(angle) * f32(math.pi / 180))
    print("double radians: cos", float(c0), "sin", float(s0), "| float radians", float(af), "cos", float(f32(math.cos(float(af)))),
          "sin", float(f32(math.sin(float(af)))))
    orig = V.rot
    res = []
    for kc in range(-3, 4):
        for ks in range(-3, 4):
            def rot(ang, x, y, z, prec, kc=kc, ks=ks):
                m = orig(ang, x, y, z, prec)
                if abs(ang - angle) < 1e-9 and y == 1.0:
                    c, s = ulpn(c0, kc), ulpn(s0, ks)
                    m[0, 0] = m[2, 2] = c; m[0, 2] = s; m[2, 0] = -s
                return m
            V.rot = rot
            verts, vp = V.build(corpus, step, "f32")
            p = C.predict(verts, vp)
            ins = p[:, 2] > 0
            if ins.sum() != len(pts):
                res.append((10 ** 9, kc, ks, "pairing"))
                continue
            q = p[ins]
            e = int((q[:, 0] != pts[:, 0]).sum()) + int((q[:, 1] != pts[:, 1]).sum())
            res.append((e, kc, ks, float(ulpn(c0, kc)), float(ulpn(s0, ks))))
    V.rot = orig
    for r in sorted(res)[:8]:
        print(" ", r)


if __name__ == "__main__":
    main()
