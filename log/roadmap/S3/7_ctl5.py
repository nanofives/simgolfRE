"""S3.7: the pixels control 5 (Text_put's width to the frame code + 8) changes: plain against controlled framegate images
(log/roadmap/S3/7_ctl5/plain, c5), per checkpoint and in total.

    ~/bin/py -3.12 log/roadmap/S3/7_ctl5.py
"""
import os
import pathlib

import numpy as np
from PIL import Image

D = pathlib.Path(__file__).resolve().parent / "7_ctl5"
tot = 0
for p in sorted((D / "plain").glob("*.port.png")):
    q = D / "c5" / p.name
    if not q.exists():
        continue
    a, b = np.asarray(Image.open(p).convert("RGB")), np.asarray(Image.open(q).convert("RGB"))
    m = (a != b).any(axis=2)
    n = int(m.sum())
    tot += n
    if n:
        ys, xs = np.nonzero(m)
        print(p.name, n, "bbox", hex(xs.min()), hex(ys.min()), hex(xs.max()), hex(ys.max()))
print("total px changed by control 5:", tot)
