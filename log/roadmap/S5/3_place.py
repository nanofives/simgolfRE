"""S5.3: where GDI places a ClearType glyph. Per glyph: the ink box of GDI's ClearType levels (samples from the pen, left =
first set sample decoded from the first nonzero window, right likewise) next to the ink box of GDI's own monochrome glyph
(lfQuality 3, pixels * 6) and FreeType's natural (unhinted) outline box (samples). Usage: 3_place.py <fontref hex> [chars]"""
import ctypes as c
import importlib
import pathlib
import sys

import numpy as np

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
G = importlib.import_module("3_gdi")
L = importlib.import_module("3_gdi_levels")
F = importlib.import_module("3_fit")
B = importlib.import_module("3_bwcmp")


def ct_box(lv):
    s = lv.reshape(lv.shape[0], -1).sum(0)   # per window (subpixel) column sums
    nz = np.nonzero(s)[0]
    # window k covers samples [2k - 2, 2k + 4) relative to pixel 0 sample 0; first nonzero window k0 -> ink starts in
    # [2k0 + 2, 2k0 + 4): use 2k0 + 3; last nonzero window k1 -> ink ends in [2k1 - 2, 2k1): 2k1 - 1
    return 2 * nz[0] + 3 - L.OX * 6, 2 * nz[-1] - 1 - L.OX * 6


def bw_box(b):
    nz = np.nonzero(b.sum(0))[0]
    return (nz[0] - L.OX) * 6, (nz[-1] + 1 - L.OX) * 6


def nat_box(pr, path, ppem, ch):
    smp = F.Probe.samples(pr, path, ppem, ch, "unhinted")
    nz = np.nonzero(smp.sum(0))[0]
    return nz[0] - L.OX * 6, nz[-1] + 1 - L.OX * 6


if __name__ == "__main__":
    fk = int(sys.argv[1], 16)
    chars = sys.argv[2] if len(sys.argv) > 2 else "".join(chr(i) for i in range(0x21, 0x7f))
    face, lh = G.FONTS[fk]
    g = G.Gdi()
    ct, mono = g.font(face, lh), g.font(face, lh, quality=3)
    pr = F.Probe()
    rows = []
    for ch in chars:
        lv = L.levels(g, ct, ch.encode("latin-1"))
        if not lv.any():
            continue
        cb, bb, nb = ct_box(lv), bw_box(B.bw(g, mono, ch.encode("latin-1"))), nat_box(pr, F.FILES[face], -lh, ord(ch))
        bw = sum(g.abc(ct, ord(ch)))
        a = c.c_int()
        f = pr.face(F.FILES[face], "unhinted")
        pr.d.pr_size(f, -lh, -lh)
        pr.d.pr_load(f, pr.d.pr_char(f, ord(ch)), F.NO_BITMAP | F.NO_HINTING, c.byref(a), None, None)
        nat = a.value * 6 / 64
        ctc, bwc, nc = sum(cb) / 2, sum(bb) / 2, sum(nb) / 2
        rows.append((ctc - bwc, ctc - nc, ctc - nc - (bw * 6 - nat) / 2))
        print(f"{ch!r} bw {bw} nat {nat:6.2f} ct {cb} bwbox {bb} natbox {nb}  ct-bw centre {ctc - bwc:+.1f}"
              f"  ct-nat centre {ctc - nc:+.1f}  minus half-difference {rows[-1][2]:+.1f}")
    r = np.array(rows)
    print("mean / sd: ct-bw %+.2f / %.2f   ct-nat %+.2f / %.2f   ct-nat-halfdiff %+.2f / %.2f" % (
        r[:, 0].mean(), r[:, 0].std(), r[:, 1].mean(), r[:, 1].std(), r[:, 2].mean(), r[:, 2].std()))
