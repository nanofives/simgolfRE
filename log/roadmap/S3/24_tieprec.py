"""S3.24: tie pixels (|o| = N/2 exactly with the 1/256-rounded ends) decided by the minor offset o' computed from other vertex
models; a model predicts `drawn` iff o' lies in the window (half-open low or high side). Scored against the real GL.

Vertex models (window coordinate v of an integer vertex, c / h the viewport centre and half size per axis):
  r<k>   c + (v - c)(1 - 2^-8 / h) rounded to 1/2^k        (r8 = the U-1076 setup the model uses)
  f64    the same unrounded, double;  f32  the same in float32 arithmetic
  int    v itself (no setup)
plus the window side: lo = [-N/2, N/2), hi = (-N/2, N/2]. The line centre's minor coordinate at the pixel centre's major
coordinate is computed from the model's ends in double, or in float32 (`/f32o`).

    ~/bin/py -3.12 log/roadmap/S3/24_tieprec.py <npz>...
"""
import importlib.util
import pathlib
import sys

import numpy as np

HERE = pathlib.Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("p24", HERE / "24_probe.py")
P = importlib.util.module_from_spec(spec)
spec.loader.exec_module(P)
f32 = np.float32


def vert(v, c, h, mode):
    if mode == "int":
        return float(v)
    if mode == "f32":
        return float(f32(c) + (f32(v) - f32(c)) * (f32(1.0) - f32(1.0) / f32(256.0) / f32(h)))
    u = c + (v - c) * (1.0 - 1.0 / 256.0 / h)
    if mode == "f64":
        return u
    k = int(mode[1:])
    return np.floor(u * (1 << k) + 0.5) / (1 << k)


def ends(r, mode):
    ax, bx = float(r["x1"]), float(r["x2"])
    ay, by = (float(r["y1"]), float(r["y2"])) if r["flip"] else (600.0 - r["y1"], 600.0 - r["y2"])
    return vert(ax, 400, 400, mode), vert(ay, 300, 300, mode), vert(bx, 400, 400, mode), vert(by, 300, 300, mode)


MODES = ["r4", "r6", "r8", "r10", "r12", "r16", "r20", "f64", "f32", "int"]


def main():
    cases = []
    for p in sys.argv[1:]:
        for r in P.load(p):
            cov, g = P.coverage(r, "s38")
            if g is None:
                continue
            dx, dy = g["bx"] - g["ax"], g["by"] - g["ay"]
            major = max(abs(dx), abs(dy))
            nn = np.floor(2 * g["half"] * g["L"] / major) + 2
            o, xmaj = P.minor_offset(g)
            tie = (np.abs(np.abs(o) - nn / 2) < 1e-9) & (cov > 0)
            if not tie.any():
                continue
            m = P.blend(r, cov)
            vis = m != r["pre"]
            rd = r["real"] != r["pre"]
            for yy, xx in zip(*np.nonzero(tie & vis)):
                cases.append((r, float(g["X"][yy, xx] + 0.5), float(g["Y"][yy, xx] + 0.5), nn, xmaj, bool(rd[yy, xx])))
    print(len(cases), "tie pixels,", sum(c[5] for c in cases), "drawn")
    res = []
    for mode in MODES:
        for f32o in (False, True):
            for side in ("lo", "hi"):
                hit = 0
                for r, X, Y, nn, xmaj, drawn in cases:
                    ax, ay, bx, by = ends(r, mode)
                    if xmaj:
                        if f32o:
                            yc = float(f32(ay) + (f32(X) - f32(ax)) * f32(by - ay) / f32(bx - ax))
                        else:
                            yc = ay + (X - ax) * (by - ay) / (bx - ax)
                        o = Y - yc
                    else:
                        if f32o:
                            xc = float(f32(ax) + (f32(Y) - f32(ay)) * f32(bx - ax) / f32(by - ay))
                        else:
                            xc = ax + (Y - ay) * (bx - ax) / (by - ay)
                        o = X - xc
                    pred = (-nn / 2 <= o < nn / 2) if side == "lo" else (-nn / 2 < o <= nn / 2)
                    hit += pred == drawn
                res.append((hit, f"{mode}{'/f32o' if f32o else ''}/{side}"))
    for hit, name in sorted(res, reverse=True)[:15]:
        print(f"{hit:5d} / {len(cases)}  {name}")


if __name__ == "__main__":
    main()
