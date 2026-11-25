"""Glyph dump with 5 luminance levels: '@' < 70, '#' < 100, '+' < 135, ':' < 170, '.' else (P41 fix round)."""
import sys
import common as c
from common import fit


def gd(img, x, y, w, h, cuts=(70, 100, 135, 170)):
    L = fit.lum(img[y:y + h, x:x + w])
    ch = "@#+:."
    out = ["      " + "".join(str((x + i) % 10) for i in range(w))]
    for j in range(h):
        s = ""
        for v in L[j]:
            k = sum(v >= t for t in cuts)
            s += ch[k]
        out.append(f"{y + j:5d} " + s)
    return "\n".join(out)


if __name__ == "__main__":
    sheet, x, y, w, h = sys.argv[1], *map(int, sys.argv[2:6])
    print(gd(c.sheet(sheet)[0], x, y, w, h))
