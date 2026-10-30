"""S3.16: contact sheet of the largest beyond-one-level cluster of each <dir> <scenario> <checkpoint> triple (port | ref | mask, x2)."""
import sys, importlib.util, pathlib
import numpy as np
from PIL import Image
spec = importlib.util.spec_from_file_location("d16", "log/roadmap/S3/16_diff.py"); D = importlib.util.module_from_spec(spec); spec.loader.exec_module(D)
out = sys.argv[1]; items = sys.argv[2:]
tiles = []
for i in range(0, len(items), 3):
    d, s, c = items[i:i + 3]
    p, r = D.load(d, s, c)
    m = (np.abs((p >> 3) - (r >> 3)).max(2) > 1)
    m[0x1F0:0x1F0+0x68, 0xD0:0xD0+0x250] |= False
    cl = sorted(D.clusters(m), key=lambda t: -len(t))
    if not cl: continue
    ys = [t[0] for t in cl[0]]; xs = [t[1] for t in cl[0]]
    x0, x1 = max(0, min(xs) - 30), min(800, max(xs) + 31); y0, y1 = max(0, min(ys) - 30), min(600, max(ys) + 31)
    M = np.zeros_like(p); M[m] = (255, 0, 255)
    a = np.concatenate([p[y0:y1, x0:x1], r[y0:y1, x0:x1], M[y0:y1, x0:x1]], 1).astype(np.uint8)
    a = np.asarray(Image.fromarray(a).resize((a.shape[1] * 2, a.shape[0] * 2), Image.NEAREST))
    print(f"{s}/{c}: {len(cl)} clusters, largest n={len(cl[0])} x{min(xs)}..{max(xs)} y{min(ys)}..{max(ys)}")
    tiles.append(a)
W = max(t.shape[1] for t in tiles); H = sum(t.shape[0] + 6 for t in tiles)
o = np.zeros((H, W, 3), np.uint8); y = 0
for t in tiles: o[y:y + t.shape[0], :t.shape[1]] = t; y += t.shape[0] + 6
Image.fromarray(o).save(out)
