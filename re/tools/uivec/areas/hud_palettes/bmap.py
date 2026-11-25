"""A glyph as a binary map at coverage 0.5 (# glyph, . ground), with x every 5 marked.  py -3.12 bmap.py <id> <state> <dark|light> x0 y0 x1 y1"""
import os, sys
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hp
from hp import fit
el, st, mode = sys.argv[1:4]
bx0, by0, bx1, by1 = (int(v) for v in sys.argv[4:8])
c = [c for c in hp.crops(el) if c[0] == st][0]
_, src, x0, y0, rgb, mask = c
L = fit.lum(rgb[by0 - y0:by1 - y0, bx0 - x0:bx1 - x0].astype(float))
m = mask[by0 - y0:by1 - y0, bx0 - x0:bx1 - x0]
lo, hi = np.percentile(L[m], 5), np.percentile(L[m], 95)
t = (L - lo) / (hi - lo)
g = (t < 0.5) if mode == "dark" else (t > 0.5)
print("     " + "".join(str(x // 10 % 10) if x % 5 == 0 else " " for x in range(bx0, bx1)))
print("     " + "".join(str(x % 10) if x % 5 == 0 else " " for x in range(bx0, bx1)))
for j in range(g.shape[0]):
    print(f"{by0 + j:4d} " + "".join("#" if v else "." for v in g[j]))
