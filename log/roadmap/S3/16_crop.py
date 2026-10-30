"""S3.16: port | reference | diff crop of a framegate checkpoint, scaled.  16_crop.py <imgdir> <scenario> <checkpoint> x0 y0 x1 y1 [scale] out.png"""
import sys
import numpy as np
from PIL import Image
d, s, c, x0, y0, x1, y1 = sys.argv[1:8]
sc = int(sys.argv[8]) if len(sys.argv) > 9 else 3
out = sys.argv[-1]
x0, y0, x1, y1 = map(int, (x0, y0, x1, y1))
p = np.asarray(Image.open(f"{d}/{s}.{c}.port.png").convert("RGB"))
r = np.asarray(Image.open(f"log/c3/alt/refs/{s}/det_b/{c}.png").convert("RGB"))
m = ((p.astype(int) >> 3) != (r.astype(int) >> 3)).any(2)
D = np.zeros_like(p); D[m] = (255, 0, 255)
a = np.concatenate([p[y0:y1, x0:x1], r[y0:y1, x0:x1], D[y0:y1, x0:x1]], axis=1)
Image.fromarray(a).resize((a.shape[1] * sc, a.shape[0] * sc), Image.NEAREST).save(out)
