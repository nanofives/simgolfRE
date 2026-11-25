"""A cut as a character map of its colours.  py -3.12 cmap.py <id> <state> [x0 y0 x1 y1] [--k N]
Colours are clustered (k-means in RGB, default 9); each cluster gets a letter, listed with its mean colour and lum."""
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hp  # noqa: E402


def kmeans(X, k, it=30):
    rng = np.random.default_rng(0)
    # init: farthest point
    C = [X[np.argmin(X.sum(1))]]
    for _ in range(k - 1):
        d = np.min([((X - c) ** 2).sum(1) for c in C], axis=0)
        C.append(X[np.argmax(d)])
    C = np.array(C, float)
    for _ in range(it):
        lab = np.argmin(((X[:, None] - C[None]) ** 2).sum(-1), 1)
        for i in range(k):
            if (lab == i).any():
                C[i] = X[lab == i].mean(0)
    return C, lab


def main():
    a = sys.argv[1:]
    k = 9
    if "--k" in a:
        i = a.index("--k")
        k = int(a[i + 1])
        a = a[:i] + a[i + 2:]
    el, st = a[0], a[1]
    c = [c for c in hp.crops(el) if c[0] == st][0]
    _, src, x0, y0, rgb, mask = c
    h, w = mask.shape
    bx0, by0, bx1, by1 = (int(v) for v in a[2:6]) if len(a) >= 6 else (x0, y0, x0 + w, y0 + h)
    sub = rgb[by0 - y0:by1 - y0, bx0 - x0:bx1 - x0].astype(float)
    m = mask[by0 - y0:by1 - y0, bx0 - x0:bx1 - x0]
    X = sub[m]
    C, lab = kmeans(X, min(k, len(np.unique(X, axis=0))))
    order = np.argsort(C @ [0.299, 0.587, 0.114])
    letters = "0123456789abcdefghij"
    name = {o: letters[i] for i, o in enumerate(order)}
    L = np.full(m.shape, " ", object)
    L[m] = [name[v] for v in lab]
    for i, o in enumerate(order):
        print(f"  {letters[i]}: {[int(v) for v in C[o]]} lum {C[o] @ [0.299, 0.587, 0.114]:.0f}")
    print("     " + "".join(str(x // 10 % 10) for x in range(bx0, bx1)))
    print("     " + "".join(str(x % 10) for x in range(bx0, bx1)))
    for j in range(L.shape[0]):
        print(f"{by0 + j:4d} " + "".join(L[j]))


if __name__ == "__main__":
    main()
