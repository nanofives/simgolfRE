"""S5.3: how one TextOutA call combines glyphs whose ink overlaps. Compares the string drawn in one call with (a) the glyphs
drawn one call each, in order, at the same pen positions (sequential blending) on the 16-bit DIB, over several colour /
background pairs. Usage: 3_overlap.py"""
import importlib
import pathlib
import sys

import numpy as np

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
G = importlib.import_module("3_gdi")

CASES = [(0x51b360, b"fjjff/\\"), (0x51a028, b"WaVAfjTo"), (0x519fd8, b"AVfjTy"), (0x51b360, b"Hello World"),
         (0x519a40, b"AVWAfj")]
COLS = [(0xffffff, 0x0000), (0x000000, 0x7fff), (0x4080c0, 0x4210), (0xf8f8f8, 0x1234)]

if __name__ == "__main__":
    g = G.Gdi()
    for fk, text in CASES:
        face, lh = G.FONTS[fk]
        f = g.font(face, lh)
        for cref, bg in COLS:
            one = g.draw(f, text, 20, 40, cref, bg, 200, 56, 16)
            # sequential: draw glyph i at pen x + extent(text[:i])
            surf = np.full((56, 200), bg, np.uint16)
            for i in range(len(text)):
                x = 20 + g.extent(f, text[:i])[0]
                surf = g.draw(f, text[i:i + 1], x, 40, cref, surf, 200, 56, 16)
            d = int((one != surf).sum())
            print(f"0x{fk:x} {text!r} colour 0x{cref:06x} bg 0x{bg:04x}: one call vs glyph by glyph differ in {d} px"
                  f" (ink px {int((one != bg).sum())})")


def combine_check():
    """levels of the one-call string against the per-glyph level maps combined by max and by min(sum, 6)"""
    L = importlib.import_module("3_gdi_levels")
    g = G.Gdi()
    for fk, text in CASES:
        face, lh = G.FONTS[fk]
        f = g.font(face, lh)
        one = L.levels(g, f, text, 20, 40, 200, 56)
        per = [L.levels(g, f, text[i:i + 1], 20 + g.extent(f, text[:i])[0], 40, 200, 56) for i in range(len(text))]
        mx = np.max(per, 0)
        sm = np.minimum(np.sum(per, 0), 6)
        over = (np.array(per) > 0).sum(0) > 1
        print(f"0x{fk:x} {text!r}: overlap levels {int(over.sum())}; one-call != max {int((one != mx).sum())},"
              f" != clamp-sum {int((one != sm).sum())}")


if __name__ == "__main__" and "--combine" in sys.argv:
    combine_check()
