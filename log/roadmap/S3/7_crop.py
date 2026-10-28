"""S3.7: crop a region of a framegate port image, the reference frame and their difference side by side (x3).

    cd port/build/S3_7_src && ~/bin/py -3.12 <abs>/log/roadmap/S3/7_crop.py <img dir> <scenario> <checkpoint> <x,y,w,h | region> <out.png>
"""
import os
import sys

import numpy as np
from PIL import Image

sys.path.insert(0, os.getcwd())
from port.test import framegate as FG  # noqa: E402
from port.test import gfx as GX  # noqa: E402


def main():
    d, scen, chk, reg, out = sys.argv[1:6]
    regs = dict(FG.INGAME_REGIONS)
    x, y, w, h = regs[reg] if reg in regs else [int(v, 0) for v in reg.split(",")]
    port = np.asarray(Image.open(os.path.join(d, f"{scen}.{chk}.port.png")).convert("RGB"))
    ref = GX.reference(scen, chk)
    a, b = port[y:y + h, x:x + w], ref[y:y + h, x:x + w]
    diff = (a != b).any(axis=2)
    dimg = np.zeros_like(a)
    dimg[diff] = (255, 0, 255)
    strip = np.concatenate([a, np.full((h, 4, 3), 128, np.uint8), b, np.full((h, 4, 3), 128, np.uint8), dimg], axis=1)
    Image.fromarray(strip).resize((strip.shape[1] * 3, strip.shape[0] * 3), Image.NEAREST).save(out)
    print(f"{scen} {chk} {reg}: {int(diff.sum())} px differ; port | ref | diff -> {out}")


if __name__ == "__main__":
    main()
