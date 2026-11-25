"""P41 popups round 5: reading aid. Prints the idle cell's glyph pixels (hex RGB) of the given info buttons and writes 12x
zooms into work5/popups/_read. py -3.12 read5.py chart checklist notice pair"""
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import common as c      # noqa: E402
import infobuttons as ib  # noqa: E402
import w5               # noqa: E402

w5.apply()
for k in sys.argv[1:]:
    r = ib.KEYS.index(k)
    box = ib.cell(r, 0)
    a = c.art(ib.SHEET, box)
    print("==", k, "idle cell rows 4-31, cols 3-30 (hex RGB)")
    print("     " + " ".join(f"{x:6d}" for x in range(3, 31)))
    for y in range(4, 32):
        print(f"{y:3d}  " + " ".join("%02x%02x%02x" % tuple(int(v) for v in a[y, x]) for x in range(3, 31)))
    c.zoom(ib.SHEET, box, s=12, name=f"z5_{k}")
