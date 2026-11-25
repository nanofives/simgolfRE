"""reports_maps: writes one element's deliverables (P41 brief): <id>.png (art LEFT, shapes RIGHT, states stacked), <id>.json
(the scenes, numbers only) and its row of results.tsv (kept in results.json, the tsv regenerated from it)."""
import json
import os

import numpy as np

import common as C
import sdf

RES_JSON = os.path.join(C.OUT, "results.json")
COLS = ["id", "status", "how", "interior_mad", "edge_px", "unsure"]


def scale_for(w, h):
    z = max(1, int(np.ceil(300.0 / max(min(w, h), 1))))
    z = min(z, 8, max(1, int((1600 - 6) // (2 * w))))
    return z


def interior_mask(art, low):
    """Pixels away from edges: the art's local spread (3 x 3) is small, i.e. a flat or smoothly lit surface."""
    L = C.fit.lum(art)
    p = np.pad(L, 1, mode="edge")
    H, W = L.shape
    nb = np.stack([p[j:j + H, i:i + W] for j in range(3) for i in range(3)])
    return (nb.max(0) - nb.min(0)) < 14


def tile(art, base, scene, origin, z):
    h, w = art.shape[:2]
    k = int(np.ceil(4.0 / z))         # supersampled, then averaged down to z
    s = z * k
    img = np.kron(base, np.ones((s, s, 1)))
    sdf.render(scene, img, s, origin=origin)
    if k > 1:
        img = img.reshape(h * z, k, w * z, k, 3).mean(axis=(1, 3))
    left = np.kron(art, np.ones((z, z, 1)))
    return np.concatenate([left, np.full((h * z, 6, 3), 255.0), img], axis=1)


def score(art, base, scene, origin, mask=None, s=4):
    h, w = art.shape[:2]
    img = np.kron(base, np.ones((s, s, 1)))
    sdf.render(scene, img, s, origin=origin)
    low = img.reshape(h, s, w, s, 3).mean(axis=(1, 3))
    im = interior_mask(art, low)
    if mask is not None:
        im &= mask
    d = np.abs(low - art)
    return float(d[im].mean()) if im.any() else float("nan"), low


def deliver(eid, states, how, unsure="", status="done", edge_px="", z=None, extra_tiles=(), mask_fn=None):
    """states: [(label, art, base, scene, origin)]; extra_tiles: more images (H x W x 3) stacked under the states."""
    tiles, mads, js = [], [], {"id": eid, "states": {}}
    for label, art, base, scene, origin in states:
        h, w = art.shape[:2]
        zz = z or scale_for(w, h)
        tiles.append(tile(art, base, scene, origin, zz))
        mk = mask_fn(label, art) if mask_fn else None
        m, _ = score(art, base, scene, origin, mk)
        mads.append(m)
        js["states"][label] = {"origin": [float(origin[0]), float(origin[1])], "size": [int(w), int(h)], "scene": scene}
    tiles += list(extra_tiles)
    img = C.stack(tiles)
    if img.shape[1] > 1600:
        from PIL import Image
        k = 1600 / img.shape[1]
        img = np.asarray(Image.fromarray(np.clip(img + 0.5, 0, 255).astype(np.uint8)).resize(
            (1600, int(img.shape[0] * k)), Image.LANCZOS), float)
    C.save_png(img, os.path.join(C.OUT, f"{eid}.png"))
    with open(os.path.join(C.OUT, f"{eid}.json"), "w") as f:
        json.dump(js, f)
    mad = round(float(np.nanmean(mads)), 2) if mads else ""
    record(eid, status, how, mad, edge_px, unsure)
    print(f"[deliver] {eid} {status} interior_mad {mad}", flush=True)
    return mads


def record(eid, status, how, mad="", edge_px="", unsure=""):
    rows = {}
    if os.path.exists(RES_JSON):
        with open(RES_JSON) as f:
            rows = json.load(f)
    rows[eid] = {"id": eid, "status": status, "how": how, "interior_mad": mad, "edge_px": edge_px, "unsure": unsure}
    with open(RES_JSON, "w") as f:
        json.dump(rows, f, indent=1)
    with open(os.path.join(C.OUT, "results.tsv"), "w", encoding="utf-8") as f:
        f.write("\t".join(COLS) + "\n")
        for k in sorted(rows):
            f.write("\t".join(str(rows[k][c]).replace("\t", " ").replace("\n", " ") for c in COLS) + "\n")
