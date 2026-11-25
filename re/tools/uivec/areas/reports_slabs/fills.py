"""List the flat light fill components of a sheet inside a box (to find plate / pill / cell fills).
Usage: fills.py <sheet> [x0 y0 x1 y1] [min_area]"""
import sys
import numpy as np
import common as C
import boxes

sh = sys.argv[1]
box = [int(v) for v in sys.argv[2:6]] if len(sys.argv) >= 6 else [0, 0, 800, 600]
mn = int(sys.argv[6]) if len(sys.argv) > 6 else 40
_, al, raw = C.art(sh)
key = (raw[..., 0].astype(np.int32) << 16) | (raw[..., 1].astype(np.int32) << 8) | raw[..., 2].astype(np.int32)
full = al > 0.99
sub = np.zeros_like(full)
sub[box[1]:box[3], box[0]:box[2]] = True
vals, counts = np.unique(key[full & sub], return_counts=True)
for v, n in zip(vals, counts):
    if n < mn:
        continue
    col = (int(v >> 16), int((v >> 8) & 255), int(v & 255))
    if C.fit.lum(np.array(col, float)) < 120 and max(col) - min(col) < 120:
        continue
    for comp in boxes.components((key == v) & full & sub, mn):
        x0, y0, x1, y1, r, iou = boxes.fit_rrect(comp)
        print(col, (x0, y0, x1, y1), f"{x1 - x0}x{y1 - y0}", "r", r, "fit", round(iou, 3), "n", len(comp))
