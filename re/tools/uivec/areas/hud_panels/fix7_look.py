"""P41 round 7: look at the approved scenes of the tab bubbles as they appear on each tray (selected yellow + two blue)
and as separate cuts, art above drawing, so the inconsistency between states can be seen.

Usage: py -3.12 fix7_look.py <out.png>
"""
import json
import os
import sys

import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import common as C  # noqa: E402
import figs  # noqa: E402,F401   registers hud_panels.limb / rrect4

SCN = os.path.join(C.ROOT, "re", "tools", "uivec", "scenes", "hud_panels")


def approved(el):
    return json.load(open(os.path.join(SCN, el + ".json")))


def tile(key, scene, rct, z=8):
    x, y, w, h = rct
    rgb, m = C.sheet(key)
    art = np.kron(rgb[y:y + h, x:x + w], np.ones((z, z, 1)))
    dr = C.render_cell(scene, rct, z)
    return np.concatenate([art, np.full((4, w * z, 3), 255.0), dr])


# tab cells inside the trays (top-left of a 33 px cell, found by eye on the sheets: halos (286.5,492.5), (256.1,511.2),
# (233.2,539.3))
TRAY_TABS = {"M": [(270, 476), (240, 495), (217, 523)], "J": [(270, 476), (240, 495), (217, 523)],
             "E": [(270, 476), (240, 495), (217, 523)]}
TRAY_EL = {"M": "member-panel", "J": "golfer-action-panel", "E": "employee-panel"}
CUTS = ["tab-golfers-group", "tab-staff-group", "tab-golfer", "employee-tab-golfer", "tab-golfer-swing",
        "tab-golfer-staff", "joecool-tab-staff"]

if __name__ == "__main__":
    rows = []
    for key in "MJE":
        sc = approved(TRAY_EL[key])
        st = list(sc["states"])[0]
        scene = sc["states"][st]["scene"]
        ts = [tile(key, scene, (x, y, 33, 33)) for x, y in TRAY_TABS[key]]
        rows.append(np.concatenate(sum([[t, np.full((t.shape[0], 8, 3), 255.0)] for t in ts], [])[:-1], axis=1))
    ts = []
    for el in CUTS:
        sc = approved(el)
        for st, v in sc["states"].items():
            key = [k for k, n in C.SHEETS.items() if n.lower() in sc["coords"].lower()][0]
            ts.append(tile(key, v["scene"], tuple(v["cell"])))
    rows.append(np.concatenate(sum([[t, np.full((t.shape[0], 8, 3), 255.0)] for t in ts], [])[:-1], axis=1))
    W = max(r.shape[1] for r in rows)
    img = np.concatenate(sum([[np.pad(r, ((0, 0), (0, W - r.shape[1]), (0, 0)), constant_values=255),
                               np.full((16, W, 3), 255.0)] for r in rows], []))
    Image.fromarray(np.clip(img + 0.5, 0, 255).astype(np.uint8)).save(sys.argv[1])
