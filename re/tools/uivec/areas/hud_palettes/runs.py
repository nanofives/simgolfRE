"""Opaque runs per row of a cut (where the cutter left the tray).  py -3.12 runs.py <id> [state]"""
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hp  # noqa: E402

el = sys.argv[1]
want = sys.argv[2] if len(sys.argv) > 2 else None
for st, src, x0, y0, rgb, mask in hp.crops(el):
    if want and st != want:
        continue
    h, w = mask.shape
    print(f"{el} {st} cut ({x0}, {y0}) {w} x {h}")
    prev = None
    for j in range(h):
        m = np.r_[False, mask[j], False].astype(int)
        d = np.diff(m)
        runs = list(zip(np.nonzero(d == 1)[0], np.nonzero(d == -1)[0]))
        txt = " ".join(f"{a}-{b}" for a, b in runs)
        if txt != prev:
            print(f"  y {j:3d}: {txt}")
            prev = txt
