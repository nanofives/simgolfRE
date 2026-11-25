"""uivec: one pad's art, shapes and difference (x4) stacked, at 3x (P41).
Usage: py -3.12 re/tools/uivec/paddiff.py <name> [idle|hover] [out.png]
"""
import copy
import os
import sys

import numpy as np
from PIL import Image

sys.path.insert(0, os.path.dirname(__file__))
import accept   # noqa: E402
import fit      # noqa: E402
import recipes  # noqa: E402


def main():
    name = sys.argv[1]
    hover = len(sys.argv) > 2 and sys.argv[2] == "hover"
    out = sys.argv[3] if len(sys.argv) > 3 else os.path.join(fit.ROOT, "log", "roadmap", "P41", "title", "tmp",
                                                             f"pad_{name}_{accept.ST[hover]}.png")
    b = accept.Bench()
    style = copy.deepcopy(recipes.STYLE)
    a = b.target(name, "pad", hover)
    v = b.render(name, "pad", hover, style, 4)
    reg = b.region(name, "pad", hover)
    d = np.clip(np.abs(v - a) * 4, 0, 255) * reg[..., None]
    z = 3
    img = np.concatenate([np.kron(q, np.ones((z, z, 1))) for q in (a, v, d)], axis=0)
    Image.fromarray(np.clip(img + 0.5, 0, 255).astype(np.uint8)).save(out)
    g = b.meas[name][accept.ST[hover]]
    print(out, "outer", g["outer"], "inner", g["inner"])


if __name__ == "__main__":
    main()
