"""P41 round 9 (2026-11-27, log/roadmap/P41/INSTALL78_BRIEF.md): the golfer info panel's Photo tab, Mariano's note on the round-7
card hud_panels--golfer-tab-photo: "center it".

The camera glyph (layers body .. lens of the bubble item) sat off the centre of its tab bubble. The glyph group is moved as one
rigid group, by the same vector in every state (idle / hover / selected), so that the centre of its drawn extent (the bounding
box of the glyph's coverage at 8x, every glyph layer together) lands on the centre of the bubble's body circle (the bevel
circle, the third layer of the bubble item). Shapes, paints, clips (the glyph is clipped to the body circle, which does not
move), the bubble, its rim and its shadow are unchanged.

Usage: py -3.12 fix9_photo.py      writes log/roadmap/P41/work9/hud_panels/golfer-tab-photo.{json,png} and results.tsv
"""
import copy
import json
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.normpath(os.path.join(HERE, "..", "..")))
import common as C  # noqa: E402
import figs  # noqa: E402,F401   registers hud_panels.limb / rrect4
import sdf  # noqa: E402
import states as S  # noqa: E402
import fix7  # noqa: E402

EL = "golfer-tab-photo"
SRC = os.path.join(C.ROOT, "log", "roadmap", "P41", "work7", "hud_panels", EL + ".json")
OUT = os.path.join(C.ROOT, "log", "roadmap", "P41", "work9", "hud_panels")
GLYPH = ["body", "top light", "low light", "prism", "button", "left light", "right light", "lens ring", "lens"]
Z = 8


def glyph_extent(scene, cell):
    """(x0, y0, x1, y1) in sheet pixels of the glyph layers' coverage (rendered white on black at Z, coverage > 0.5)"""
    it = scene[1]
    ls = []
    for L in it["layers"]:
        if L.get("name") in GLYPH:
            o = {k: v for k, v in L.items() if k not in ("fx",)}
            o["fill"] = [255, 255, 255]
            o.pop("alpha", None)
            ls.append(o)
    x, y, w, h = cell
    img = np.zeros((h * Z, w * Z, 3))
    sdf.render([dict(it, layers=ls)], img, Z, origin=(x, y))
    m = img[..., 0] > 127.5
    ys, xs = np.nonzero(m)
    return (x + xs.min() / Z, y + ys.min() / Z, x + (xs.max() + 1) / Z, y + (ys.max() + 1) / Z)


def bubble_centre(scene):
    return tuple(scene[1]["layers"][2]["shape"]["c"])


def main():
    doc = json.load(open(SRC, encoding="utf-8"))
    prev = copy.deepcopy(doc)
    rows = []
    offs = {}
    for st, v in doc["states"].items():
        e = glyph_extent(v["scene"], v["cell"])
        bc = bubble_centre(v["scene"])
        gc = ((e[0] + e[2]) / 2, (e[1] + e[3]) / 2)
        offs[st] = (gc[0] - bc[0], gc[1] - bc[1])
    d = offs["idle"]
    mv = (round(-d[0], 2), round(-d[1], 2))
    for st, v in doc["states"].items():
        it = v["scene"][1]
        it["layers"] = [S.move_layer(L, mv[0], mv[1], clip=False) if L.get("name") in GLYPH else L for L in it["layers"]]
    after = {}
    for st, v in doc["states"].items():
        e = glyph_extent(v["scene"], v["cell"])
        bc = bubble_centre(v["scene"])
        after[st] = ((e[0] + e[2]) / 2 - bc[0], (e[1] + e[3]) / 2 - bc[1])
        rows.append((st, offs[st], after[st]))
    doc["round9"] = (f"Mariano round 7: 'center it'. The camera glyph group moved by ({mv[0]:+.2f}, {mv[1]:+.2f}) px in every state "
                     "so its drawn extent's centre sits on the bubble body circle's centre; shapes unchanged.")
    os.makedirs(OUT, exist_ok=True)
    with open(os.path.join(OUT, EL + ".json"), "w", encoding="utf-8", newline="\n") as f:
        json.dump(doc, f, default=C._r)
    # art | previous | new per state, then the overlay of the new states' edges
    fix7.OUT = OUT
    rr = []
    for st, v in doc["states"].items():
        rr.append((st, "G", tuple(v["cell"]), prev["states"][st]["scene"], v["scene"]))
    align = [(0, 0) if st != "hover" else (0, -1) for st in doc["states"]]
    img, _ = fix7.compare_rows(rr, Z, align=align)
    fix7.save(img, EL + ".png")
    with open(os.path.join(OUT, "results.tsv"), "w", encoding="utf-8", newline="\n") as f:
        f.write("id\tstatus\tchanged\tunsure\n")
        txt = "; ".join(f"{st}: glyph extent centre minus bubble centre ({a[0]:+.2f}, {a[1]:+.2f}) -> ({b[0]:+.2f}, {b[1]:+.2f}) px"
                        for st, a, b in rows)
        f.write(f"{EL}\tdone\tcamera glyph group (9 layers) moved by ({mv[0]:+.2f}, {mv[1]:+.2f}) px in every state, the same "
                f"vector everywhere; {txt}; bubble, rim, shadow, shapes and paints unchanged\t"
                "centred on the glyph's drawn bounding box (prism top to body bottom, left light to right edge), as the round-7 "
                "back arrows; the clip (the body circle) does not move\n")
    for st, a, b in rows:
        print(f"{st}: offset before ({a[0]:+.3f}, {a[1]:+.3f})  after ({b[0]:+.3f}, {b[1]:+.3f})")
    print("moved by", mv)


if __name__ == "__main__":
    main()
