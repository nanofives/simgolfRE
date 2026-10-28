"""S3.5b: classify the scene pixels the final queue gate kept (5b_img/queue/<scenario>.<checkpoint>.bad.png, scene pixels beyond one
level of the original's root at renderSpriteQueue's return) by what the original has there:
  green   the original's pixel is clearly green (g > r + 40 and g > b + 40): the golfer-selection ring Terrain::drawLine draws
  grey    a near-grey (channel spread <= 24) pixel brighter in the original: a white / grey line (path line, range rings)
  other   anything else (looked at one by one in 5b_notes.md)
    ~/bin/py -3.12 log/roadmap/S3/5b_queue_kind.py > log/roadmap/S3/5b_queue_kind.tsv"""
import pathlib

import numpy as np
from PIL import Image

D = pathlib.Path(__file__).resolve().parent / "5b_img" / "queue"
print("checkpoint\tbad\tgreen\tgrey\tother")
for f in sorted(D.glob("*.bad.png")):
    stem = f.name[:-len(".bad.png")]
    b = np.asarray(Image.open(f)) > 0
    o = np.asarray(Image.open(D / f"{stem}.orig.png")).astype(int)
    p = np.asarray(Image.open(D / f"{stem}.port.png")).astype(int)
    ob, pb = o[b], p[b]
    green = (ob[:, 1] > ob[:, 0] + 40) & (ob[:, 1] > ob[:, 2] + 40)
    grey = (ob.max(1) - ob.min(1) <= 24) & (ob.sum(1) > pb.sum(1)) & ~green
    other = ~(green | grey)
    print(f"{stem}\t{int(b.sum())}\t{int(green.sum())}\t{int(grey.sum())}\t{int(other.sum())}")
