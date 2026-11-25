"""P41 popups area: results rows for the info buttons (scored from their saved scenes), the kept-art elements and the blocked
handwriting. Usage: py -3.12 finish.py"""
import json
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import common as c      # noqa: E402

BASE = ("lilac tab strip (4 light rows on top, 5 dark rows below, cut at both cell sides) under a tan disc in a navy rim (rim "
        "circle fitted per state; the body's lit surface measured per state: tan, lighter tan on hover, yellow when selected; "
        "the hover shadow on the tab fitted); glyph: ")
IB = {
    "flag": ("a pole, a small waving pennant, the green as a dark lens whose near (left) side is deeper, a 1 px lilac light line "
             "along its top, the pole and the cup in tan below the green's edge",
             "the green's light top line and the tan cup under the pole are 1-2 px features"),
    "notice": ("a speech bubble (one ellipse) with a comma-like tail hooking down-left, the '!' cut out of it in tan (bar and dot)", ""),
    "pair": ("the same golfer figure three times, two in front and one behind (6 px right, 7 px up): ring head, a club held level "
             "behind the neck with its head turned down, body line, a back leg and a front leg with a foot; 1 px lines",
             "the roster says 'a pair of golfers' but the art shows three identical figures"),
    "pin": ("a pole with a red pennant (crimson edges, bright core), a navy upside-down triangle (a top line from a small lilac "
            "ball, two sides, a thick top right corner) ending in a rounded arrowhead", ""),
    "money": ("a dollar sign: an S stroke 3 px wide with flat ends and a vertical bar 2.2 px wide through it", ""),
    "chart": ("navy L axes (square ends) and an orange zigzag 2 px wide with flat ends through seven read points", ""),
    "checklist": ("a navy square outline and a thick green check over it (one outline: short arm, notch, long arm with a flat top "
                  "end) with a pale green light line along its upper-left edges", ""),
    "trophy": ("a cup drawn as one mirrored profile (flat mouth with a pale lip row and a lighter row under it, rounded bowl, "
               "stem), two ring handles (not mirror images in the art: the left one is wider) and a rounded foot", ""),
    "globe": ("a sphere whose ocean and green continents are both lit by bevel paints measured on the ocean / green pixels "
              "(white highlight upper left, deep blue lower left, dark green on the right); four continents as polygons",
              "the continents were read at 1x on a sphere 22 px across; check their outlines"),
    "tiles": ("four purple blocks in a 2 x 2 isometric layout drawn back to front, each a light top face, a mid left face and a "
              "dark right face (flat colours, no outline), corners read on the face map", ""),
}

KEPT = {
    "message-golf-ball": "picture (marked art): the dimpled message golf ball",
    "popup-icon-trophy": "picture (marked art)",
    "popup-icon-star": "picture (marked art)",
    "popup-icon-book": "picture (marked art)",
    "popup-icon-laurel": "picture (marked art)",
    "pair-book-icon": "picture (marked art); pasted as art inside pair-base",
    "head-select-ball": "picture (marked art)",
    "bulletin-wood": "full backdrop (marked art)",
    "bulletin-cork": "full backdrop (marked art)",
    "trophy-mantle": "full backdrop (marked art)",
    "top10-blank": "full backdrop (marked art)",
    "top10-trophies": "picture (marked art)",
    "trophy-plates": "engraved plaques (marked art)",
    "trophy-cup": "picture (marked art)",
    "tacs-ball": "picture (marked art)",
    "tacs-checks": "gold check bubbles (marked art)",
}


def main():
    keys = list(IB)
    for i, k in enumerate(keys):
        eid = f"info-button-{k}"
        d = json.load(open(os.path.join(c.OUT, eid + ".json")))
        mads, edges = [], []
        for st, S in d["states"].items():
            m, _ = c.score(S["scene"], "InfoButtons.pcx", tuple(S["box"]))
            e = c.edge_px(S["scene"], "InfoButtons.pcx", tuple(S["box"]))
            mads.append(m)
            if e is not None:
                edges.append(e)
        how, unsure = IB[k]
        u = "glyph numbers tuned by at most 0.5 px against the idle art"
        if unsure:
            u = unsure + "; " + u
        c.record(eid, "done", BASE + how, float(np.mean(mads)), float(np.mean(edges)), u)
        print(f"[{i + 1}/{len(keys)}] {eid} mad {np.mean(mads):.2f}", flush=True)
    for eid, why in KEPT.items():
        c.record(eid, "kept_art", why)
    c.record("tacs-handwriting", "blocked", "not drawn", unsure="awaiting Mariano's decision")
    print("rows written")


if __name__ == "__main__":
    main()
