"""amenities-rotate: the tan bubble with a circular arrow (red tail, green head).

Read (cmap.py idle and hover, gridded 22x view, 2026-11-24): a ring about 2.6 px wide round (14.75, 14.0) local, radius 5.6,
running counter-clockwise on the screen from its red tail at the bottom (95 deg, screen angles) up the right side, over the
top (where red turns green) and down the left side, ending at 175 deg in a green triangular head (base 5.4 px across, tip
4.9 px on) pointing down. Local coordinates of the idle cut (0, 550)."""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from geom import arc, poly  # noqa: E402

C, R, A0, A1 = (14.75, 14.0), 5.6, 175.0, 95.0      # the head's end (left) and the tail's end (bottom), screen degrees
base = (C[0] + R * math.cos(math.radians(A0)), C[1] + R * math.sin(math.radians(A0)))
rad = (math.cos(math.radians(A0)), math.sin(math.radians(A0)))
tan = (math.sin(math.radians(A0)), -math.cos(math.radians(A0)))      # travel: counter-clockwise on the screen
HEAD = poly((base[0] + rad[0] * 2.7, base[1] + rad[1] * 2.7), (base[0] + tan[0] * 4.9, base[1] + tan[1] * 4.9),
            (base[0] - rad[0] * 2.7, base[1] - rad[1] * 2.7))
SPEC = dict(id="amenities-rotate", ref="idle", circle=(14.0, 14.0, 13.5), ground="auto", shadow=False,
            paint={"arrow": {"split_x": [14.75, 1.5]}}, glyph_dark=80,
            how="tan bubble, navy rim; glyph: a circular arrow: a 2.6 px ring (r 5.6) running counter-clockwise from a red "
                "tail at the bottom up the right side and over the top, down the left side into a green triangular head "
                "pointing down; paint green on the left half and red on the right half, each measured, blending over 3 px at the middle",
            glyph=[("ring: an open circle from the red tail at the bottom up the right side, over the top and down the left side", arc(C[0], C[1], R, A0, A1 + 360 - A0, 2.6), "arrow"),
                   ("head: a green triangle pointing down at the ring's left end", HEAD, "arrow")])

if __name__ == "__main__":
    import bel
    bel.build(SPEC)
