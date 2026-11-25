"""P41 round 3 (log/roadmap/P41/FIX3_BRIEF.md): world-save, Mariano: "center it".

Measured on the round-2 shapes (work2/frontend/world-save.json):
- the glass bubble is the circle (576.57, 487.58) r 15.11, so it spans x 561.46-591.68 and rows 472.47-502.69;
- the floppy is x 566-586, rows 477-497 (centre 576.0, 487.0): 4.54 px of glass on its left and top against 5.68 px on its
  right and bottom, so it sat 0.57 px left and 0.58 px up of the bubble's centre (the art draws it there too);
- the socket and the outer ring are centred on (576.48, 487.84), 0.26 px below the bubble.

Fix: everything concentric on one centre, with the floppy's 1 px light lines kept on whole pixels (a half-pixel shift would
blur every one of them). The floppy moves (+1, +1) to centre (577.0, 488.0) and the bubble, the socket and the outer ring are
re-centred on that same point (bubble +0.43 / +0.42 px, ring +0.52 / +0.16 px). The hover state gets the same treatment on its
own centre (the hover bubble sits 150 px left and 1 px up of the idle one). Paints are bevels on the shape's own distance
field and the specular spot moves with the bubble, so the lighting is unchanged.
"""
import copy
import json
import os

import numpy as np

import common as c
import fix2 as F

c.OUT = os.path.join(c.ROOT, "log", "roadmap", "P41", "work3", "frontend")
c.VIEW = os.path.join(c.OUT, "_view")
c.RES = os.path.join(c.OUT, "results.tsv")
os.makedirs(c.VIEW, exist_ok=True)

SRC = os.path.join(c.ROOT, "log", "roadmap", "P41", "work2", "frontend", "world-save.json")
BTN = "Interface/WorldButton.pcx"
COLS = ["id", "status", "changed", "unsure"]


def _shift_circle(sh, dx, dy):
    assert sh["t"] == "circle", sh["t"]
    sh = dict(sh)
    sh["c"] = [round(sh["c"][0] + dx, 4), round(sh["c"][1] + dy, 4)]
    return sh


def recentre(scene, glyph_d=(1.0, 1.0)):
    """One state's scene with the ring, socket, bubble and floppy concentric on the floppy's new centre."""
    it = copy.deepcopy(scene[0])
    L = it["layers"]
    bubble = L[4]["shape"]                                    # the glass body (bevel); L[5] is its specular spot
    ring = L[0]["shape"]
    gd = it["layers"][-1]
    g = gd["shape"]
    gm = [0.0, 0.0]
    if g["t"] == "move":                                      # the hover glyph is the idle one moved
        gm, g = g["d"], g["of"]
    # the floppy's centre before the shift (x 566-586, rows 477-497 on the idle sheet)
    gc = (576.0 + gm[0] + glyph_d[0], 487.0 + gm[1] + glyph_d[1])
    db = (gc[0] - bubble["c"][0], gc[1] - bubble["c"][1])
    dr = (gc[0] - ring["c"][0], gc[1] - ring["c"][1])
    for k, lay in enumerate(L[:-1]):
        d = dr if k in (0, 1, 3) else db                      # 0 outer ring, 1 ring bevel, 3 socket; 2 rim shade, 4-5 bubble
        lay["shape"] = _shift_circle(lay["shape"], *d)
        if "falloff" in lay:
            fc = lay["falloff"]["c"]
            lay["falloff"]["c"] = [round(fc[0] + d[0], 4), round(fc[1] + d[1], 4)]
    gd["shape"] = {"t": "move", "of": g, "d": [gm[0] + glyph_d[0], gm[1] + glyph_d[1]]}
    gd["clip"] = copy.deepcopy(L[4]["shape"])
    x0, y0, x1, y1 = it["box"]
    it["box"] = [x0 - 1.0, y0 - 1.0, x1 + 1.0, y1 + 1.0]
    return it, {"centre": [round(gc[0], 2), round(gc[1], 2)], "bubble_shift": [round(db[0], 3), round(db[1], 3)],
                "ring_shift": [round(dr[0], 3), round(dr[1], 3)], "glyph_shift": list(glyph_d)}


def check(it):
    """Glass on each side of the floppy, measured on the shapes (sampled at 1/8 px)."""
    from common import sdf
    x0, y0, x1, y1 = [int(v) for v in it["box"]]
    s = 8
    X, Y = np.meshgrid(np.arange(x0 * s, x1 * s) / s + 0.5 / s, np.arange(y0 * s, y1 * s) / s + 0.5 / s)
    gm = sdf.sd(it["layers"][-1]["shape"], X, Y) < 0
    bm = sdf.sd(it["layers"][4]["shape"], X, Y) < 0
    def ext(m):
        ys, xs = np.nonzero(m)
        return X[0, xs.min()], X[0, xs.max()] + 1 / s, Y[ys.min(), 0], Y[ys.max(), 0] + 1 / s
    gx0, gx1, gy0, gy1 = ext(gm)
    bx0, bx1, by0, by1 = ext(bm)
    return {"left": round(gx0 - bx0, 3), "right": round(bx1 - gx1, 3), "top": round(gy0 - by0, 3),
            "bottom": round(by1 - gy1, 3)}


def main():
    src = json.load(open(SRC, encoding="utf-8"))
    btn, _ = c.sheet(BTN)
    states, info = [], {}
    for st in src["states"]:
        before = check(st["scene"][0])
        it, inf = recentre(st["scene"])
        inf["glass_before"], inf["glass_after"] = before, check(it)
        info[st["name"]] = inf
        print(st["name"], inf)
        states.append({"name": st["name"], "art": btn, "scene": [it], "rect": tuple(st["rect"])})
    how = ("glass bubble in its socket ring with the floppy disk (round-2 shapes), now concentric: the floppy moved (+1, +1) px "
           "onto the bubble's centre and the bubble, socket and ring re-centred on the same point")
    save = c.result
    c.result = lambda *a, **k: None
    try:
        sc = c.element("world-save", states, how, z=8, extra={"recentre": info})
    finally:
        c.result = save
    a = info["idle"]["glass_after"]
    changed = (f"centred: the floppy sat 0.57 px left and 0.58 px up of the bubble's centre (glass {info['idle']['glass_before']['left']} "
               f"px left / {info['idle']['glass_before']['right']} px right); now {a['left']} / {a['right']} px left/right and "
               f"{a['top']} / {a['bottom']} px top/bottom, and the bubble sits centred in its ring; the floppy's lines stay on "
               f"whole pixels, so they are as crisp as before")
    unsure = "the art itself draws the floppy about half a pixel up-left of centre; centred here as he asked"
    with open(c.RES, "w", encoding="utf-8") as f:
        f.write("\t".join(COLS) + "\n")
        f.write("\t".join(["world-save", "done", changed, unsure]) + "\n")
    print("world-save done", sc)


if __name__ == "__main__":
    main()
