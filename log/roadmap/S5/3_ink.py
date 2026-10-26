"""S5.3: ink per row. Under the 6-sample box every sample is counted by 3 windows, so a row's level sum / 3 = the set samples in
that row (averaged over subrows with y smoothing). Compares GDI with 3_fity's model per row, and reports the mean of
(GDI - model) samples per ink row and the mean horizontal ink centre difference (samples) per font: a thickness and a placement
figure that do not depend on each other. Usage: 3_ink.py <fontref hex>[,...] [--v 6] [--xmode scale] [--interp 40]"""
import argparse
import importlib
import pathlib
import sys

import numpy as np

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
G = importlib.import_module("3_gdi")
L = importlib.import_module("3_gdi_levels")
F = importlib.import_module("3_fit")
Y = importlib.import_module("3_fity")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("fonts")
    ap.add_argument("--chars", default="".join(chr(i) for i in range(0x21, 0x7f)))
    ap.add_argument("--v", type=int, default=6)
    ap.add_argument("--xmode", default="scale")
    ap.add_argument("--interp", type=int, default=40)
    ap.add_argument("--ymode", default="ppem")
    ap.add_argument("--per-glyph", action="store_true")
    ap.add_argument("--xfac", type=int, default=1)
    a = ap.parse_args()
    g = G.Gdi()
    pr = F.Probe()
    for fk in a.fonts.split(","):
        face, lh = G.FONTS[int(fk, 16)]
        f = g.font(face, lh)
        m = Y.YModel(pr, a.v, a.ymode, "round", a.interp, 0, xmode=a.xmode)
        m.xfac = a.xfac
        thick, cen, rowsdiff = [], [], 0
        for ch in a.chars:
            ref = L.levels(g, f, ch.encode("latin-1")).astype(float)
            bw = sum(g.abc(f, ord(ch)))
            lv = m.levels(F.FILES[face], -lh, ord(ch), bw).astype(float)
            rs, ms = ref.reshape(L.H, -1), lv.reshape(L.H, -1)
            x = np.arange(rs.shape[1])
            gl_t, gl_c = [], []
            for y in range(L.H):
                if rs[y].sum() == 0 and ms[y].sum() == 0:
                    continue
                if (rs[y].sum() == 0) != (ms[y].sum() == 0):
                    rowsdiff += 1
                    continue
                gl_t.append((rs[y].sum() - ms[y].sum()) / 3)
                gl_c.append(((rs[y] * x).sum() / rs[y].sum() - (ms[y] * x).sum() / ms[y].sum()) * 2)
            thick += gl_t
            cen += gl_c
            if a.per_glyph and gl_t:
                print(f"  {ch!r} thick {np.mean(gl_t):+.2f} centre {np.mean(gl_c):+.2f}")
        print(f"{fk} {face} {-lh}: rows {len(thick)} thickness gdi-model {np.mean(thick):+.3f} samples (sd {np.std(thick):.3f})"
              f" centre {np.mean(cen):+.3f} samples (sd {np.std(cen):.3f}) rows with ink on one side only {rowsdiff}")


if __name__ == "__main__":
    main()
