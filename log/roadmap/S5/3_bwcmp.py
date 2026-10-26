"""S5.3: GDI ClearType level maps next to GDI's monochrome raster (lfQuality NONANTIALIASED_QUALITY 3) and FreeType's v35
hinted outline (x scale 1 and 6) of the same glyph. Usage: 3_bwcmp.py <fontref hex> <chars>"""
import importlib
import pathlib
import sys

import numpy as np

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
G = importlib.import_module("3_gdi")
L = importlib.import_module("3_gdi_levels")
F = importlib.import_module("3_fit")


def bw(g, font, text):
    a = G.chan32(g.draw(font, text, L.OX, L.OY, 0xffffff, 0, L.W, L.H, 32))[..., 1]
    return (a > 0).astype(int)


def rows(m, x0, x1, y0, y1, fmt):
    return [fmt(m, y, x0, x1) for y in range(y0, y1)]


if __name__ == "__main__":
    fk = int(sys.argv[1], 16)
    face, lh = G.FONTS[fk]
    g = G.Gdi()
    ct = g.font(face, lh)
    mono = g.font(face, lh, quality=3)
    pr = F.Probe()
    for ch in sys.argv[2]:
        lv = L.levels(g, ct, ch.encode("latin-1"))
        b = bw(g, mono, ch.encode("latin-1"))
        s1 = pr.samples(F.FILES[face], -lh, ord(ch), "v35")   # x scaled by 6 after v35 hinting
        ft1 = s1.reshape(L.H, L.W, 6).max(-1)   # any sample
        ys, xs = np.nonzero(lv.max(-1) + b)
        y0, y1, x0, x1 = ys.min(), ys.max() + 1, max(xs.min() - 1, 0), xs.max() + 2
        print(repr(ch), "abc", g.abc(ct, ord(ch)), "abc mono", g.abc(mono, ord(ch)))
        for y in range(y0, y1):
            a = " ".join("%d%d%d" % tuple(lv[y, x]) for x in range(x0, x1))
            bb = "".join("#" if b[y, x] else "." for x in range(x0, x1))
            ss = "".join("".join("#" if v else "." for v in s1[y, x * 6:(x + 1) * 6]) + "|" for x in range(x0, x1))
            print("  %3d %s   %s   %s" % (y - L.OY, a, bb, ss))
