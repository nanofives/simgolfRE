"""S5.3: the ClearType model with vertical oversampling (GDI's y-direction smoothing, fonts without a gasp table). FreeType
loads the glyph (interpreter, y hinting mode), scales x by 6 and y by V, places it with 3_fit2's compatible-width shift
(+ --bias), scans it with the monochrome rasterizer at 6 x V samples per pixel; level = rounding(count in the 6 x V box / V).

    ~/bin/py -3.12 log/roadmap/S5/3_fity.py --fonts 0x51b360 [--v 1,2,4,6,8] [--ymode ppem|yover|unhinted]
                                           [--lround round|floor|ceil] [--interp 40] [--bias N] [--show N] [--sweep]
--sweep reports, per glyph, whether some bias in -64..64 (step 4) makes it exact (a placement error, not a shape error)."""
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

OX, OY, W, H, S = L.OX, L.OY, L.W, L.H, 6


class YModel:
    def __init__(self, pr, v=6, ymode="ppem", lround="round", interp=40, bias=0, hint=True, xmode="center"):
        self.pr, self.v, self.ymode, self.lround, self.interp, self.bias, self.hint = pr, v, ymode, lround, interp, bias, hint
        self.xmode = xmode
        pr.d.pr_render2.argtypes = [c.c_void_p, c.c_long, c.c_long, c.c_long, c.c_long, c.c_void_p, c.c_int, c.c_int]

    def levels(self, path, ppem, ch, bw):
        d, V = self.pr.d, self.v
        f = self.pr.face(path, f"y{self.interp}")
        d.pr_interp(self.interp)
        gid = d.pr_char(f, ch)
        a = c.c_int()
        d.pr_size(f, ppem, ppem)
        d.pr_load(f, gid, F.NO_BITMAP | F.NO_HINTING, c.byref(a), None, None)
        sh = int(np.floor((bw * S * 64 - a.value * S) / 2 + 0.5)) + self.bias
        sx = S << 16
        if self.xmode == "scale" and a.value > 0:
            sx = int(S * 65536 * bw * 64 / a.value)
            sh = self.bias
        if self.xmode == "none":
            sh = self.bias
        if self.xmode == "cround":   # (BW - natural advance rounded to whole pixels) / 2, in samples
            sh = (bw - (a.value + 32) // 64) * S * 32 + self.bias
        flags = F.NO_BITMAP | (0 if self.hint and self.ymode != "unhinted" else F.NO_HINTING)
        sy = V
        xfac = getattr(self, "xfac", 1)
        if self.ymode == "yover":
            d.pr_size(f, xfac * ppem, V * ppem)
            sy = 1
        else:
            d.pr_size(f, xfac * ppem, ppem)
        sx //= xfac
        npts = c.c_int()
        if d.pr_load(f, gid, flags, c.byref(a), c.byref(npts), None):
            raise RuntimeError("load")
        smp = np.zeros((H * V, W * S), np.uint8)
        if npts.value > 0:
            e = d.pr_render2(f, sx, sy << 16, OX * S * 64 + sh, (H - OY) * V * 64,
                             smp.ctypes.data_as(c.c_void_p), W * S, H * V)
            if e:
                raise RuntimeError(f"render {e}")
        cnt = F.box_levels(smp).reshape(H, V, W, 3).sum(1)   # 0 .. 6V
        if self.lround == "round":
            return (2 * cnt + V) // (2 * V)
        if self.lround == "floor":
            return cnt // V
        if self.lround == "ceil":
            return (cnt + V - 1) // V
        raise ValueError(self.lround)


def run(g, pr, fk, chars, v, ymode, lround, interp, bias, show=0, sweep=False, xmode="center"):
    face, lh = G.FONTS[int(fk, 16)]
    f = g.font(face, lh)
    eq = dl = fixable = 0
    shown = 0
    m = YModel(pr, v, ymode, lround, interp, bias, xmode=xmode)
    for ch in chars:
        ref = L.levels(g, f, ch.encode("latin-1"))
        bw = sum(g.abc(f, ord(ch)))
        lv = m.levels(F.FILES[face], -lh, ord(ch), bw)
        dd = int((lv != ref).sum())
        eq += dd == 0
        dl += dd
        if dd and sweep:
            for b in range(-64, 65, 4):
                mm = YModel(pr, v, ymode, lround, interp, bias + b, xmode=xmode)
                if not (mm.levels(F.FILES[face], -lh, ord(ch), bw) != ref).any():
                    fixable += 1
                    break
        if show and dd and shown < show:
            shown += 1
            ys, xs = np.nonzero(np.maximum(lv.max(-1), ref.max(-1)))
            y0, y1, x0, x1 = ys.min(), ys.max() + 1, max(xs.min() - 1, 0), xs.max() + 2
            print(f"  {ch!r} bw {bw}  gdi / model")
            for y in range(y0, y1):
                print("   %3d %s   %s" % (y - OY, " ".join("%d%d%d" % tuple(ref[y, x]) for x in range(x0, x1)),
                                         " ".join("%d%d%d" % tuple(lv[y, x]) for x in range(x0, x1))))
    return eq, dl, fixable, len(chars)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--fonts", default="0x51b360")
    ap.add_argument("--chars", default="".join(chr(i) for i in range(0x21, 0x7f)))
    ap.add_argument("--v", default="1,2,3,4,5,6,8,16")
    ap.add_argument("--ymode", default="ppem")
    ap.add_argument("--lround", default="round")
    ap.add_argument("--interp", type=int, default=40)
    ap.add_argument("--bias", type=int, default=0)
    ap.add_argument("--show", type=int, default=0)
    ap.add_argument("--sweep", action="store_true")
    ap.add_argument("--xmode", default="center")
    a = ap.parse_args()
    g = G.Gdi()
    pr = F.Probe()
    for fk in a.fonts.split(","):
        for ym in a.ymode.split(","):
            for lr in a.lround.split(","):
                for v in [int(x) for x in a.v.split(",")]:
                    eq, dl, fx, n = run(g, pr, fk, a.chars, v, ym, lr, a.interp, a.bias, a.show, a.sweep, a.xmode)
                    print(f"{fk} ymode {ym:8s} lround {lr:5s} V {v:2d}: equal {eq:3d}/{n} levels differing {dl:6d}"
                          + (f" fixable by a shift {fx}" if a.sweep else ""), flush=True)


if __name__ == "__main__":
    main()
