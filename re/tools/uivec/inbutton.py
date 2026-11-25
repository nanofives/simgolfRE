"""uivec: one title button at 8x, the art beside the shapes, idle above hover (P41).
Usage: py -3.12 re/tools/uivec/inbutton.py <name> [out.png]      (UIVEC_OUTLINE / UIVEC_PARTS choose the glyph's source)
"""
import os
import sys

import numpy as np
from PIL import Image

sys.path.insert(0, os.path.dirname(__file__))
import fit      # noqa: E402
import sdf      # noqa: E402
import title    # noqa: E402


def main():
    name = sys.argv[1]
    out = sys.argv[2] if len(sys.argv) > 2 else os.path.join(fit.ROOT, "log", "roadmap", "P41", title.OUT, f"{name}_in_button.png")
    base_idx, base_pal, un_idx, un_pal, mo_idx, mo_pal = title.load_art()
    meas = title.measure(verbose=False)
    cx, cy, r = meas[name]["idle"]["c"]
    m = int(r * 1.18)
    box = (int(cx) - m, int(cy) - m - 2, int(cx) + m, int(cy) + m - 2)
    z, tiles = 8, []
    for hover, (oi, op) in ((False, (un_idx, un_pal)), (True, (mo_idx, mo_pal))):
        art = title.composite(base_idx, base_pal, oi, op)
        size = ((box[2] - box[0]) * z, (box[3] - box[1]) * z)
        a = Image.fromarray(art).crop(box).resize(size, Image.BILINEAR)
        bg = np.asarray(Image.fromarray(art if hover else base_pal[base_idx]).crop(box).resize(size, Image.BILINEAR), float).copy()
        sdf.render(title.scene(meas, hover, only=(name,)), bg, z, origin=box[:2])
        tiles.append(np.concatenate([np.asarray(a, float), bg], axis=1))
    Image.fromarray(np.clip(np.concatenate(tiles) + 0.5, 0, 255).astype(np.uint8)).save(out)
    print(out)


if __name__ == "__main__":
    main()
