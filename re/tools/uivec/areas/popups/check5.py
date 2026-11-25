"""P41 popups round 5: the 8x check, every relit button beside three round-2 siblings, idle and hover (work5/popups/_read)."""
import json
import os
import sys

import numpy as np
from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import common as c      # noqa: E402
import infobuttons as ib  # noqa: E402
import rel5             # noqa: E402
import w5               # noqa: E402

w5.apply()
rows = []
for st_i, st in ((0, "idle"), (1, "hover")):
    tiles = []
    for k in ("chart", "checklist", "notice", "pair", "flag", "money", "tiles"):
        src = os.path.join(c.OUT if k in rel5.KEYS5 else rel5.R2, f"info-button-{k}.json")
        d = json.load(open(src))
        r = ib.KEYS.index(k)
        tiles.append(c.render_box(d["states"][st]["scene"], ib.cell(r, st_i), 8, c.FLAT))
        tiles.append(np.full((tiles[-1].shape[0], 6, 3), 255.0))
    rows.append(np.concatenate(tiles, axis=1))
img = np.concatenate([rows[0], np.full((6, rows[0].shape[1], 3), 255.0), rows[1]])
w5.check()
p = os.path.join(c.SCR, "check8x.png")
Image.fromarray(np.clip(img + 0.5, 0, 255).astype(np.uint8)).save(p)
print(p)
