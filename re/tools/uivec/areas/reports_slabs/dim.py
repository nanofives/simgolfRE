"""report-dim: black.pcx is one flat black 800 x 600 sheet (every pixel 0, 0, 0; checked below) that the game blends at a
quarter over the screen behind every report. The shape: one rectangle the size of the screen, painted black; the blend
strength is the game's (not part of the sheet), recorded as "blend" in the scene for the port."""
import json
import os

import numpy as np

import common as C
import slabkit as K


def main():
    idx, pal = C.fit.load_sheet(C.IS + "black.pcx")
    rgb = pal[idx].astype(float)
    flat = bool((rgb == rgb[0, 0]).all())
    col = [float(v) for v in rgb[0, 0]]
    S = {"t": "rrect", "c": [400.0, 300.0], "half": [400.0, 300.0], "r": 0.0, "rot": 0.0}
    scene = [{"box": [0, 0, 800, 600], "layers": [{"shape": S, "fill": col}], "part": "dim", "blend": 0.25}]

    def mine(box, s):
        img = np.empty(((box[3] - box[1]) * s, (box[2] - box[0]) * s, 3))
        img[:] = C.FLAT
        C.sdf.render(scene, img, s, origin=(box[0], box[1]))
        return img
    K.compare_png(os.path.join(C.OUT, "report-dim.png"), rgb, mine, [("idle (120 x 90 corner)", (0, 0, 120, 90))], scale=3)
    json.dump({"id": "report-dim", "sheet": "black.pcx", "scene": scene, "sheet_is_flat": flat},
              open(os.path.join(C.OUT, "report-dim.json"), "w"))
    mad = float(np.abs(K.down(mine((0, 0, 800, 600), 1), 1) - rgb).mean())
    print("report-dim flat", flat, col, "mad", mad)
    return mad


if __name__ == "__main__":
    main()
