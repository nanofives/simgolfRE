"""S3.29: for a 29_texfit model's wrong pixels, the model's u, v, colour, filtered texel and 5-bit margin per channel.

    cd <tree> && ~/bin/py -3.12 ../../../log/roadmap/S3/29_detail.py <scene> <model> [n]
"""
import importlib
import pathlib
import sys

import numpy as np

sys.path.insert(0, str(pathlib.Path.cwd()))
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
TF = importlib.import_module("29_texfit")


def main():
    scene, nm = sys.argv[1], sys.argv[2]
    lim = int(sys.argv[3]) if len(sys.argv) > 3 else 20
    tris, tex, real = TF.load(scene)
    R5 = np.stack([(real >> 10) & 31, (real >> 5) & 31, real & 31], -1).astype(int)
    f = TF.MODELS[nm]
    n = 0
    for ti, t in enumerate(tris):
        rows = TF.coverage(t); ctx = {"rows": rows}
        ytop = rows[-1][0] if rows else 0
        for (y, xs) in rows:
            u, v, cols = f(t, y, xs, ctx)
            u = np.asarray(u, float); v = np.asarray(v, float)
            tx = TF.bilinear(tex, u, v)
            up = TF.plane(t, TF.S_, xs + 0.5, y + 0.5); vp = TF.plane(t, TF.T_, xs + 0.5, y + 0.5)
            k = (TF.K[y & 3][xs & 3] + 0.5) / 16
            for j, x in enumerate(xs):
                bad = []
                for ch in range(3):
                    qv = tx[j, ch] * cols[ch][j] * 31 / 255 + k[j]
                    if min(31, max(0, int(np.floor(qv)))) != R5[y, x, ch]:
                        bad.append((ch, qv, R5[y, x, ch]))
                if bad:
                    n += 1
                    print(f"tri {ti} ({x},{y}) rows-from-top {ytop - y} span-pos {x - xs[0]} u {u[j]:.6f} (plane {up[j]:.6f}) v {v[j]:.6f} (plane {vp[j]:.6f})")
                    for ch, qv, r in bad:
                        print(f"     ch{ch} model q {qv:.6f} real {r}  texel {tx[j, ch]:.4f} colour {cols[ch][j]:.7f}")
                    if n >= lim:
                        return


if __name__ == "__main__":
    main()
