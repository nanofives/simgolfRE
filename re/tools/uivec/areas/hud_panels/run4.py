"""P41 hud_panels round 4: build the four round-4 elements into log/roadmap/P41/work4/hud_panels/.

Usage: py -3.12 re/tools/uivec/areas/hud_panels/run4.py [element id ...]
One clean version per element. The face elements also get, under the art | shapes stack, an enlarged block at 12x
(art row over drawing row, five faces per row) and an actual-size strip (art over drawing at 1x and 2x).
"""
import os
import sys

os.environ["HUDP_WORK"] = "work4"
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np  # noqa: E402
from PIL import Image  # noqa: E402

import run  # noqa: E402,F401   every earlier builder
import common as C  # noqa: E402
import el_bubbles as EB  # noqa: E402
import el_fix4_faces  # noqa: E402,F401
import el_fix4_figs  # noqa: E402,F401

ROUND4 = ["joecool-tab-staff", "tab-golfer-staff", "member-mood-faces", "member-mood-faces-extra"]
FACES = {"member-mood-faces", "member-mood-faces-extra"}


def face_block(el, scenes, z=12, per=5):
    cells = C.CELLS[el]
    arts, draws, small = [], [], []
    for st, key, (x, y, w, h) in cells:
        rgb, _ = C.sheet(key)
        art = rgb[y:y + h, x:x + w]
        arts.append(np.kron(art, np.ones((z, z, 1))))
        draws.append(C.render_cell(scenes[st], (x, y, w, h), z))
        small.append((art, C.render_cell(scenes[st], (x, y, w, h), 8).reshape(h, 8, w, 8, 3).mean(axis=(1, 3))))
    gap = 4
    rows = []
    for k in range(0, len(cells), per):
        for group in (arts[k:k + per], draws[k:k + per]):
            r = np.concatenate(sum([[t, np.full((t.shape[0], gap, 3), 255.0)] for t in group], [])[:-1], axis=1)
            rows.append(r)
            rows.append(np.full((gap, r.shape[1], 3), 255.0))
        rows.append(np.full((gap * 3, rows[-1].shape[1], 3), 255.0))
    # actual size: art strip over drawing strip, at 1x and 2x
    for zz in (1, 2):
        for which in (0, 1):
            strip = np.concatenate([np.kron(s[which], np.ones((zz, zz, 1))) for s in small], axis=1)
            rows.append(strip)
        rows.append(np.full((gap * 2, rows[-1].shape[1], 3), 255.0))
    W = max(r.shape[1] for r in rows)
    return np.concatenate([np.pad(r, ((0, 0), (0, W - r.shape[1]), (0, 0)), constant_values=255) for r in rows])


def build(el):
    out = EB.BUILD[el](el)
    res = C.compare(el, out, z=Z.get(el))
    if el in FACES:
        path = os.path.join(C.OUT, el + ".png")
        top = np.asarray(Image.open(path).convert("RGB"), float)
        blk = face_block(el, out)
        W = max(top.shape[1], blk.shape[1])
        pad = lambda a: np.pad(a, ((0, 0), (0, W - a.shape[1]), (0, 0)), constant_values=255)   # noqa: E731
        img = np.concatenate([pad(top), np.full((12, W, 3), 255.0), pad(blk)])
        Image.fromarray(np.clip(img + 0.5, 0, 255).astype(np.uint8)).save(path)
    print(el, res)


Z = {}

if __name__ == "__main__":
    for el in sys.argv[1:] or ROUND4:
        build(el)
