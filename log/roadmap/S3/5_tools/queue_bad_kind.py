"""S3.5: for each checkpoint the queue gate kept (log/roadmap/S3/5_img/queue/<scenario>.<checkpoint>.bad.png: scene pixels that
differ beyond one level), how many of those pixels are near-grey (channel spread <= 24) and brighter in the original than in
the port: the signature of a line the original draws over the sprites (Terrain::drawLine / drawMapLine / drawCircle), which
the port does not draw yet (S3.8)."""
import pathlib

import numpy as np
from PIL import Image

D = pathlib.Path(__file__).resolve().parents[1] / "5_img" / "queue"
for f in sorted(D.glob("*.bad.png")):
    stem = f.name[:-len(".bad.png")]
    b = np.asarray(Image.open(f)) > 0
    o = np.asarray(Image.open(D / f"{stem}.orig.png")).astype(int)
    p = np.asarray(Image.open(D / f"{stem}.port.png")).astype(int)
    ob, pb = o[b], p[b]
    grey = ob.max(1) - ob.min(1) <= 24
    brighter = ob.sum(1) > pb.sum(1)
    print(f"{stem}\tbad {int(b.sum())}\tgrey_brighter {int((grey & brighter).sum())}\tother {int((~(grey & brighter)).sum())}")
