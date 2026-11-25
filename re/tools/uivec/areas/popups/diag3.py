"""Reading aid: art | render averaged to 1x | |difference| x3, nearest, enlarged. py -3.12 diag3.py <id> <state> [z]"""
import json
import sys

import numpy as np
from PIL import Image

import w3
import hs3  # noqa: F401
import common as c

w3.apply()
eid, st = sys.argv[1], sys.argv[2]
z = int(sys.argv[3]) if len(sys.argv) > 3 else 12
d = json.load(open(c.OUT + "/" + eid + ".json"))
rel = d["sheet"]
box = tuple(d["states"][st]["box"])
sc = d["states"][st]["scene"]
x0, y0, x1, y1 = box
s = 6
low = c.render_box(sc, box, s, c.FLAT).reshape(y1 - y0, s, x1 - x0, s, 3).mean(axis=(1, 3))
a = c.art(rel, box)
diff = np.clip(np.abs(low - a).max(axis=2, keepdims=True) * 3, 0, 255).repeat(3, 2)
row = np.concatenate([a, np.full((y1 - y0, 1, 3), 255.0), low, np.full((y1 - y0, 1, 3), 255.0), diff], 1)
big = np.kron(row, np.ones((z, z, 1)))
p = c.SCR + "/" + eid + "_" + st.replace(" ", "_") + "_diag.png"
Image.fromarray(np.clip(big + 0.5, 0, 255).astype(np.uint8)).save(p)
print(p)
