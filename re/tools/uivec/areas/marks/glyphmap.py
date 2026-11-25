"""Print a medal's dark-glyph map: pixels darker than their 7x7 neighbourhood mean ('#' > 22, '+' > 10)."""
import sys
import numpy as np
import common as C
import fit


def box(a, k):
    p = np.pad(a, k, mode='edge')
    s = np.zeros_like(a)
    for dy in range(-k, k + 1):
        for dx in range(-k, k + 1):
            s += p[k + dy:k + dy + a.shape[0], k + dx:k + dx + a.shape[1]]
    return s / (2 * k + 1) ** 2


def show(name, r):
    art, m = C.crop(C.CI, *r)
    L = fit.lum(art)
    d = box(L, 3) - L
    print(name)
    print('   ' + ''.join(str(i % 10) for i in range(art.shape[1])))
    for j in range(art.shape[0]):
        print(f'{j:2d} ' + ''.join(('#' if d[j, i] > 22 else '+' if d[j, i] > 10 else '.') if m[j, i] else ' '
                                  for i in range(art.shape[1])))


if __name__ == "__main__":
    import el_medals
    for k in sys.argv[1:]:
        show(k + " a", el_medals.CELLS[k][0])
        show(k + " b", el_medals.CELLS[k][1])
