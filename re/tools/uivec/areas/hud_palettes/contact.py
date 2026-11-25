"""Contact sheet of several elements' cuts.  py -3.12 contact.py <z> <out.png> <id>..."""
import os
import sys

import numpy as np
from PIL import Image, ImageDraw

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hp  # noqa: E402


def main():
    z = int(sys.argv[1])
    out = sys.argv[2]
    tiles = []
    for el in sys.argv[3:]:
        for st, src, x0, y0, rgb, mask in hp.crops(el):
            h, w = mask.shape
            im = Image.fromarray(hp.art_on_flat(rgb, mask).astype(np.uint8)).resize((w * z, h * z), Image.NEAREST)
            tiles.append((f"{el}:{st}", im))
    W = 1600
    x = y = rowh = 0
    pos = []
    for lab, im in tiles:
        if x + im.width > W:
            x, y, rowh = 0, y + rowh + 14, 0
        pos.append((x, y))
        x += im.width + 6
        rowh = max(rowh, im.height)
    can = Image.new("RGB", (W, y + rowh + 14), (20, 20, 20))
    d = ImageDraw.Draw(can)
    for (lab, im), (x, y) in zip(tiles, pos):
        can.paste(im, (x, y + 12))
        d.text((x, y), lab[:max(8, im.width // 6)], fill=(255, 255, 0))
    can.save(out)
    print(out, can.size)


if __name__ == "__main__":
    main()
