"""S3.29: list the differing pixels of a probe (GlRaster keep_float against the real frame): position, channel, port value (8-bit),
signed margin to the 5-bit boundary, and the triangle id.

    cd <tree> && SG_PORTSIM_DLL=... ~/bin/py -3.12 ../../../log/roadmap/S3/29_where.py <scene> [knobs]
"""
import pathlib
import sys

import numpy as np

sys.path.insert(0, str(pathlib.Path.cwd()))
from port.test import glprobe as P, glraster as R, lib as L  # noqa: E402

KD = np.array([[15, 7, 13, 5], [3, 11, 1, 9], [12, 4, 14, 6], [0, 8, 2, 10]])


def main():
    sc = sys.argv[1]
    knobs = {int(a): int(b) for a, b in (kv.split("=") for kv in (sys.argv[2] if len(sys.argv) > 2 else "").split(",") if kv)}
    lib = L.load()
    d = P.PROBE / sc
    setup = R.Setup(d)

    def patch(r, dll):
        t = "".join(f"#tune {i} {v}\n" for i, v in knobs.items()).encode()
        if t:
            dll.sg_raster_exec(r, t, len(t))
    post, got, st, unk, pre, ffb, sfb = R.run(lib, setup, "s1", patch=patch, want_float=True)
    P5, G5 = P.rgb5(post).reshape(600, 800, 3), P.rgb5(got).reshape(600, 800, 3)
    ys, xs, cs = np.nonzero(P5 != G5)
    print(sc, "differing channel values", len(ys))
    for y, x, c in zip(ys, xs, cs):
        v = float(ffb[y, x, c]); k = KD[y & 3][x & 3]
        q = v * 31 / 255 + (k + 0.5) / 16
        m = (q - np.floor(q + 0.5)) * 255 / 31
        print(f"  ({x:3d},{y:3d}) ch{c} port {G5[y,x,c]} real {P5[y,x,c]} value {v:10.5f} margin {m:+.5f} texel {sfb[y,x,c]:9.4f}")


if __name__ == "__main__":
    main()
