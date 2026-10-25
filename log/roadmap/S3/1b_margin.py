"""S3.1b: on a probe variant (1b_probe.py's scratch root), where the real 5-bit value disagrees with GlRaster's, how far GlRaster's
pre-quantisation colour is from the interval the real value implies (in 8-bit units, signed: + = the real colour is higher).

    cd port/build/S3_head && SG_PORTSIM_DLL=<dll> PYTHONPATH=. ~/bin/py -3.12 ../../../log/roadmap/S3/1b_margin.py g6 i012_raised_strip__t0
"""
import pathlib
import sys

import numpy as np

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import importlib.util  # noqa: E402

spec = importlib.util.spec_from_file_location("p", pathlib.Path(__file__).resolve().parent / "1b_probe.py")
P = importlib.util.module_from_spec(spec)
spec.loader.exec_module(P)

from port.test import glraster as R, lib as L  # noqa: E402

K = np.array([[15, 7, 13, 5], [3, 11, 1, 9], [12, 4, 14, 6], [0, 8, 2, 10]])


def margins(post, ffb):
    real = P.rgb(post).reshape(600, 800, 3)
    yy, xx = np.mgrid[0:600, 0:800]
    d = ((K[yy & 3, xx & 3] + 0.5) / 16.0)[..., None]
    lo = (real - d) * 255.0 / 31.0          # colour interval implied by the real level: [lo, hi)
    hi = (real + 1 - d) * 255.0 / 31.0
    c = ffb.astype(np.float64)
    m = np.where(c < lo, lo - c, np.where(c >= hi, hi - c, 0.0))
    return m, c


def main():
    lib = L.load()
    R.bind(lib.dll)
    label, step = sys.argv[1], sys.argv[2]
    post, got, st, dd, res = P.compare(lib, label, step, want_float=True)
    ffb = res[5]
    m, c = margins(post, ffb)
    wrote = (ffb >= 0).all(axis=-1)
    for k, name in enumerate("rgb"):
        mk = m[..., k][wrote]
        nz = mk[mk != 0]
        print(f"{name}: px {wrote.sum()} wrong {nz.size} up {int((nz > 0).sum())} down {int((nz < 0).sum())} "
              f"median |m| {np.median(np.abs(nz)) if nz.size else 0:.4f} max |m| {np.abs(nz).max() if nz.size else 0:.4f}")
        if nz.size:
            print("   quantiles", np.round(np.quantile(nz, [0.01, 0.1, 0.5, 0.9, 0.99]), 4))
    # does the error depend on the colour level? show wrong fraction per 8-bit colour bucket of red
    cr = c[..., 0][wrote]
    mr = m[..., 0][wrote]
    for lo in range(0, 256, 32):
        sel = (cr >= lo) & (cr < lo + 32)
        if sel.sum():
            print(f"   red {lo:3d}..{lo + 31:3d}: px {int(sel.sum()):6d} wrong {int((mr[sel] != 0).sum()):5d} mean m {mr[sel][mr[sel] != 0].mean() if (mr[sel] != 0).any() else 0:+.3f}")


if __name__ == "__main__":
    main()
