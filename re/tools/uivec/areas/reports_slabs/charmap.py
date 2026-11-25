"""A character map of a sheet box: ' ' outside, '#' navy, '+' dark shade, 'd' darker lilac, '.' slab lilac, 'l' light
lilac, 'p' pill lilac, 'c' cream, 't' tan, 'w' white-ish, '?' other.  Usage: charmap.py <sheet> x y w h [step]"""
import sys
import numpy as np
import common as C


def cls(c, a):
    if a < 0.5:
        return " "
    r, g, b = c
    L = 0.299 * r + 0.587 * g + 0.114 * b
    if L < 60:
        return "#"
    if L < 100:
        return "+"
    if b > r + 25:      # lilac family
        if L < 135:
            return "d"
        if L < 160:
            return "."
        if L < 185:
            return "l"
        return "p"
    if r > 230 and g > 225:
        return "c" if b < 238 else "w"
    if r > 180 and b < 200:
        return "t"
    return "?"


if __name__ == "__main__":
    sh = sys.argv[1]
    x, y, w, h = (int(v) for v in sys.argv[2:6])
    st = int(sys.argv[6]) if len(sys.argv) > 6 else 1
    _, al, raw = C.art(sh)
    print("     " + "".join(str((x + i) // 10 % 10) if (x + i) % 10 < st else " " for i in range(0, w, st)))
    for j in range(y, y + h, st):
        print("%4d " % j + "".join(cls(raw[j, i], al[j, i]) for i in range(x, x + w, st)))
