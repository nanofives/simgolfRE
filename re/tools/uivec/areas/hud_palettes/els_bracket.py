"""hud-arc-bracket: the thin tan arc that links a mode bubble to its palette, three sizes.

Read on the cuts (2026-11-24) from the tan band's centre line per column and the navy pixels beside it: a short tan stick at
the top left (the hook) with a navy line on its right, a 2.6 px tan band sweeping down to the right along a curve with a
navy edge under it, ending in a flat tan ledge (9 px) with a dark navy edge under it that runs to the cut's right end.
Local coordinates of each cut."""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from geom import rect, line, moved, poly  # noqa: E402


def bracket(hook, arc, ledge_x, ledge_y, w):
    """hook: (top, bottom) of the stick; arc: centre-line points ending at the ledge's lower left corner; ledge_y: the
    ledge's centre row; ledge_x: its lower left corner (its left end slants back 5 px at the top)."""
    a = line(*arc, w=2.6, smooth=2, cap="round")
    ledge = poly((ledge_x + 5.0, ledge_y - 4.5), (w, ledge_y - 4.5), (w, ledge_y + 4.5), (ledge_x, ledge_y + 4.5))
    return [("arc: a light lavender glint along its upper side", moved(line(*arc, w=1.4, smooth=2, cap="round"), -0.5, -2.2), "glint"),
            ("arc: navy edge under the band", moved(line(*arc, w=2.6, smooth=2, cap="round"), 0.6, 2.6), "navy"),
            ("hook: navy line on its right", moved(line(*hook, w=2.4, cap="round"), 4.6, 0.0), "navy"),
            ("ledge: dark edge under it", rect(ledge_x, ledge_y + 4.5, w, ledge_y + 8.0), "dark"),
            ("hook: a short tan stick at the top left", line(*hook, w=3.0, cap="round"), "tan"),
            ("arc: the tan band", a, "tan"),
            ("ledge: a flat tan bar to the right end, its left end slanting", ledge, "tan")]


SPEC = dict(id="hud-arc-bracket", parts=None, parts_by_state={
    "large": bracket([(4.0, 1.0), (3.6, 10.0)], [(3.6, 10.2), (12, 11.5), (30, 14.5), (60, 24.5), (90, 40.5), (114, 57.5), (132, 74.5),
                                                  (138, 81.0), (140.6, 86.0)], 139.0, 83.0, 214.0),
    "medium": bracket([(7.6, 1.2), (2.8, 11.6)], [(2.8, 11.8), (18, 18), (30, 25), (42, 33), (54, 42.5), (66, 53), (78, 64.5),
                                                   (82, 69.5), (83.6, 70.4)], 82.0, 67.0, 153.0),
    "small": bracket([(11.6, 2.4), (5.4, 11.0)], [(5.4, 11.2), (12, 14.5), (18, 21), (24, 27.5), (30, 34), (33, 39.0), (34.0, 40.4)],
                     33.0, 37.0, 107.0)},
    how="a short tan stick at the top left with a navy line on its right, a 2.6 px tan band curving down to the right with "
        "a navy edge under it, ending in a flat 9 px tan ledge with a dark navy edge under it; three sizes")
SPEC["parts"] = SPEC["parts_by_state"]["large"]

if __name__ == "__main__":
    import fel
    fel.build(SPEC)
