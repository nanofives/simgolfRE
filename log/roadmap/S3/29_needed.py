"""S3.29: for each pixel a 29_texfit.py model gets wrong, the smallest change of u alone, of v alone or of the channel's colour alone
(each searched in float32 ulps of that value, up to 4096 ulps either way) that makes the model's final stage (--final fd) give
the real 5-bit value; prints the walk position too.

    cd <tree> && ~/bin/py -3.12 ../../../log/roadmap/S3/29_needed.py <scene> <model>
"""
import importlib
import pathlib
import sys

import numpy as np

sys.path.insert(0, str(pathlib.Path.cwd()))
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
TF = importlib.import_module("29_texfit")
f32 = np.float32


def q_of(tex, u, v, cols, k):
    return [float(np.floor(q[0])) for q in TF.final32(tex, np.array([u]), np.array([v]), [np.array([c]) for c in cols], np.array([k]), "fd")]


def ulps(x, n):
    """x moved by n float32 ulps (through the ordered integer view of the float)"""
    b = int(np.array([f32(x)]).view(np.int32)[0])
    o = b if b >= 0 else -(b & 0x7fffffff)
    o += n
    b2 = o if o >= 0 else (-o) | -0x80000000
    return float(np.array([b2], np.int32).view(f32)[0])


def main():
    scene, nm = sys.argv[1], sys.argv[2]
    tris, tex, real = TF.load(scene)
    R5 = np.stack([(real >> 10) & 31, (real >> 5) & 31, real & 31], -1).astype(int)
    f = TF.MODELS[nm]
    for ti, t in enumerate(tris):
        rows = TF.coverage(t); ctx = {"rows": rows}
        ytop = rows[-1][0] if rows else 0
        for (y, xs) in rows:
            u, v, cols = f(t, y, xs, ctx)
            k = (TF.K[y & 3][xs & 3] + 0.5) / 16
            qs = TF.final32(tex, np.asarray(u, float), np.asarray(v, float), cols, k, "fd")
            for j, x in enumerate(xs):
                got = [min(31, max(0, int(np.floor(qs[c][j])))) for c in range(3)]
                want = list(R5[y, x])
                if got == want:
                    continue
                cj = [float(cols[c][j]) for c in range(3)]
                res = {}
                for name in ("u", "v", "c"):
                    best = None
                    for n in sorted(range(-4096, 4097), key=abs):
                        if n == 0:
                            continue
                        uu, vv, cc = float(u[j]), float(v[j]), list(cj)
                        if name == "u":
                            uu = ulps(uu, n)
                        elif name == "v":
                            vv = ulps(vv, n)
                        else:
                            ch = [c for c in range(3) if got[c] != want[c]][0]
                            cc[ch] = ulps(cc[ch] * 31.0, n) / 31.0
                        g2 = [min(31, max(0, int(q))) for q in q_of(tex, uu, vv, cc, k[j])]
                        if g2 == want:
                            best = n
                            break
                    res[name] = best
                print(f"tri {ti} ({x},{y}) rows-from-top {ytop - y} span-pos {x - xs[0]} u {float(u[j]):.5f} v {float(v[j]):.5f}"
                      f" needed ulps: u {res['u']} v {res['v']} colour {res['c']}")


if __name__ == "__main__":
    main()
