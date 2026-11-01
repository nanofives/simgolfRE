"""S3.21: how far each remaining one-level pixel's pre-quantisation value lies from its 5-bit boundary (GlRaster keep_float run),
in 8-bit units, against the same statistic over all drawn pixels; plus the pixel's blend / texture state.

    cd <tree> && SG_PORTSIM_DLL=... ~/bin/py -3.12 ../../../log/roadmap/S3/21_margin.py <corpus> <step ...>
"""
import pathlib
import sys

import numpy as np

sys.path.insert(0, str(pathlib.Path.cwd()))
from port.test import glraster as R, lib as L  # noqa: E402

KD = [[15, 7, 13, 5], [3, 11, 1, 9], [12, 4, 14, 6], [0, 8, 2, 10]]   # GlRaster kDither[y & 3][x & 3]


def rgb(a):
    return np.stack([(a >> 10) & 31, (a >> 5) & 31, a & 31], axis=-1).astype(int)


def main():
    corpus = sys.argv[1]
    lib = L.load()
    setup = R.Setup(pathlib.Path.cwd() / "port" / "corpus" / "gl" / corpus)
    allm = []
    for step in sys.argv[2:]:
        post, got, st, _u, pre, ffb, sfb = R.run(lib, setup, step, want_float=True)
        P, G = rgb(post).reshape(600, 800, 3), rgb(got).reshape(600, 800, 3)
        d = np.abs(P - G)
        ys, xs, cs = np.nonzero(d == 1)
        print(step, "one-level channel values", len(ys))
        for y, x, c in list(zip(ys, xs, cs))[:30]:
            v = float(ffb[y, x, c])
            k = KD[y & 3][x & 3]
            q = v * 31 / 255 + (k + 0.5) / 16
            m = (q - np.floor(q + 0.5)) * 255 / 31   # signed distance to the nearest boundary, in 8-bit units
            blend = sfb[y, x, 7]
            print(f"  ({x:3d},{y:3d}) ch {c} value {v:10.5f} margin {m:+.5f} port {G[y, x, c]} orig {P[y, x, c]} texel {sfb[y, x, c]:8.3f} blend {blend:.0f}")
            allm.append(abs(m))
    a = np.array(allm)
    if len(a):
        print("margins |m| quantiles", np.quantile(a, [0.1, 0.5, 0.9, 1.0]).round(5).tolist())


if __name__ == "__main__":
    main()
