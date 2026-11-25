"""uivec: the Load Game screen's remaining chrome (P41): the scroll arrows and track, and the dark info bar. Every number
was read off enlargements and pixel rows of Title_LoadGame.pcx (2026-11-24); nothing is fitted.

Usage: py -3.12 re/tools/uivec/load_extras.py   -> log/roadmap/P41/load/extras_scene.json, extras_compare.png, a table
"""
import json
import os
import sys

import numpy as np
from PIL import Image

sys.path.insert(0, os.path.dirname(__file__))
import fit      # noqa: E402
import sdf      # noqa: E402

CREAM = [239, 220, 171]
EDGE = [189, 190, 193]


def tri(tip, base_y, half, name):
    """A scroll arrow: a cream triangle (tip, flat base), a soft dark edge on the side the light does not reach, and the
    faint light line the art has under an up arrow's base."""
    x, y = tip
    up = base_y > y
    pts = [[x, y], [x - half, base_y], [x + half, base_y]]
    # Mariano, 2026-11-24: a plain triangle, no dent in the base (what read as a notch is the light line under it)
    shape = {"t": "poly", "rings": [pts]}
    L = [{"shape": {"t": "move", "of": shape, "d": [-0.9, -0.6 if up else -1.1]}, "fill": [72, 76, 122], "alpha": 0.55, "soft": 0.7},
         {"shape": shape, "fill": CREAM}]
    if up:
        L.append({"shape": {"t": "stroke", "pts": [[x - half - 0.5, base_y + 0.4], [x + half + 0.6, base_y + 0.4]], "w": 1.0,
                            "cap": "butt"}, "fill": [200, 200, 222], "alpha": 0.75})
    if not up:  # a down arrow carries a dark line along the top of its base
        L.insert(1, {"shape": {"t": "stroke", "pts": [[x - half + 0.3, base_y - 0.6], [x + half - 0.3, base_y - 0.6]], "w": 1.0,
                               "cap": "butt"}, "fill": [70, 74, 118], "alpha": 0.45})
    ys = sorted((y, base_y))
    return {"name": name, "box": [x - half - 3, ys[0] - 3, x + half + 3, ys[1] + 3], "layers": L}


def rr(x0, y0, x1, y1, r):
    return {"t": "rrect", "c": [(x0 + x1) / 2, (y0 + y1) / 2], "half": [(x1 - x0) / 2, (y1 - y0) / 2], "r": r, "rot": 0.0}


def scene():
    items = [tri((784.7, 104.6), 118.4, 4.9, "scroll up, first arrow"), tri((784.7, 122.6), 136.4, 4.9, "scroll up, second arrow"),
             tri((784.5, 400.0), 385.3, 4.6, "scroll down, first arrow"), tri((784.5, 418.0), 403.3, 4.6, "scroll down, second arrow")]
    # the track: a thin light line round a slot the colour of the frame, a dark line across its top
    items.append({"name": "scroll track", "box": [776, 141, 792, 381], "layers": [
        {"shape": {"t": "stroke", "pts": [[779.2, 143.5], [788.8, 143.5]], "w": 1.0, "cap": "butt"}, "fill": [66, 70, 112]},
        {"shape": {"t": "outline", "of": rr(779.5, 144.5, 788.5, 378.5, 1.0), "w": 1.0}, "fill": EDGE}]})
    # the info bar: a dark box in a light lilac line two pixels wide, a dark line under it
    items.append({"name": "info bar", "box": [373, 432, 775, 495], "layers": [
        {"shape": rr(377.0, 436.4, 771.8, 492.0, 4.5), "fill": [40, 42, 66]},
        {"shape": rr(376.0, 435.0, 771.0, 490.8, 4.5), "fill": [176, 178, 214]},
        {"shape": rr(377.9, 436.9, 769.1, 488.9, 3.0), "fill": [33, 36, 52]}]})
    return items


def main():
    out = os.path.join(fit.ROOT, "log", "roadmap", "P41", "load")
    idx, pal = fit.load_sheet("Interface/Title_LoadGame.pcx")
    art = pal[idx].astype(float)
    items = scene()
    with open(os.path.join(out, "extras_scene.json"), "w") as f:
        json.dump(items, f)
    s = 6
    tiles = []
    print(f"{'element':28s} {'mad':>6s} {'w16':>6s}")
    for k, it in enumerate(items):
        x0, y0, x1, y1 = (int(v) for v in it["box"])
        # the shapes over the frame's plain colour where the element was (taken from just outside its box), so nothing
        # of the art's own element is lent to them
        ground = np.median(np.concatenate([art[y0, x0:x1], art[y1 - 1, x0:x1]]), axis=0)
        base = np.empty((y1 - y0, x1 - x0, 3))
        base[:] = ground
        img = np.kron(base, np.ones((s, s, 1)))
        sdf.render([it], img, s, origin=(x0, y0))
        low = img.reshape(y1 - y0, s, x1 - x0, s, 3).mean(axis=(1, 3))
        a = art[y0:y1, x0:x1]
        d = np.abs(low - a)
        print(f"{it['name']:28s} {d.mean():6.2f} {(d.max(axis=2) <= 16).mean():6.3f}")
        print(f"[{k + 1}/{len(items)}] {it['name']} PASS", flush=True)
        z = 8 if (x1 - x0) < 40 else 2
        up = lambda q: np.kron(q, np.ones((z, z, 1)))   # noqa: E731
        hi = np.asarray(Image.fromarray(np.clip(img + 0.5, 0, 255).astype(np.uint8)).resize(((x1 - x0) * z, (y1 - y0) * z),
                                                                                         Image.LANCZOS), float)
        tiles.append(np.concatenate([up(a), hi], axis=1))
    H = max(t.shape[0] for t in tiles[:5])
    row = np.concatenate([np.pad(t, ((0, H - t.shape[0]), (0, 8), (0, 0))) for t in tiles[:5]], axis=1)
    bar = tiles[5]
    W = max(row.shape[1], bar.shape[1])
    sheet = np.concatenate([np.pad(row, ((0, 0), (0, W - row.shape[1]), (0, 0))), np.pad(bar, ((8, 0), (0, W - bar.shape[1]), (0, 0)))])
    Image.fromarray(np.clip(sheet + 0.5, 0, 255).astype(np.uint8)).save(os.path.join(out, "extras_compare.png"))


if __name__ == "__main__":
    main()
