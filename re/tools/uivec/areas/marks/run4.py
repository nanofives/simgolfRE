"""Fix round 4: render and record gb-mood into log/roadmap/P41/work4/marks/.
Usage: py -3.12 run4.py
The comparison image: on top, art LEFT and shapes RIGHT, the five faces in a row at 1x, 2x and 6x (the shapes at 1x and 2x
are box-filtered renders, what a 16 px icon shows on screen); under it the enlarged row at 12x: the art's five faces, then
the shapes' five faces.
"""
import os
import sys

import numpy as np
from PIL import Image, ImageDraw

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import common as C  # noqa: E402

C.OUT = os.path.join(C.ROOT, "log", "roadmap", "P41", "work4", "marks")
os.makedirs(C.OUT, exist_ok=True)
import el4_moods  # noqa: E402

COLS = ["id", "status", "changed", "unsure", "how", "interior_mad", "edge_px"]
BG = C.FLAT
WHITE = 255.0


def u8(a):
    return Image.fromarray(np.clip(np.asarray(a, float) + 0.5, 0, 255).astype(np.uint8))


def art_at(art, m, z):
    a = C.on_flat(art, m, BG)
    return np.asarray(u8(a).resize((a.shape[1] * z, a.shape[0] * z), Image.NEAREST), float)


def shapes_at(scene, w, h, z):
    bg = np.empty((h, w, 3))
    bg[:] = BG
    if z <= 2:                       # what the icon shows at that size: rendered at 8 x z and box-filtered, then enlarged
        ss = 8
        big = C.render_scene(scene, w, h, ss * z, bg)
        small = big.reshape(h * z, ss, w * z, ss, 3).mean(axis=(1, 3))
        return small
    return C.render_scene(scene, w, h, z, bg)


def row(tiles, gap):
    H = max(t.shape[0] for t in tiles)
    out = []
    for k, t in enumerate(tiles):
        if k:
            out.append(np.full((H, gap, 3), BG))
        out.append(np.pad(t, ((0, H - t.shape[0]), (0, 0), (0, 0)), constant_values=0) if t.shape[0] < H else t)
    return np.concatenate(out, axis=1)


def stack(blocks, gap, fill=WHITE):
    W = max(b.shape[1] for b in blocks)
    out = []
    for k, b in enumerate(blocks):
        if k:
            out.append(np.full((gap, W, 3), fill))
        pad = W - b.shape[1]
        out.append(np.concatenate([b, np.full((b.shape[0], pad, 3), fill)], axis=1) if pad else b)
    return np.concatenate(out)


def side(a, b, gap=10):
    H = max(a.shape[0], b.shape[0])
    pa = np.full((H, a.shape[1], 3), WHITE); pa[:a.shape[0]] = a
    pb = np.full((H, b.shape[1], 3), WHITE); pb[:b.shape[0]] = b
    return np.concatenate([pa, np.full((H, gap, 3), WHITE), pb], axis=1)


def label(img, text):
    im = u8(img)
    d = ImageDraw.Draw(im)
    d.text((4, 2), text, fill=(0, 0, 0))
    return np.asarray(im, float)


def band(text, W, h=16):
    return label(np.full((h, W, 3), WHITE), text)


def compose(E, path):
    arts, scenes = [], []
    for name, rel, (x, y, w, h), fn in E["states"]:
        art, m = C.crop(rel, x, y, w, h)
        arts.append((art, m))
        scenes.append(fn())
    w, h = arts[0][0].shape[1], arts[0][0].shape[0]
    top = []
    for z, g in ((1, 4), (2, 6), (6, 12)):
        A = row([art_at(a, m, z) for a, m in arts], g)
        S = row([shapes_at(s, w, h, z) for s in scenes], g)
        top.append(side(A, S, 16))
    top = stack(top, 10)
    A12 = row([art_at(a, m, 12) for a, m in arts], 12)
    S12 = row([shapes_at(s, w, h, 12) for s in scenes], 12)
    W = max(top.shape[1], A12.shape[1])
    img = stack([band("art LEFT, shapes RIGHT at 1x, 2x, 6x", W), top,
                 band("12x: art", W), A12, band("12x: shapes", W), S12], 4)
    u8(img).save(path)
    return scenes


def write_row(r):
    p = os.path.join(C.OUT, "results.tsv")
    rows = {}
    if os.path.exists(p):
        with open(p) as f:
            next(f)
            for line in f:
                c = line.rstrip("\n").split("\t")
                rows[c[0]] = c
    rows[r[0]] = r
    with open(p, "w") as f:
        f.write("\t".join(COLS) + "\n")
        for k in sorted(rows):
            f.write("\t".join(rows[k]) + "\n")


def run(eid):
    E = el4_moods.ELEMENTS[eid]
    scenes = compose(E, os.path.join(C.OUT, f"{eid}.png"))
    scores = []
    for (name, rel, (x, y, w, h), fn), sc in zip(E["states"], scenes):
        art, m = C.crop(rel, x, y, w, h)
        bg = C.bgfill(art, m, BG)
        scores.append(C.score(sc, C.on_flat(art, m, BG), m, bg))
    C.save_json(eid, {"id": eid, "round": 4, "emotions": E["emotions"],
                      "coords": "crop pixels: pixel (i, j) covers [i, i+1) x [j, j+1); scale with the element",
                      "states": [{"state": n, "emotion": E["emotions"][k], "sheet": r, "rect": list(b), "scene": s}
                                 for k, ((n, r, b, _), s) in enumerate(zip(E["states"], scenes))]})
    mad = round(float(np.mean([s[0] for s in scores])), 2)
    edge = int(np.sum([s[1] for s in scores]))
    write_row([eid, E.get("status", "done"), E["changed"], E.get("unsure", ""), E["how"], str(mad), str(edge)])
    print(f"{eid}: mad {mad} edge {edge}")


if __name__ == "__main__":
    for i in sys.argv[1:] or list(el4_moods.ELEMENTS):
        run(i)
