"""S3.15 (U-1392): per differing pixel of a glreplay-exact variant (1b_probe.py's snaps/<step>__<name> under the scratch root),
the pipeline values GlRaster kept (sg_raster_keep_float): texel, final 8-bit value, the dither offset and where the value lies
against the level boundary, so the error's arithmetic can be read.

    cd port/build/S3_15 && SG_PORTSIM_DLL=... S31B_SCRATCH=... PYTHONPATH=. ~/bin/py -3.12 ../../../log/roadmap/S3/15_margin.py c7 fuzz1_z0__tex [n]
"""
import os
import pathlib
import sys

import numpy as np

from port.test import glraster as R, lib as L

SCR = pathlib.Path(os.environ["S31B_SCRATCH"])
DITHER = [[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]]


def rgb(a):
    return np.stack([(a >> 10) & 31, (a >> 5) & 31, a & 31], axis=-1).astype(int)


def main():
    label, step = sys.argv[1], sys.argv[2]
    nshow = int(sys.argv[3]) if len(sys.argv) > 3 else 40
    lib = L.load()
    R.bind(lib.dll)
    setup = R.Setup(SCR / label)
    post, got, st, _unk, pre, ffb, sfb = R.run(lib, setup, step, want_float=True)
    P, G = rgb(post).reshape(600, 800, 3), rgb(got).reshape(600, 800, 3)
    d = np.abs(P - G)
    ys, xs = np.nonzero(d.max(2) > 0)
    print(f"{label}/{step}: drawn {st[2]} differing {len(xs)}")
    margins = []
    rows = []
    for y, x in zip(ys, xs):
        dd = (DITHER[y & 3][x & 3] + 0.5) / 16.0
        for k in range(3):
            if P[y, x, k] == G[y, x, k]:
                continue
            v = float(ffb[y, x, k])
            q = v * 31.0 / 255.0 + dd
            # the 8-bit value at which the real level would start (real > ours) or end (real < ours)
            want = P[y, x, k]
            edge = (want - dd) * 255.0 / 31.0 if want > G[y, x, k] else (want + 1 - dd) * 255.0 / 31.0
            m = edge - v
            margins.append(m)
            tex = sfb[y, x, k]
            rows.append((x, y, "rgb"[k], tex, v, v / tex if tex else float("nan"), m, int(sfb[y, x, 7]), want - G[y, x, k]))
    m = np.array(margins)
    if len(m):
        print("margin (8-bit units needed to reach the real level): |m| quantiles",
              np.quantile(np.abs(m), [0, 0.25, 0.5, 0.75, 0.9, 1.0]).round(4))
        print("sign of the real-minus-ours error: +", int((m > 0).sum()), " -", int((m < 0).sum()))
    for r in rows[:nshow]:
        print("  (%d,%d) %s texel %.3f out %.4f col %.5f margin %+.4f blend %d err %+d" % r)


if __name__ == "__main__":
    main()
