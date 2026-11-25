"""Round 3 look tool: art and a scene json side by side, contrast stretched so faint tones show, with a pixel grid.

py -3.12 stretch.py <id> <scene json> <out.png> [z] [lo] [gain] [state...]
"""
import json
import os
import sys

import numpy as np
from PIL import Image, ImageDraw

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hp  # noqa: E402


def stretch(img, lo, gain):
    lum = img.astype(float) @ np.array([0.3, 0.59, 0.11])
    s = np.clip((lum - lo) * gain, 0, 255)
    return np.stack([s] * 3, -1)


def grid(im, z, x0, y0, w, h):
    d = ImageDraw.Draw(im)
    for g in range(0, max(w, h) + 1):
        if g <= w and (x0 + g) % 5 == 0:
            d.line([(g * z, 0), (g * z, h * z)], fill=(255, 0, 0) if (x0 + g) % 10 == 0 else (110, 50, 50))
            if (x0 + g) % 10 == 0:
                d.text((g * z + 2, 2), str(x0 + g), fill=(255, 255, 0))
        if g <= h and (y0 + g) % 5 == 0:
            d.line([(0, g * z), (w * z, g * z)], fill=(255, 0, 0) if (y0 + g) % 10 == 0 else (110, 50, 50))
            if (y0 + g) % 10 == 0:
                d.text((2, g * z + 2), str(y0 + g), fill=(255, 255, 0))
    return im


def main():
    el, js, out = sys.argv[1:4]
    z = int(sys.argv[4]) if len(sys.argv) > 4 else 8
    lo = float(sys.argv[5]) if len(sys.argv) > 5 else 95
    gain = float(sys.argv[6]) if len(sys.argv) > 6 else 5
    want = sys.argv[7:]
    doc = json.load(open(js))
    rows = []
    for st, src, x0, y0, rgb, mask in hp.crops(el):
        if want and st not in want:
            continue
        h, w = mask.shape
        r = hp.render_state(doc["states"][st]["scene"], x0, y0, w, h, z, art=hp.art_on_flat(rgb, mask))
        a = np.asarray(Image.fromarray(hp.art_on_flat(rgb, mask).astype(np.uint8)).resize((w * z, h * z), Image.NEAREST), float)
        A = grid(Image.fromarray(stretch(a, lo, gain).astype(np.uint8)), z, x0, y0, w, h)
        R = grid(Image.fromarray(stretch(r, lo, gain).astype(np.uint8)), z, x0, y0, w, h)
        row = Image.new("RGB", (2 * w * z + 8, h * z), (0, 0, 60))
        row.paste(A, (0, 0))
        row.paste(R, (w * z + 8, 0))
        rows.append(row)
    W = max(r.width for r in rows)
    H = sum(r.height for r in rows) + 6 * (len(rows) - 1)
    can = Image.new("RGB", (W, H), (0, 0, 60))
    y = 0
    for r in rows:
        can.paste(r, (0, y))
        y += r.height + 6
    can.save(out)
    print(out, can.size)


if __name__ == "__main__":
    main()
