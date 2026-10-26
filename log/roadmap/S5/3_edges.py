"""S5.3: decode GDI ClearType level maps back to subpixel coverage. Under the box model (window k = 3 consecutive subpixels,
each subpixel = 2 samples), level(k) = c(k-1) + c(k) + c(k+1) over subpixel counts c in 0..2, so c is recovered left to
right from a zero margin. A decode that fails (c outside 0..2, or a nonzero remainder) disproves the box model for that glyph.
Prints GDI's subpixel counts next to FreeType variants'. Usage: 3_edges.py <fontref hex> <chars> [variants]"""
import importlib
import pathlib
import sys

import numpy as np

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
G = importlib.import_module("3_gdi")
L = importlib.import_module("3_gdi_levels")
F = importlib.import_module("3_fit")


def decode(lv):
    """(H, W, 3) levels -> (H, 3W) subpixel counts, or None"""
    h, w, _ = lv.shape
    seq = lv.reshape(h, 3 * w)   # window k centred on subpixel k
    c = np.zeros((h, 3 * w + 2), np.int32)   # c[:, k+1] = subpixel k
    for k in range(3 * w):
        # level(k) = c(k-1) + c(k) + c(k+1) -> c(k+1)
        if k + 2 < 3 * w + 2:
            c[:, k + 2] = seq[:, k] - c[:, k] - c[:, k + 1]
    if (c < 0).any() or (c > 2).any():
        return None
    return c[:, 1:-1]


def sub_counts(smp):
    h, ws = smp.shape
    return smp.reshape(h, ws // 2, 2).sum(-1)


def line(cnt, y, x0, x1):
    return "|".join("".join(".:#"[v] for v in cnt[y, 3 * x:3 * x + 3]) for x in range(x0, x1))


if __name__ == "__main__":
    fk = int(sys.argv[1], 16)
    variants = (sys.argv[3] if len(sys.argv) > 3 else "v35,v40,unhinted").split(",")
    face, lh = G.FONTS[fk]
    g = G.Gdi()
    ct = g.font(face, lh)
    pr = F.Probe()
    for ch in sys.argv[2]:
        lv = L.levels(g, ct, ch.encode("latin-1"))
        d = decode(lv)
        fts = [sub_counts(pr.samples(F.FILES[face], -lh, ord(ch), v)) for v in variants]
        if d is None:
            print(repr(ch), "DECODE FAILED")
            continue
        ys, xs = np.nonzero(lv.max(-1))
        y0, y1, x0, x1 = ys.min(), ys.max() + 1, max(xs.min() - 1, 0), xs.max() + 2
        print(repr(ch), "abc", g.abc(ct, ord(ch)), "   gdi  /  " + "  /  ".join(variants))
        for y in range(y0, y1):
            print("  %3d %s   %s" % (y - L.OY, line(d, y, x0, x1), "   ".join(line(f, y, x0, x1) for f in fts)))
