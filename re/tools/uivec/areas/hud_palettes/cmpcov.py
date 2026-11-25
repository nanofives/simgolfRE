"""Round 3 check: the glyph's dark coverage, art vs a scene json, as text (per pixel; '#' both dark, 'A' art only, 'R' render only).
py -3.12 cmpcov.py <id> <json> <state> x0 x1 y0 y1"""
import json
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hp  # noqa: E402


def cov(img, bg, navy):
    lum = img @ np.array([0.3, 0.59, 0.11])
    return np.clip((bg - lum) / (bg - navy), 0, 1)


def main():
    el, js, st = sys.argv[1:4]
    xa, xb, ya, yb = map(int, sys.argv[4:8])
    d = json.load(open(js))
    c = {k[0]: k for k in hp.crops(el)}[st]
    _, src, x0, y0, rgb, mask = c
    h, w = mask.shape
    z = 6
    r = hp.render_state(d["states"][st]["scene"], x0, y0, w, h, z, art=hp.art_on_flat(rgb, mask))
    r = r.reshape(h, z, w, z, 3).mean((1, 3))
    bg = float(sys.argv[8]) if len(sys.argv) > 8 else 200.0
    ca = cov(rgb.astype(float), bg, 32)
    cr = cov(r, bg, 32)
    tot = 0
    print("      " + "".join(str(x % 10) for x in range(xa, xb)))
    for y in range(ya, yb):
        s = ""
        for x in range(xa, xb):
            a, b = ca[y - y0, x - x0], cr[y - y0, x - x0]
            tot += abs(a - b)
            if abs(a - b) < 0.25:
                s += "#" if a > 0.5 else "."
            else:
                s += "A" if a > b else "R"
        print(f"{y:4d}  {s}")
    print("sum |diff|", round(tot, 1))


if __name__ == "__main__":
    main()
