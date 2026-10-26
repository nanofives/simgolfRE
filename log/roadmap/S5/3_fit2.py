"""S5.3: the ClearType coverage model with GDI's compatible-width placement. FreeType loads the glyph in a y-hinting mode
(interpreter v40 by default), scales x by 6, and shifts the outline right by (6 * BW advance - natural advance) / 2 in
samples, where BW advance = GDI's ABC sum (the compatible width) and the natural advance is the glyph's advance in the
overscaled space (`adv` mode). The shift is applied in 26.6 with the rounding of `--round`. Levels by the 6-sample box.

    ~/bin/py -3.12 log/roadmap/S5/3_fit2.py [--fonts ...] [--chars ...] [--interp 40] [--adv linear|hinted|x6]
                                            [--round none|sample|half] [--bias N] [--show N]
"""
import argparse
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
E = importlib.import_module("3_edges")

OX, OY, W, H, S = L.OX, L.OY, L.W, L.H, 6


class Model:
    def __init__(self, pr, interp=40, xfac=1, flags=F.NO_BITMAP, adv="linear", rnd="none", bias=0):
        self.pr, self.interp, self.xfac, self.flags, self.adv, self.rnd, self.bias = pr, interp, xfac, flags, adv, rnd, bias

    def natural(self, f, gid, ppem):
        """natural advance in samples * 64"""
        d = self.pr.d
        a = c.c_int()
        if self.adv == "linear":
            d.pr_size(f, ppem, ppem)
            d.pr_load(f, gid, F.NO_BITMAP | F.NO_HINTING, c.byref(a), None, None)
            return a.value * S
        if self.adv == "x6":
            d.pr_size(f, S * ppem, ppem)
            d.pr_load(f, gid, self.flags, c.byref(a), None, None)
            return a.value
        d.pr_size(f, ppem, ppem)
        d.pr_load(f, gid, self.flags, c.byref(a), None, None)
        return a.value * S

    def samples(self, path, ppem, ch, bw):
        d = self.pr.d
        f = self.pr.face(path, f"m{self.interp}")
        d.pr_interp(self.interp)
        gid = d.pr_char(f, ch)
        nat = self.natural(f, gid, ppem)
        sh = (bw * S * 64 - nat) / 2 + self.bias
        if self.rnd == "sample":
            sh = int(np.floor(sh / 64 + 0.5)) * 64
        elif self.rnd == "half":
            sh = int(np.floor(sh / 32 + 0.5)) * 32
        elif self.rnd == "floor":
            sh = int(np.floor(sh / 64)) * 64
        else:
            sh = int(np.floor(sh + 0.5))
        d.pr_size(f, self.xfac * ppem, ppem)
        adv, npts = c.c_int(), c.c_int()
        if d.pr_load(f, gid, self.flags, c.byref(adv), c.byref(npts), None):
            raise RuntimeError("load")
        out = np.zeros((H, W * S), np.uint8)
        if npts.value > 0:
            e = d.pr_render(f, (S // self.xfac) << 16, OX * S * 64 + sh, (H - OY) * 64, 0,
                            out.ctypes.data_as(c.c_void_p), W * S, H, -1, 0)
            if e:
                raise RuntimeError(f"render {e}")
        return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--fonts", default=",".join("0x%x" % k for k in G.FONTS))
    ap.add_argument("--chars", default="".join(chr(i) for i in range(0x21, 0x7f)))
    ap.add_argument("--interp", type=int, default=40)
    ap.add_argument("--xfac", type=int, default=1)
    ap.add_argument("--adv", default="linear")
    ap.add_argument("--round", default="none")
    ap.add_argument("--bias", type=int, default=0)
    ap.add_argument("--show", type=int, default=0)
    a = ap.parse_args()
    g = G.Gdi()
    m = Model(F.Probe(), a.interp, a.xfac, F.NO_BITMAP, a.adv, a.round, a.bias)
    tot_eq = tot_n = tot_d = 0
    for fk in a.fonts.split(","):
        face, lh = G.FONTS[int(fk, 16)]
        f = g.font(face, lh)
        eq = dl = rowbad = 0
        shown = 0
        for ch in a.chars:
            ref = L.levels(g, f, ch.encode("latin-1"))
            bw = sum(g.abc(f, ord(ch)))
            lv = F.box_levels(m.samples(F.FILES[face], -lh, ord(ch), bw))
            dd = int((lv != ref).sum())
            eq += dd == 0
            dl += dd
            rowbad += int(((lv.max(-1) > 0) != (ref.max(-1) > 0)).any(1).sum())
            if a.show and dd and shown < a.show:
                shown += 1
                dg, df = E.decode(ref), E.decode(lv)
                ys, xs = np.nonzero(np.maximum(lv.max(-1), ref.max(-1)))
                y0, y1, x0, x1 = ys.min(), ys.max() + 1, max(xs.min() - 1, 0), xs.max() + 2
                print(f"  {ch!r} bw {bw}  gdi / model")
                for y in range(y0, y1):
                    print("   %3d %s   %s" % (y - OY, E.line(dg, y, x0, x1) if dg is not None else "?",
                                             E.line(df, y, x0, x1)))
        tot_eq += eq
        tot_n += len(a.chars)
        tot_d += dl
        print(f"{fk} {face:16s} {-lh:3d} equal {eq:3d}/{len(a.chars)} levels differing {dl:6d} rows with ink mismatch"
              f" {rowbad}", flush=True)
    print(f"TOTAL equal {tot_eq}/{tot_n} levels differing {tot_d}")


if __name__ == "__main__":
    main()
