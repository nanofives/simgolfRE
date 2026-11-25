"""amenities-strip: the four pieces of the scalloped lilac strip (start, middle, middle 2, end) that tile into the amenities
row. Read on the cuts' runs (outlines) and a three-level luminance map (dark / mid / light faces), 2026-11-24.

Each piece is a dark lilac panel with a light rail along its top and a navy line along its bottom; the start and middle 2
pieces end in a concave scallop (the arch between two strips), the end piece in a rounded cap; their slanted top corners
carry a light border. The faint cubes are iso faces in two lighter tones: a mid lilac band of big rhombi (the cubes' left
faces) and narrow light parallelograms (their lit tops). Local coordinates of each piece's cut.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from geom import rect, line, poly, inter  # noqa: E402


def piece(outline, mids, lights, borders, w, curves=()):
    parts = [("panel: dark lilac", outline, "base")]
    parts += [(f"faint cubes: mid lilac face {i + 1}", inter(m, outline), "mid") for i, m in enumerate(mids)]
    parts += [(f"faint cubes: light top {i + 1}", inter(l, outline), "light") for i, l in enumerate(lights)]
    parts += [("light rail along the top", inter(rect(0, 0, w, 2.0), outline), "rail")]
    parts += [(f"light border on the slanted edge {i + 1}", inter(b, outline), "rail") for i, b in enumerate(borders)]
    parts += [("navy line along the bottom", inter(rect(0, 76.8, w, 79.0), outline), "edge")]
    parts += [(f"navy outline along the curved edge {i + 1}", inter(c, outline), "edge") for i, c in enumerate(curves)]
    return parts


START = piece(poly((8, 0), (68, 0), (68, 79), (43, 79), (41, 71), (39, 66), (37, 62), (35, 59), (33, 56), (31, 53), (29, 51), (27, 48),
                   (25, 46), (23, 44), (21, 42), (18, 40), (16, 38), (14, 36), (12, 34), (9, 32), (6, 30), (3, 28), (0, 28), (0, 17),
                   (4, 12), (6, 7)),
              [poly((38, 5), (68, 23), (68, 73), (63, 69), (56, 64), (50, 59), (48, 52), (42, 45), (30, 36), (22, 28), (22, 21))],
              [poly((25, 26.5), (31, 26.5), (48.5, 40), (47.5, 43.5), (43, 43.5)),
               poly((50, 53.5), (54.5, 53.5), (68, 61.5), (68, 73), (64, 68))],
              [line((8.5, 0.5), (4.5, 12.0), (1.0, 18.5), w=3.0)], 68,
              [line((43, 79), (41, 71), (39, 66), (37, 62), (35, 59), (33, 56), (31, 53), (29, 51), (27, 48), (25, 46), (23, 44), (21, 42),
                    (18, 40), (16, 38), (14, 36), (12, 34), (9, 32), (6, 30), (3, 28), (0, 28), w=4.0, smooth=1, cap="round")])
MIDDLE = piece(rect(0, 0, 47, 79),
               [poly((24.5, 5.5), (47, 20), (47, 74), (24.5, 60.5), (0, 74), (0, 20))],
               [poly((0, 20), (24.5, 33), (24.5, 40), (0, 27))], [], 47)
MIDDLE2 = piece(poly((0, 0), (58, 0), (60, 5), (62, 8), (64, 14), (67, 17), (67, 28), (64, 28), (61, 30), (59, 32), (56, 34), (54, 36),
                     (51, 38), (49, 40), (47, 42), (45, 44), (43, 46), (41, 49), (39, 51), (37, 54), (35, 56), (33, 59), (31, 63), (29, 66),
                     (27, 70), (25, 75), (25, 79), (0, 79)),
                [poly((24, 5), (46, 21), (46, 30), (36, 39), (24, 48), (23, 57), (19, 63), (10, 69), (4, 73), (0, 73), (0, 21))],
                [poly((0.5, 23.5), (4, 23.5), (22.5, 36.5), (22.5, 45.5), (19, 45.5), (0.5, 34))],
                [line((64.0, 18.5), (67.0, 18.5), w=3.0)], 67,
                [line((67, 28), (64, 28), (61, 30), (59, 32), (56, 34), (54, 36), (51, 38), (49, 40), (47, 42), (45, 44), (43, 46), (41, 49),
                      (39, 51), (37, 54), (35, 56), (33, 59), (31, 63), (29, 66), (27, 70), (25, 75), (25, 79), w=4.0, smooth=1, cap="round")])
END = piece(poly((0, 0), (11, 0), (15, 2), (19, 4), (21, 6), (23, 8), (25, 10), (27, 12), (29, 15), (36, 21), (39, 24), (41, 30), (41, 50),
                 (39, 51), (37, 54), (35, 56), (33, 59), (31, 63), (29, 66), (27, 70), (25, 75), (25, 79), (0, 79)),
            [poly((0, 32.5), (22.5, 48), (22.5, 61), (18, 64), (9, 69.5), (4, 73), (0, 73))], [],
            [line((0.0, 0.8), (6.0, 1.0), (12.0, 3.4), (17.0, 6.4), w=2.5), line((29.5, 17.5), (36.0, 21.0), (40.0, 29.0), w=2.5)], 41,
            [line((11, 0), (15, 2), (19, 4), (21, 6), (23, 8), (25, 10), (27, 12), (29, 15), (36, 21), (39, 24), (41, 30), (41, 50), (39, 51),
                  (37, 54), (35, 56), (33, 59), (31, 63), (29, 66), (27, 70), (25, 75), (25, 79), w=4.0, smooth=1, cap="round")])

SPEC = dict(id="amenities-strip", parts=START, parts_by_state={"start": START, "middle": MIDDLE, "middle 2": MIDDLE2, "end": END},
            how="dark lilac panel pieces (start with a concave scallop on its left, plain middles, middle 2 with a scallop on "
                "its right, a rounded end cap), a light rail along the top and light borders on the slanted corners, a navy "
                "line along the bottom, and the faint cubes as iso faces: mid lilac rhombi and narrow light tops")

if __name__ == "__main__":
    import fel
    fel.build(SPEC)
