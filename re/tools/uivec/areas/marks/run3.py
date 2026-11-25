"""Fix round 3: render and record the redone marks elements into log/roadmap/P41/work3/marks/.
Usage: py -3.12 run2.py [id ...]   (no id: all). results.tsv columns: id, status, how, changed, unsure (+ mad / edge).
"""
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import common as C  # noqa: E402

C.OUT = os.path.join(C.ROOT, "log", "roadmap", "P41", "work3", "marks")
os.makedirs(C.OUT, exist_ok=True)
import el3_moods, el3_fun  # noqa: E401,E402

ELEMENTS = {}
for mod in (el3_moods, el3_fun):
    ELEMENTS.update(mod.ELEMENTS)
COLS = ["id", "status", "how", "changed", "unsure", "interior_mad", "edge_px"]


def write_row(row):
    p = os.path.join(C.OUT, "results.tsv")
    rows = {}
    if os.path.exists(p):
        with open(p) as f:
            next(f)
            for line in f:
                c = line.rstrip("\n").split("\t")
                rows[c[0]] = c
    rows[row[0]] = row
    with open(p, "w") as f:
        f.write("\t".join(COLS) + "\n")
        for k in sorted(rows):
            f.write("\t".join(rows[k]) + "\n")


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
    C.compare(rows, E.get("z", 20), os.path.join(C.OUT, f"{eid}.png"))
    C.save_json(eid, {"id": eid, "round": 3, "coords": "crop pixels: pixel (i, j) covers [i, i+1) x [j, j+1); scale with the element",
                      "states": [{"state": n, "sheet": r, "rect": list(b), "scene": scenes[n]} for n, r, b, _ in E["states"]]})
    mad = round(float(np.mean([s[0] for s in scores])), 2)
    edge = int(np.sum([s[1] for s in scores]))
    write_row([eid, E.get("status", "done"), E["how"], E["changed"], E.get("unsure", ""), str(mad), str(edge)])
    print(f"{eid}: mad {mad} edge {edge}")


if __name__ == "__main__":
    for i in sys.argv[1:] or list(ELEMENTS):
        run(i)
