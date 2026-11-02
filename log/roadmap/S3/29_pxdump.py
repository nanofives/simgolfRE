"""S3.29: GlRaster's one-level pixels of a corpus pass in detail (keep_float run): the triangle (window vertices, s / t, colours are
not kept), the pixel's plane u / v, the texel quad and weights, the port's pre-quantisation value and margin, real against port.

    cd <tree> && SG_PORTSIM_DLL=... ~/bin/py -3.12 ../../../log/roadmap/S3/29_pxdump.py <corpus> <step> [--knob i=v,...] [n]
"""
import importlib
import pathlib
import sys

import numpy as np

sys.path.insert(0, str(pathlib.Path.cwd()))
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from port.test import glraster as R, lib as L  # noqa: E402
LO = importlib.import_module("29_locate")
RZ = importlib.import_module("29_resid")
KD = np.array([[15, 7, 13, 5], [3, 11, 1, 9], [12, 4, 14, 6], [0, 8, 2, 10]])


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    knobs = {}
    for a in sys.argv[1:]:
        if a.startswith("--knob="):
            knobs = {int(k): int(v) for k, v in (kv.split("=") for kv in a[7:].split(","))}
    corpus, step = args[0], args[1]
    lim = int(args[2]) if len(args) > 2 else 20
    lib = L.load()
    setup = R.Setup(pathlib.Path.cwd() / "port" / "corpus" / "gl" / corpus)
    post, got, ids, tri = LO.run(lib, setup, step, knobs)
    _p, _g, ffb, sfb, rho = RZ.run(lib, setup, step, knobs)
    P, G = LO.ch(post), LO.ch(got)
    d = np.abs(P - G).max(-1)
    ys, xs = np.nonzero(d >= 1)
    for y, x in list(zip(ys, xs))[:lim]:
        ti = ids[y, x]
        T = tri[ti]
        dist, u, v, rows, area = LO.info(T, x, y)
        k = KD[y & 3][x & 3]
        print(f"({x},{y}) tri {ti} rho {rho[y, x]:.3f} edge-dist {dist:.3f} rows-from-top {rows:.1f} area {area:.0f} u {u:.5f} v {v:.5f}"
              f" frac(u-.5) {(u - .5) % 1:.4f} frac(v-.5) {(v - .5) % 1:.4f} blend {sfb[y, x, 7]:.0f}")
        print("     verts", " ".join(f"({T[2*i]:.3f},{T[2*i+1]:.3f}|{T[6+2*i]:.3f},{T[7+2*i]:.3f})" for i in range(3)))
        for c in range(3):
            if P[y, x, c] != G[y, x, c]:
                vv = float(ffb[y, x, c]); q = vv * 31 / 255 + (k + 0.5) / 16
                print(f"     ch{c} port {G[y, x, c]} real {P[y, x, c]} value {vv:.5f} q {q:.6f} texel {sfb[y, x, c]:.4f} dst {sfb[y, x, 4 + c]:.0f}")


if __name__ == "__main__":
    main()
