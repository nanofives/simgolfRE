"""Gridded enlarged view of an element's cuts.  py -3.12 look.py <id> [z] [state...]  -> scratch/<id>_look.png"""
import os
import sys

import numpy as np
from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hp  # noqa: E402

OUTD = os.environ.get("LOOK_OUT", os.path.join(hp.OUT, "_look"))


def main():
    el = sys.argv[1]
    z = int(sys.argv[2]) if len(sys.argv) > 2 else 12
    want = sys.argv[3:]
    ims = []
    for st, src, x0, y0, rgb, mask in hp.crops(el):
        if want and st not in want:
            continue
        print(st, src, x0, y0, mask.shape[1], mask.shape[0])
        ims.append(hp.gridview(rgb, mask, x0, y0, z))
    W = sum(i.width for i in ims) + 6 * (len(ims) - 1)
    H = max(i.height for i in ims)
    can = Image.new("RGB", (W, H), (20, 20, 20))
    x = 0
    for i in ims:
        can.paste(i, (x, 0))
        x += i.width + 6
    os.makedirs(OUTD, exist_ok=True)
    p = os.path.join(OUTD, f"{el}_look.png")
    can.save(p)
    print(p)


if __name__ == "__main__":
    main()
