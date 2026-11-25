"""Reading aid: art | shapes crops of a work3 element at a higher zoom. py -3.12 detail3.py <id> <z> <state:x0,y0,x1,y1> ..."""
import json
import sys

import numpy as np
from PIL import Image

import w3
import hs3  # noqa: F401  (registers popups.sinter)
import common as c

w3.apply()
eid, z = sys.argv[1], int(sys.argv[2])
d = json.load(open(c.OUT + "/" + eid + ".json"))
rel = d["sheet"]
tiles = []
for spec in sys.argv[3:]:
    st, b = spec.split(":")
    sub = tuple(int(v) for v in b.split(","))
    sc = d["states"][st]["scene"]
    a = np.kron(c.art(rel, sub), np.ones((z, z, 1)))
    r = c.render_box(sc, sub, z, c.FLAT)
    tiles.append(np.concatenate([a, np.full((a.shape[0], 6, 3), 255.0), r], 1))
W = max(t.shape[1] for t in tiles)
im = np.concatenate([np.pad(t, ((0, 6), (0, W - t.shape[1]), (0, 0)), constant_values=255) for t in tiles])
p = c.SCR + "/" + eid + "_detail.png"
Image.fromarray(np.clip(im, 0, 255).astype(np.uint8)).save(p)
print(p)
