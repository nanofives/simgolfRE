"""P41 hud_panels: measure every bubble cell (rim + body circles) and every paint family; writes bubble_geom.json and
bubble_paint.json next to this file.
Usage: py -3.12 re/tools/uivec/areas/hud_panels/measure_bubbles.py
"""
import json
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import common as C  # noqa: E402
import bubble as B  # noqa: E402

GROUPS = {
    "golfer-stats-button": "M:big", "joecool-stats-button": "J:big", "employee-thumbs-up": "E:big",
    "golfer-club-button": "J:small", "golfer-crossed-button": "J:small", "golfer-trophy-button": "J:small",
    "employee-hire": "E:mid", "employee-fire": "E:mid", "employee-resume": "E:mid",
    "tab-golfer": "blue", "tab-golfer-staff": "blue", "tab-golfers-group": "blue", "tab-golfer-swing": "blue",
    "joecool-tab-staff": "blue", "tab-staff-group": "blue", "employee-tab-golfer": "blue",
}
for _k in ("info", "mood", "swing", "photo", "story", "history", "thought"):
    GROUPS["golfer-tab-" + _k] = "G:tab"
GROUPS["golfer-tab-ok"] = "G:ok"
GEOM_FILE = os.path.join(HERE, "bubble_geom.json")


def family(el, st):
    g = GROUPS[el]
    return g if g == "blue" else g + ":" + st


def main():
    geom = {}
    for el in GROUPS:
        cells = C.CELLS[el]
        for st, key, rect in cells:
            guess = None
            if el == "golfer-tab-ok" and st == "disabled":
                g0 = geom[el]["idle"]["c"]
                guess = (g0[0] + 200, g0[1] + 1, g0[2])
            c = B.measure_rim(key, rect, guess)
            body = B.measure_body(key, c)
            if body is None:
                body = [c[0], c[1], round(c[2] - 1.5, 2)]
            geom.setdefault(el, {})[st] = {"c": c, "body": body, "key": key, "rect": list(rect)}
    fams = {}
    for el, sts in geom.items():
        for st, g in sts.items():
            fams.setdefault(family(el, st), []).append((g["key"], g["c"], g["body"]))
    paint = {}
    for fam, inst in fams.items():
        rows = [B.body_rows(k, c, b) for k, c, b in inst]
        paint[fam] = {"shade": B.solve_family(rows), "rim": list(np.median([B.rim_colour(k, c, b) for k, c, b in inst], axis=0))}
        print(fam, len(inst), "rim", paint[fam]["rim"], "base", paint[fam]["shade"][-1][0])
    for el, sts in geom.items():
        for st, g in sts.items():
            g["family"] = family(el, st)
    with open(GEOM_FILE, "w") as f:
        json.dump(geom, f, indent=0)
    B.save_paint(paint)


if __name__ == "__main__":
    main()
