"""S3.16: x / y histogram (bins of 8 px) of the pixels a framegate port image differs in from its reference."""
import sys, collections, pathlib
import numpy as np
from PIL import Image
d, s, c = sys.argv[1:4]
p = np.asarray(Image.open(pathlib.Path(d) / f"{s}.{c}.port.png").convert("RGB")).astype(int) >> 3
r = np.asarray(Image.open(f"log/c3/alt/refs/{s}/det_b/{c}.png").convert("RGB")).astype(int) >> 3
m = (p != r).any(2)
ys, xs = np.nonzero(m)
print(s, c, len(xs))
print("x", sorted(collections.Counter(int(x) // 8 * 8 for x in xs).items()))
print("y", sorted(collections.Counter(int(y) // 8 * 8 for y in ys).items()))
