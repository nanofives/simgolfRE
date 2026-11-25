"""P41 hud_panels round 3 (2026-11-24): helpers to read the art beside a scene at 12-16x, and the work3 output paths.

side(key, box, scene, s, out): art crop (nearest) | the scene rendered over the same crop, with a pixel grid on both.
OUT3: log/roadmap/P41/work3/hud_panels.
"""
import os
import sys

import numpy as np
from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import common as C  # noqa: E402
import sdf  # noqa: E402

OUT3 = os.path.join(C.ROOT, "log", "roadmap", "P41", "work3", "hud_panels")


def _grid(img, s, x, y, w, h):
    d = ImageDraw.Draw(img)
    for g in range(max(w, h) + 1):
        if g <= w:
            d.line([(g * s, 0), (g * s, h * s)], fill=(255, 0, 0) if (x + g) % 10 == 0 else (120, 60, 60))
        if g <= h:
            d.line([(0, g * s), (w * s, g * s)], fill=(255, 0, 0) if (y + g) % 10 == 0 else (120, 60, 60))
    return img


def side(key, box, scene, s=14, out=None, bg=None, grid=True):
    x, y, w, h = box
    rgb, m = C.sheet(key)
    art = rgb[y:y + h, x:x + w]
    big = np.empty((h * s, w * s, 3))
    big[:] = C.FLAT if bg is None else bg
    sdf.render(scene, big, s, origin=(x, y))
    a = Image.fromarray(np.clip(np.kron(art, np.ones((s, s, 1))) + .5, 0, 255).astype(np.uint8))
    b = Image.fromarray(np.clip(big + .5, 0, 255).astype(np.uint8))
    if grid:
        _grid(a, s, x, y, w, h)
        _grid(b, s, x, y, w, h)
    W = Image.new("RGB", (2 * w * s + 6, h * s), (255, 255, 255))
    W.paste(a, (0, 0))
    W.paste(b, (w * s + 6, 0))
    out = out or os.path.join(OUT3, "_side.png")
    os.makedirs(os.path.dirname(out), exist_ok=True)
    W.save(out)
    return out


def low(scene, box, ss=4):
    """The scene rendered at 1:1 (ss x ss supersampled), as an h x w x 3 array."""
    x, y, w, h = box
    img = np.empty((h * ss, w * ss, 3))
    img[:] = C.FLAT
    sdf.render(scene, img, ss, origin=(x, y))
    return img.reshape(h, ss, w, ss, 3).mean(axis=(1, 3))


def sides(cells, scenes, s=12, out=None, cols=2):
    """Several cells, each art | scene, laid out in `cols` columns (for the 16 px faces)."""
    tiles = []
    for st, key, box in cells:
        p = side(key, box, scenes[st], s, os.path.join(OUT3, "_tile.png"))
        tiles.append(Image.open(p).copy())
    w, h = tiles[0].size
    rows = (len(tiles) + cols - 1) // cols
    W = Image.new("RGB", (cols * (w + 10), rows * (h + 10)), (255, 255, 255))
    for i, t in enumerate(tiles):
        W.paste(t, ((i % cols) * (w + 10), (i // cols) * (h + 10)))
    out = out or os.path.join(OUT3, "_sides.png")
    W.save(out)
    return out
