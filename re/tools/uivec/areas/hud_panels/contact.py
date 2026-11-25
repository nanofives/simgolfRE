"""P41 hud_panels: the first state row of several comparison images side by side (for a quick look).
Usage: py -3.12 re/tools/uivec/areas/hud_panels/contact.py out.png id [id ...]"""
import os
import sys

import numpy as np
from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import common as C  # noqa: E402

out, ids = sys.argv[1], sys.argv[2:]
tiles = []
for el in ids:
    im = np.asarray(Image.open(os.path.join(C.OUT, el + ".png")).convert("RGB"))
    cells = C.CELLS[el]
    h = cells[0][2][3]
    n = len(cells)
    hh = (im.shape[0] + 6) // n - 6 if n > 1 else im.shape[0]
    tiles.append(im[:hh])
H = max(t.shape[0] for t in tiles)
row = np.concatenate([np.pad(t, ((0, H - t.shape[0]), (0, 10), (0, 0)), constant_values=255) for t in tiles], 1)
s = min(1.0, 1900 / row.shape[1])
Image.fromarray(row).resize((int(row.shape[1] * s), int(row.shape[0] * s)), Image.LANCZOS).save(out)
print(out)
