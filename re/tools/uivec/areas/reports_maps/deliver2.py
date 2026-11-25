"""P41 fix round: deliver.py writing to log/roadmap/P41/work2/reports_maps/ with the fix round's columns
(id, status, how, changed, unsure). The comparison image is the same layout as round one (art LEFT, shapes RIGHT)."""
import json
import os

import numpy as np

import common as C
import deliver as D

OUT2 = os.path.join(C.ROOT, "log", "roadmap", "P41", "work2", "reports_maps")
RES_JSON = os.path.join(OUT2, "results.json")
COLS = ["id", "status", "how", "changed", "unsure", "interior_mad"]


def deliver(eid, states, how, changed, unsure="", status="done", z=None, extra_tiles=(), mask_fn=None):
    os.makedirs(OUT2, exist_ok=True)
    tiles, mads, js = [], [], {"id": eid, "states": {}}
    for label, art, base, scene, origin in states:
        h, w = art.shape[:2]
        zz = z or D.scale_for(w, h)
        tiles.append(D.tile(art, base, scene, origin, zz))
        mk = mask_fn(label, art) if mask_fn else None
        m, _ = D.score(art, base, scene, origin, mk)
        mads.append(m)
        js["states"][label] = {"origin": [float(origin[0]), float(origin[1])], "size": [int(w), int(h)], "scene": scene}
    tiles += list(extra_tiles)
    img = C.stack(tiles)
    if img.shape[1] > 1600:
        from PIL import Image
        k = 1600 / img.shape[1]
        img = np.asarray(Image.fromarray(np.clip(img + 0.5, 0, 255).astype(np.uint8)).resize(
            (1600, int(img.shape[0] * k)), Image.LANCZOS), float)
    C.save_png(img, os.path.join(OUT2, f"{eid}.png"))
    with open(os.path.join(OUT2, f"{eid}.json"), "w") as f:
        json.dump(js, f)
    mad = round(float(np.nanmean(mads)), 2) if mads else ""
    record(eid, status, how, changed, unsure, mad)
    print(f"[deliver2] {eid} {status} interior_mad {mad}", flush=True)
    return mads


def record(eid, status, how, changed, unsure="", mad=""):
    rows = {}
    if os.path.exists(RES_JSON):
        with open(RES_JSON) as f:
            rows = json.load(f)
    rows[eid] = {"id": eid, "status": status, "how": how, "changed": changed, "unsure": unsure, "interior_mad": mad}
    with open(RES_JSON, "w") as f:
        json.dump(rows, f, indent=1)
    with open(os.path.join(OUT2, "results.tsv"), "w", encoding="utf-8") as f:
        f.write("\t".join(COLS) + "\n")
        for k in sorted(rows):
            f.write("\t".join(str(rows[k][c]).replace("\t", " ").replace("\n", " ") for c in COLS) + "\n")
