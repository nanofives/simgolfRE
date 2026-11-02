"""S3.29 (U-1309): the real GL's error against GlRaster's plane model on a slow-ramp probe, read at the dither flips.

Along a row and one dither phase (x & 3) the probe's channel value is monotonic; where the real 5-bit value steps from L - 1 to L
between two phase columns xa < xb, the real pre-quantisation value q (5-bit units) satisfies q_real(xa) < L <= q_real(xb). With
q_real = q_port + e and e smooth, e lies in (L - q_port(xb), L - q_port(xa)); the midpoint is printed per flip, in 5-bit units and
in the channel's own units (`--gain g`: e / g, e.g. texels of u when the value is 255 * alpha).

    cd <tree> && SG_PORTSIM_DLL=... ~/bin/py -3.12 ../../../log/roadmap/S3/29_flips.py <scene> [--ch 0] [--gain 31] [--save f.npy]
"""
import pathlib
import sys

import numpy as np

sys.path.insert(0, str(pathlib.Path.cwd()))
from port.test import glprobe as P, glraster as R, lib as L  # noqa: E402

KD = np.array([[15, 7, 13, 5], [3, 11, 1, 9], [12, 4, 14, 6], [0, 8, 2, 10]])


def flips(scene, ch=0, knobs=None):
    lib = L.load()
    d = P.PROBE / scene
    setup = R.Setup(d)

    def patch(r, dll):
        t = "".join(f"#tune {i} {v}\n" for i, v in (knobs or {}).items()).encode()
        if t:
            dll.sg_raster_exec(r, t, len(t))
    post, got, st, unk, pre, ffb, sfb = R.run(lib, setup, "s1", patch=patch, want_float=True)
    real = P.rgb5(post).reshape(600, 800, 3)[:, :, ch]
    ky = (KD[(np.arange(600) & 3)[:, None], (np.arange(800) & 3)[None, :]] + 0.5) / 16
    q = ffb[:, :, ch].astype(np.float64) * 31 / 255 + ky
    out = []   # (y, x_a, x_b, e_lo, e_hi)
    for y in range(600):
        for ph in range(4):
            xs = np.arange(ph, 800, 4)
            r = real[y, xs]; qq = q[y, xs]
            ok = ffb[y, xs, ch] >= 0
            for i in range(len(xs) - 1):
                if not (ok[i] and ok[i + 1]):
                    continue
                if r[i + 1] == r[i] + 1:      # rising flip
                    Lv = r[i + 1]
                    out.append((y, xs[i], xs[i + 1], Lv - qq[i + 1], Lv - qq[i]))
                elif r[i + 1] == r[i] - 1:    # falling flip: q_real(xa) >= L > q_real(xb), L = r[i]
                    Lv = r[i]
                    out.append((y, xs[i], xs[i + 1], Lv - qq[i], Lv - qq[i + 1]))
    return np.array(out, float)


def main():
    a = sys.argv[1:]
    scene = a[0]
    ch = int(a[a.index("--ch") + 1]) if "--ch" in a else 0
    gain = float(a[a.index("--gain") + 1]) if "--gain" in a else 1.0
    F = flips(scene, ch)
    lo, hi = np.minimum(F[:, 3], F[:, 4]), np.maximum(F[:, 3], F[:, 4])
    mid = (lo + hi) / 2
    print(scene, "flips", len(F), "error interval width median", float(np.median(hi - lo)))
    for b in range(8):
        s = (F[:, 1] >= b * 100) & (F[:, 1] < b * 100 + 100)
        if s.any():
            print(f"  x {b*100:3d}: n {int(s.sum()):5d} mid mean {mid[s].mean()/gain:+.3e} min {mid[s].min()/gain:+.3e} max {mid[s].max()/gain:+.3e}")
    for b in range(6):
        s = (F[:, 0] >= b * 100) & (F[:, 0] < b * 100 + 100)
        if s.any():
            print(f"  y {b*100:3d}: n {int(s.sum()):5d} mid mean {mid[s].mean()/gain:+.3e} min {mid[s].min()/gain:+.3e} max {mid[s].max()/gain:+.3e}")
    if "--save" in a:
        np.save(a[a.index("--save") + 1], F)


if __name__ == "__main__":
    main()
