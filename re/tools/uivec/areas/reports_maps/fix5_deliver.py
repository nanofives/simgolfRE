"""P41 round 5: write work5/reports_maps/ (three-panel images, json, results.tsv, _family.png)."""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fix5_route_tops as F  # noqa: E402

COMMON = ("relit on route-top-course's approved round-2 recipe: each sunk recess is its outer lip + a flat floor + ONE inner "
          "shadow (black alpha 0.43, soft rounded rectangle cut out 3.6 / 3.3 px inside the left / top walls, softness 3.8, "
          "corner r 4.0), the same numbers as course; replaces round one's nearest-edge bevel, which mitred every recess corner "
          "and put a light band inside the right and bottom walls")
CHANGED = {
    "employ": COMMON + "; the shadow lies over the cream column and the rules (the art: 161/255 on the cream column, 24/36 "
              "on the vertical rule at the top wall, ratio 0.63 as on course); table floor flat 243,230,204 as the art; no "
              "shade on the table feet (the art shows none); the slab's outer lip no longer paints a lavender band over the "
              "table feet above the slab line (kept on its sides)",
    "aura": COMMON + "; the shadow lies over the cream box's top under the slab line (the art: 0.77 / 0.86 / 0.95 there, "
            "course's profile)",
    "value": COMMON + "; the shadow lies over the stripes along the left wall, as course's over its stripes",
}
UNSURE = {
    "employ": "course's foot offset b (3.0) was its untuned start value, never visible on course; set b = r (-2.4) because "
              "the table feet show no shade (course renders the same either way)",
    "aura": "the meter keeps its own approved lighting (route-meter-aura), unchanged",
    "value": "the meter keeps its own approved lighting (route-meter-value), unchanged",
}
COLS = ["id", "status", "changed", "unsure", "interior_mad", "window_mad_r1", "window_mad_r5"]

if __name__ == "__main__":
    rows = [F.deliver(m, CHANGED[m], UNSURE[m]) for m in ("employ", "aura", "value")]
    with open(os.path.join(F.OUT5, "results.tsv"), "w", encoding="utf-8") as f:
        f.write("\t".join(COLS) + "\n")
        for r in rows:
            f.write("\t".join(str(r[c]) for c in COLS) + "\n")
    F.family()
