"""S5.3: fit the ClearType coverage geometry. For each corpus font and each printable byte, the GDI level map (white on
black, 32 bpp, 3_gdi_levels.levels) against FreeType variants (3_probe s5probe.dll): the outline hinted in a mode, scaled
horizontally by 6 (or hinted at 6 x ppem in x), scanned by FreeType's monochrome rasterizer at 6 samples per pixel, each
subpixel level = set samples in a 6-sample box centred on it (R [6p-2, 6p+4), G [6p, 6p+6), B [6p+2, 6p+8)).

    ~/bin/py -3.12 log/roadmap/S5/3_fit.py [--variants a,b,..] [--fonts 0x51b360,...] [--chars ...] [--show N]

Prints per variant and font the glyphs equal at shift 0, equal at the best shift, and the differing level count."""
import argparse
import ctypes as c
import importlib
import pathlib
import sys

import numpy as np

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parents[2]
sys.path.insert(0, str(HERE))
G = importlib.import_module("3_gdi")
L = importlib.import_module("3_gdi_levels")

FILES = {"Arial Bold": "C:/Windows/Fonts/arial.ttf", "Manual SSi Bold": str(ROOT / "original" / "manu3_.TTF"),
         "Manual SSi": str(ROOT / "original" / "manu3_.TTF"), "Klepto ITC TT": str(ROOT / "original" / "KLEPTO__.TTF")}
OX, OY, W, H = L.OX, L.OY, L.W, L.H
S = 6

NO_HINTING, NO_BITMAP = 2, 8
T_NORMAL, T_LIGHT, T_MONO, T_LCD = 0, 1 << 16, 2 << 16, 3 << 16

# name: (interpreter, load flags, x ppem factor (outline hinted at f * ppem in x), x scale after loading)
VARIANTS = {
    "v35": (35, NO_BITMAP | T_NORMAL, 1, 6),
    "v35mono": (35, NO_BITMAP | T_MONO, 1, 6),
    "v40": (40, NO_BITMAP | T_NORMAL, 1, 6),
    "v40lcd": (40, NO_BITMAP | T_LCD, 1, 6),
    "unhinted": (40, NO_BITMAP | NO_HINTING, 1, 6),
    "v35x6": (35, NO_BITMAP | T_NORMAL, 6, 1),
    "v40x6": (40, NO_BITMAP | T_NORMAL, 6, 1),
}


class Probe:
    def __init__(self, path=ROOT / "port" / "build" / "S5_3_probe" / "Release" / "s5probe.dll"):
        d = self.d = c.CDLL(str(path))
        d.pr_open.restype = c.c_void_p
        d.pr_open.argtypes = [c.c_char_p, c.c_int]
        d.pr_size.argtypes = [c.c_void_p, c.c_int, c.c_int]
        d.pr_char.argtypes = [c.c_void_p, c.c_uint]
        d.pr_char.restype = c.c_uint
        d.pr_load.argtypes = [c.c_void_p, c.c_uint, c.c_int, c.POINTER(c.c_int), c.POINTER(c.c_int),
                              c.POINTER(c.c_int)]
        d.pr_render.argtypes = [c.c_void_p, c.c_long, c.c_long, c.c_long, c.c_int, c.c_void_p, c.c_int, c.c_int,
                                c.c_int, c.c_int]
        self.faces = {}

    def face(self, path, variant=""):
        if (path, variant) not in self.faces:
            self.faces[(path, variant)] = self.d.pr_open(path.encode(), 0)
        return self.faces[(path, variant)]

    def samples(self, path, ppem, ch, variant, shift=0):
        """(H, W * S) 0/1 samples with the glyph origin at pixel (OX, OY); shift in samples"""
        interp, flags, fx, sx = VARIANTS[variant]
        f = self.face(path, variant)
        self.d.pr_interp(interp)
        self.d.pr_size(f, fx * ppem, ppem)
        g = self.d.pr_char(f, ch)
        adv, npts, ofl = c.c_int(), c.c_int(), c.c_int()
        if self.d.pr_load(f, g, flags, c.byref(adv), c.byref(npts), c.byref(ofl)):
            raise RuntimeError("load")
        out = np.zeros((H, W * S), np.uint8)
        if npts.value > 0:
            e = self.d.pr_render(f, sx << 16, (OX * S + shift) * 64, (H - OY) * 64, 0,
                                 out.ctypes.data_as(c.c_void_p), W * S, H, -1, 0)
            if e:
                raise RuntimeError(f"render {e}")
        return out


def box_levels(smp):
    """(H, W*S) samples -> (H, W, 3) levels"""
    h, ws = smp.shape
    w = ws // S
    pad = np.zeros((h, ws + 2 * S), np.int32)
    pad[:, S:S + ws] = smp
    cs = np.concatenate([np.zeros((h, 1), np.int32), np.cumsum(pad, 1)], 1)   # cs[k] = sum pad[:k]
    out = np.zeros((h, w, 3), np.int32)
    for ch in range(3):
        st = np.arange(w) * S + 2 * ch - 2 + S
        out[..., ch] = cs[:, st + S] - cs[:, st]
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--variants", default=",".join(VARIANTS))
    ap.add_argument("--fonts", default=",".join("0x%x" % k for k in G.FONTS))
    ap.add_argument("--chars", default="".join(chr(i) for i in range(0x21, 0x7f)))
    ap.add_argument("--shifts", type=int, default=6)
    ap.add_argument("--show", type=int, default=0)
    a = ap.parse_args()
    g = G.Gdi()
    pr = Probe()
    for fk in a.fonts.split(","):
        face, lh = G.FONTS[int(fk, 16)]
        f = g.font(face, lh)
        ref = {ch: L.levels(g, f, ch.encode("latin-1")) for ch in a.chars}
        for v in a.variants.split(","):
            eq0 = eqb = 0
            diff0 = 0
            shifts = {}
            shown = 0
            for ch in a.chars:
                best = None
                for s in range(-a.shifts, a.shifts + 1):
                    lv = box_levels(pr.samples(FILES[face], -lh, ord(ch), v, s))
                    d = int((lv != ref[ch]).sum())
                    if s == 0:
                        diff0 += d
                        eq0 += d == 0
                        d0lv = lv
                    if best is None or d < best[0]:
                        best = (d, s)
                eqb += best[0] == 0
                shifts[best[1]] = shifts.get(best[1], 0) + 1
                if a.show and shown < a.show and int((d0lv != ref[ch]).sum()):
                    shown += 1
                    print(f"  {ch!r} gdi / ft (shift 0)")
                    L.show(ref[ch])
                    L.show(d0lv)
            print(f"{fk} {face} {-lh} {v:9s} equal@0 {eq0}/{len(a.chars)} equal@best {eqb} levels differing@0 {diff0}"
                  f" best shifts {sorted(shifts.items())}", flush=True)


if __name__ == "__main__":
    main()
