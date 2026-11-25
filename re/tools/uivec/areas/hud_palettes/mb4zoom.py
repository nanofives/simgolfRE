"""Scratch zoom for hud-mode-base round 4: art vs a saved scene json, cropped to a sheet box, at z.

py -3.12 mb4zoom.py <scene.json> <out.png> x0 y0 x1 y1 z
"""
import json
import os
import sys

import numpy as np
from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hp  # noqa: E402


def main():
    js, out, bx0, by0, bx1, by1, z = sys.argv[1], sys.argv[2], *map(int, sys.argv[3:8])
    doc = json.load(open(js))
    st, src, x0, y0, rgb, mask = hp.crops("hud-mode-base")[0]
    scene = doc["states"][st]["scene"]
    w, h = bx1 - bx0, by1 - by0
    art = hp.art_on_flat(rgb, mask)[by0 - y0:by1 - y0, bx0 - x0:bx1 - x0]
    a = np.asarray(Image.fromarray(art.astype(np.uint8)).resize((w * z, h * z), Image.NEAREST), float)
    r = hp.render_state(scene, bx0, by0, w, h, z)
    row = np.zeros((h * z, 2 * w * z + 8, 3)) + 30
    row[:, :w * z] = a
    row[:, w * z + 8:] = r
    Image.fromarray(np.clip(row + 0.5, 0, 255).astype(np.uint8)).save(out)


if __name__ == "__main__":
    main()
