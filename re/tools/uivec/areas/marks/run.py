"""Render, score and record marks elements.  Usage: py -3.12 run.py [id ...]   (no id: all)

Each element module registers ELEMENTS[id] = {"how": str, "unsure": str, "z": zoom, "states": [(name, sheet, (x, y, w, h),
scene_fn)]}. scene_fn() returns a scene in crop coordinates (pixel (i, j) of the crop covers [i, i+1) x [j, j+1)).
"""
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import common as C  # noqa: E402
import el_discs, el_loose, el_moods, el_pins, el_medals  # noqa: E401,E402

ELEMENTS = {}
for mod in (el_discs, el_loose, el_moods, el_pins, el_medals):
    ELEMENTS.update(mod.ELEMENTS)


def run(eid):
    E = ELEMENTS[eid]
    rows, scenes, scores = [], {}, []
    for name, rel, (x, y, w, h), fn in E["states"]:
        art, m = C.crop(rel, x, y, w, h)
        bg = C.bgfill(art, m, E.get("bg", C.FLAT))
        scene = fn()
        scenes[name] = scene
        rows.append((art, m, scene, bg))
        scores.append(C.score(scene, C.on_flat(art, m, bg[0, 0]), m, bg))
    z = E.get("z", 20)
    C.compare(rows, z, os.path.join(C.OUT, f"{eid}.png"))
    C.save_json(eid, {"id": eid, "coords": "crop pixels: pixel (i, j) covers [i, i+1) x [j, j+1); scale with the element",
                      "states": [{"state": n, "sheet": r, "rect": list(b), "scene": scenes[n]} for n, r, b, _ in E["states"]]})
    mad = round(float(np.mean([s[0] for s in scores])), 2)
    edge = int(np.sum([s[1] for s in scores]))
    C.results_row(eid, E.get("status", "done"), E["how"], mad, edge, E.get("unsure", ""))
    print(f"{eid}: mad {mad} edge {edge} ({', '.join(f'{n}:{s[0]}/{s[1]}' for (n, *_), s in zip(E['states'], scores))})")


if __name__ == "__main__":
    ids = sys.argv[1:] or list(ELEMENTS)
    for i in ids:
        if i.endswith("*"):
            for k in ELEMENTS:
                if k.startswith(i[:-1]):
                    run(k)
        else:
            run(i)
