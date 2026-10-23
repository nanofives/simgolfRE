"""Where does Windows' generic OpenGL sample a texture? (phase 5 step 3). A probe scene of quads that map a random texture at an
INTEGER number of texels per pixel keeps the bilinear weights constant inside each quad, so the weights the real GL used can be
fitted per quad from its colours; their difference from the exact weights is the sampling-position error, mapped here over the
screen.

    ~/bin/py -3.12 -m port.test.glshift_fit [scale] [world]   (default 2.0, 1.0; world = world units per pixel of the projection)
"""
from __future__ import annotations

import sys

import numpy as np

from port.test import glprobe as P
from port.test import lib as L

F32 = np.float32


def fit(scale: float = 2.0, seed: int = 5, w: float = 90.0, name: str = "texshift", world: float = 1.0):
    lib = L.load()
    P.probe_tex(lib, scales=[scale], name=name, seed=seed, w=w, quiet=True, world=world)
    d = P.PROBE / name
    real = np.frombuffer((d / "snaps" / "s1" / "dib_post.bin").read_bytes(), dtype="<u2").reshape(600, 800)
    R5 = np.stack([(real >> 10) & 31, (real >> 5) & 31, real & 31], axis=-1)
    rng = np.random.default_rng(seed)
    tex = rng.integers(0, 256, 64 * 64 * 3, dtype=np.uint8).reshape(64, 64, 3).astype(float)
    out = []
    for gy in range(6):
        for gx in range(8):
            ox = float(F32(gx * 100 + 3 + rng.uniform(0, 1)))
            oy = float(F32(gy * 100 + 3 + rng.uniform(0, 1)))
            span = w * scale / 64.0
            u0, v0 = rng.uniform(0, 1), rng.uniform(0, 1)
            xs = np.arange(int(ox) + 2, int(ox + w) - 2)
            ys = np.arange(int(oy) + 2, int(oy + w) - 2)
            X, Y = np.meshgrid(xs, ys)
            u = (u0 + (X + 0.5 - ox) / w * span) * 64 - 0.5
            v = (v0 + (Y + 0.5 - oy) / w * span) * 64 - 0.5
            i0, j0 = np.floor(u).astype(int), np.floor(v).astype(int)
            ax0, ay0 = float((u - i0).mean()), float((v - j0).mean())
            t00 = tex[np.mod(j0, 64), np.mod(i0, 64)]
            t10 = tex[np.mod(j0, 64), np.mod(i0 + 1, 64)]
            t01 = tex[np.mod(j0 + 1, 64), np.mod(i0, 64)]
            t11 = tex[np.mod(j0 + 1, 64), np.mod(i0 + 1, 64)]
            kk = P.K[Y % 4, X % 4][..., None]

            def match(ax, ay):
                c = t00 * (1 - ax) * (1 - ay) + t10 * ax * (1 - ay) + t01 * (1 - ax) * ay + t11 * ax * ay
                q = np.clip(np.floor(c * 31 / 255 + (kk + 0.5) / 16), 0, 31)
                return (q == R5[Y, X]).mean()
            grid = np.linspace(-0.03, 0.03, 61)
            best = max(((match(ax0 + a, ay0 + b), a, b) for a in grid for b in grid))
            # refine around the coarse optimum
            g2 = np.linspace(-0.001, 0.001, 9)
            best = max(((match(ax0 + best[1] + a, ay0 + best[2] + b), best[1] + a, best[2] + b) for a in g2 for b in g2))
            out.append((ox + w / 2, oy + w / 2, best[1] / scale, best[2] / scale, best[0], match(ax0, ay0)))
    return np.array(out)


def main():
    scale = float(sys.argv[1]) if len(sys.argv) > 1 else 2.0
    world = float(sys.argv[2]) if len(sys.argv) > 2 else 1.0
    a = fit(scale, world=world, name="texshift_w%g" % world)
    print("quads", len(a), "mean match exact weights %.3f -> fitted weights %.3f" % (a[:, 5].mean(), a[:, 4].mean()))
    x, y, sx, sy = a[:, 0], a[:, 1], a[:, 2], a[:, 3]
    for name, v in (("x", sx), ("y", sy)):
        A = np.stack([np.ones(len(x)), x - 400, y - 300], axis=1)
        coef, res, *_ = np.linalg.lstsq(A, v, rcond=None)
        print("sampling shift in %s (px) = %+.5f %+.3e*(x-400) %+.3e*(y-300)   rms residual %.5f (rms shift %.5f)" %
              (name, coef[0], coef[1], coef[2], float(np.sqrt(np.mean((A @ coef - v) ** 2))), float(np.sqrt(np.mean(v ** 2)))))
    np.save(P.PROBE / "texshift_fit.npy", a)


if __name__ == "__main__":
    main()
