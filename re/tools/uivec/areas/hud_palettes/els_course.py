"""courseinfo.pcx: the three blue medals (cash, fun, skill), the three value boxes ending in them, and the course title badge.

Read with cmap.py and 15x gridded views (2026-11-24). Medal glyphs are navy drawn over a white halo (each navy stroke sits on a
wider white stroke), which is what makes them read as cut-out signs on the glossy blue ball. Numbers are SHEET coordinates of
the medal 'a' cuts; M() moves them to a cut's local frame.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from geom import rect, line, poly, circle, oval, outlined, U, moved, rrect_c  # noqa: E402


def M(parts, ox, oy):
    return [(n, moved(s, -ox, -oy), k) for n, s, k in parts]


def halo(name, s, w, k_extra=1.8):
    """A navy stroke over its white halo: the same path, wider, in white, under it."""
    import copy
    h = copy.deepcopy(s)
    h["w"] = s["w"] + k_extra
    return [(name + ": white halo", h, "white"), (name, s, "navy")]


S_PATH = [(369.5, 81.6), (368.6, 79.7), (365.0, 79.5), (361.6, 79.8), (360.3, 81.6), (361.0, 83.4), (364.5, 84.3), (368.6, 85.2),
          (369.6, 87.0), (368.4, 89.0), (365.0, 89.5), (361.6, 89.2), (360.0, 87.6)]
CASH = (halo("dollar: one S stroke", line(*S_PATH, w=1.6, smooth=2, cap="round"), 2.0) +
        halo("dollar: left bar above the S", line((363.6, 74.6), (363.6, 79.0), w=1.2), 1.6) +
        halo("dollar: right bar above the S", line((367.6, 74.6), (367.6, 79.0), w=1.2), 1.6) +
        halo("dollar: left bar below the S", line((363.4, 90.0), (363.4, 93.6), w=1.2), 1.6) +
        halo("dollar: right bar below the S", line((366.6, 90.0), (366.6, 93.6), w=1.2), 1.6))
FUN = [("mouth: white halo", poly((357.6, 181.0), (366.0, 180.6), (375.0, 179.8), (374.6, 184.8), (372.2, 189.4), (368.2, 191.4),
                                  (363.0, 191.4), (359.6, 188.2)), "white"),
       ("mouth: an open laughing D, dark inside", poly((358.6, 182.0), (366.0, 181.6), (374.0, 180.8), (373.6, 184.5), (371.4, 188.6),
                                                       (368.0, 190.4), (363.4, 190.4), (360.6, 187.6)), "navy"),
       ("mouth: upper teeth, a white band", poly((362.4, 182.5), (372.6, 181.7), (372.0, 183.9), (362.6, 184.3)), "white"),
       ("mouth: tongue and lower lip, light", oval(365.6, 188.1, 3.6, 1.4), "white"),
       ("left eye", oval(362.8, 176.6, 0.9, 1.4), "navy"), ("right eye", oval(368.8, 175.8, 0.9, 1.4), "navy"),
       ("cheek crease at the left of the mouth", line((357.4, 178.4), (358.6, 181.4), w=1.0), "navy")]
SKILL = ([("head: white halo ring", outlined(circle(364.0, 280.5, 4.7), 3.4), "white"),
          ("head: a navy ring", outlined(circle(364.0, 280.5, 4.7), 1.2), "navy")] +
         halo("body and pin: a line from the lower left across to the right and up", line((361.0, 291.6), (366.0, 291.6), (372.6, 285.0), w=1.4), 1.8) +
         halo("a short stroke down into the bend", line((365.0, 287.0), (366.2, 290.6), w=1.2), 1.6))

MED = dict(ground="auto", shadow=False, ref="a", body_kdir=0.1, pick={"white": "light"})
SPECS = [
    dict(id="course-cash-medal", circle=(15.5, 15.5, 15.0), glyph=M(CASH, 350, 69),
         how="glossy blue ball with a dark rim; glyph: a dollar sign: one navy S stroke and two short bars above and below it, "
             "each navy stroke over a white halo", **MED),
    dict(id="course-fun-medal", circle=(15.5, 15.5, 15.0), glyph=M(FUN, 350, 169),
         how="glossy blue ball; glyph: a laughing face: an open D mouth (navy, a white band of upper teeth, a light tongue) "
             "in a white halo, two small navy eyes and a cheek crease", **MED),
    dict(id="course-skill-medal", circle=(15.5, 15.5, 15.0), glyph=M(SKILL, 350, 269),
         how="glossy blue ball; glyph: a figure with a pin: a navy ring for the head, a line from the lower left across and "
             "up to the right (the pin) and a short stroke into the bend, each navy over a white halo", **MED),
]


def box(id_, glyph, ox, oy, mx, w, what, fy=26.5):
    """A value box: a 2 px stadium outline (its line lighter on the outer top, darker below) whose right end runs under the
    medal. ox, oy: the medal 'a' cut; mx: the medal's x in the box cut (found by matching the medal's pixels); w: the field's
    length to the medal's centre."""
    field = outlined(rrect_c(w / 2.0, fy, w / 2.0 - 1.0, 8.5, 8.5), 2.0)
    return dict(id=id_, ref="idle", circle=(mx + 15.5, 15.5, 15.0), shadow=False, body_kdir=0.1, pick={"white": "light"},
                ground=[("value field: a 2 px stadium outline running under the medal", field, "bevel")],
                glyph=M(glyph, ox - mx, oy), ground_stops=[0.0, 0.7, 1.4, 2.0],
                how="value field: a 2 px navy stadium outline (lighter on its outer top, darker below) whose right end runs "
                    "under a glossy blue medal; medal glyph as on the loose medal: " + what)


SPECS += [box("course-cash-box", CASH, 350, 69, 107, 125, "a dollar sign"),
          box("course-fun-box", FUN, 350, 169, 79, 97, "a laughing face", fy=27.5),
          box("course-skill-box", SKILL, 350, 269, 57, 75, "a figure with a pin")]

if __name__ == "__main__":
    import bel
    want = sys.argv[1:]
    for sp in SPECS:
        if not want or sp["id"] in want:
            bel.build(sp)
