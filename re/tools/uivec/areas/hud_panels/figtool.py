"""P41 hud_panels fix round: check a glyph's coverage against the art's, as text side by side.

cov_art(key, box, inside): the art's dark coverage over box (x0, y0, w, h) (fit.dark_coverage, `inside` a bool mask or
None = the whole box). cov_shape(shape, box): the fraction of each pixel inside a shape (8 x 8 samples).
show(key, box, shape, inside=None): prints art | mine | difference ('+' mine has more, '-' the art has more) and returns
(mean |difference|, worst pixel).
"""
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import common as C  # noqa: E402
import fit  # noqa: E402
import sdf  # noqa: E402


def cov_art(key, box, inside=None, lohi=None):
    rgb, m = C.sheet(key)
    x, y, w, h = box
    a = rgb[y:y + h, x:x + w]
    ins = m[y:y + h, x:x + w] if inside is None else inside
    if lohi:
        lo, hi = lohi
        L = fit.lum(a)
        cov = np.clip((hi - L) / (hi - lo), 0, 1) * ins
        return cov
    cov, lo, hi = fit.dark_coverage(a, ins)
    return cov


def cov_shape(shape, box, n=8):
    x, y, w, h = box
    jj, ii = np.mgrid[0:h * n, 0:w * n]
    X = x + (ii + 0.5) / n
    Y = y + (jj + 0.5) / n
    d = sdf.sd(shape, X, Y)
    return (d < 0).reshape(h, n, w, n).mean(axis=(1, 3))


def ch(c):
    return "#" if c > 0.85 else "+" if c > 0.6 else ":" if c > 0.35 else "." if c > 0.12 else "_"


def show(key, box, shape, inside=None, lohi=None, quiet=False):
    A = cov_art(key, box, inside, lohi)
    B = cov_shape(shape, box)
    if inside is not None:
        B = B * inside
    D = B - A
    x, y, w, h = box
    if not quiet:
        hdr = "".join(str((x + i) % 10) for i in range(w))
        print("     " + hdr + "   " + hdr + "   " + hdr)
        for j in range(h):
            a = "".join(ch(c) for c in A[j])
            b = "".join(ch(c) for c in B[j])
            d = "".join("+" if v > 0.35 else "-" if v < -0.35 else "." if abs(v) > 0.15 else "_" for v in D[j])
            print(f"{y + j:4d} {a} | {b} | {d}")
    mad = float(np.abs(D).mean())
    print(f"coverage mean |diff| {mad:.3f}  worst {np.abs(D).max():.2f}  art mass {A.sum():.1f} mine {B.sum():.1f}")
    return mad
