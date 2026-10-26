"""S5.3: GDI ClearType level maps per glyph. White (0xffffff) TextOutA on a black 32-bit DIB (BkMode TRANSPARENT,
TA_BASELINE) gives per channel one of the 7 values 0 58 102 144 182 219 255 (= levels 0..6, first worker's e1 probe);
this maps them back to levels. Usage: 3_gdi_levels.py <face> <height> [chars] -> prints the maps."""
import importlib
import pathlib
import sys

import numpy as np

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
G = importlib.import_module("3_gdi")

LV = {0: 0, 58: 1, 102: 2, 144: 3, 182: 4, 219: 5, 255: 6}
OX, OY, W, H = 16, 40, 96, 56


def levels(g, font, text: bytes, ox=OX, oy=OY, w=W, h=H):
    a = G.chan32(g.draw(font, text, ox, oy, 0xffffff, 0x000000, w, h, 32))
    out = np.zeros(a.shape, np.int32)
    for v, l in LV.items():
        out[a == v] = l
    bad = ~np.isin(a, list(LV))
    if bad.any():
        raise RuntimeError(f"unexpected values {np.unique(a[bad])}")
    return out   # (h, w, 3) r g b levels


def show(lv):
    ys, xs = np.nonzero(lv.max(-1))
    if not len(ys):
        print("  (empty)")
        return
    y0, y1, x0, x1 = ys.min(), ys.max() + 1, xs.min(), xs.max() + 1
    print(f"  box x {x0 - OX}..{x1 - OX} y {y0 - OY}..{y1 - OY}")
    for y in range(y0, y1):
        print("  %4d " % (y - OY) + " ".join("%d%d%d" % tuple(lv[y, x]) for x in range(x0, x1)))


if __name__ == "__main__":
    face, height = sys.argv[1], int(sys.argv[2])
    chars = sys.argv[3] if len(sys.argv) > 3 else "lIHo"
    g = G.Gdi()
    f = g.font(face, -height)
    for ch in chars:
        print(repr(ch), "abc", g.abc(f, ord(ch)))
        show(levels(g, f, ch.encode("latin-1")))
