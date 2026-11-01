"""S3.21: fit the real GL's MVP row values on one pass (feedback points): every element of row 0 (x) and row 1 (y) over its
float32 neighbours, the transform translation-first in float32, the measured viewport rule.

    ~/bin/py -3.12 log/roadmap/S3/21_mvpfit.py <corpus> <step> [span]
"""
import importlib.util
import itertools
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
        v = np.nextafter(v, f32(1e9) if k > 0 else f32(-1e9))
    return v


def main():
    corpus, step = sys.argv[1], sys.argv[2]
    span = int(sys.argv[3]) if len(sys.argv) > 3 else 3
    pts = V.read_points(C.OUT / f"{corpus}_{step}.f32")
    verts, vp = V.build(corpus, step, "f32")
    mv, pr = verts[0][1], verts[0][2]
    assert all(v[1] is mv and v[2] is pr for v in verts), "one matrix pair per pass assumed"
    mvp = V.mul(pr, mv, "f32")
    O = np.array([v[0] for v in verts]).astype(np.float32)
    p = C.predict(verts, vp)
    ins = p[:, 2] > 0
    O = O[ins]
    print("mv row0", [float(f32(v)) for v in mv[0]], "row1", [float(f32(v)) for v in mv[1]])
    print("mvp row0", [float(f32(v)) for v in mvp[0]], "row1", [float(f32(v)) for v in mvp[1]])
    for row, axis, c, h in ((0, 0, 400.0, 400.0), (1, 1, 300.0, 300.0)):
        nz = [k for k in range(4) if mvp[row, k] != 0]
        res = []
        for ks in itertools.product(range(-span, span + 1), repeat=len(nz)):
            m = [f32(mvp[row, k]) for k in range(4)]
            for k, d in zip(nz, ks):
                m[k] = ulpn(m[k], d)
            s = np.full(len(O), m[3], np.float32)
            for k in range(3):
                s = f32(s + f32(O[:, k] * m[k]))
            w = np.round(256 * (f32(s * f32(h - 1 / 256))).astype(np.float64) + 256 * c) / 256
            res.append((int((w != pts[:, axis]).sum()), ks))
        res.sort()
        print("row", row, "nonzero", nz, "best", res[:5], "unchanged", [r for r in res if not any(r[1])])


if __name__ == "__main__":
    main()
