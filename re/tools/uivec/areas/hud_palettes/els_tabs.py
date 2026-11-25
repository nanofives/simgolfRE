"""Palette bubbles: the terrain palette's hole button, and the blue glass tab bubbles (shovel, up-down arrows, house, grass).

Read with cov.py (coverage digits) and gridded enlargements, 2026-11-24. Coordinates local to each element's idle cut.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from geom import rect, line, poly, circle, oval, outlined, sub, curve, U  # noqa: E402

SPECS = []


def S(**k):
    SPECS.append(k)


# terrain-hole-button, idle cut at (0, 481): y_local = y - 481
S(id="terrain-hole-button", circle=(16.0, 16.0, 15.5), ground="auto", shadow=False,
  how="tan bubble, navy rim; glyph: a flag in a cup: a thin pole (1.5 px, bowing a little to the left as it goes down), "
      "a waving pennant from its top (thick near the pole, a light fold just off the pole, fluttering out into a thin "
      "tail), and the cup as a dark ellipse (rx 8.2, ry 3.0) holding two small light marks at its left",
  glyph=[("pole: thin, bowing a little to the left", curve((13.3, 6.2), (12.2, 14.0), (12.0, 22.6), 1.35, 1.3), "navy"),
         ("pennant: thick at the pole, waving out into a thin tail",
          poly((13.2, 7.0), (15.5, 7.4), (19.5, 8.0), (21.5, 9.5), (22.4, 11.4), (23.6, 12.9), (26.4, 14.4), (21.2, 14.3),
               (18.6, 12.6), (15.6, 11.9), (13.2, 12.0), smooth=1), "navy"),
         ("cup: a dark ellipse", oval(16.0, 24.65, 8.2, 3.05), "navy"),
         ("fold: a light stripe in the pennant beside the pole", poly((14.2, 8.2), (15.7, 8.3), (15.6, 11.0), (14.2, 11.0)),
          "hole"),
         ("two small light marks in the cup", U(oval(11.5, 24.9, 0.55, 1.0), oval(13.5, 24.9, 0.55, 1.0)), "hole")])


# tab-shovel, idle cut at (150, 480): x_local = x - 150, y_local = y - 480
# fix round ("better connected lines"): the grip is one closed loop, the shaft starts at the loop's lower corner (round cap,
# so the joint is filled) and runs into the blade; the blade's top is square to the shaft (rising to the right, as in the art)
S(id="tab-shovel", circle=(16.0, 16.0, 15.5), ground="auto_circle", shadow=False,
  how="blue glass tab bubble; glyph: a spade: a triangular D-grip drawn as one closed 1.5 px loop, a 2 px shaft leaving the "
      "loop's lower corner and slanting down to the right into the blade (round ends, so grip, shaft and blade join "
      "without a gap), a blade whose flat top is square to the shaft, its right edge straight and its left edge running in "
      "to the point at the bottom",
  glyph=[("grip: one closed triangular loop", line((14.6, 11.0), (11.35, 8.85), (16.2, 7.05), w=1.5, closed=True), "navy"),
         ("shaft: from the grip's lower corner into the blade", line((14.6, 11.0), (17.6, 20.6), w=2.0, cap="round"), "navy"),
         ("blade: top square to the shaft, straight right edge, left edge running in to the point",
          poly((14.4, 20.5), (20.9, 18.9), (21.75, 19.6), (21.75, 25.6), (21.0, 27.0), (19.8, 28.2), (18.6, 27.6), (17.4, 26.6),
               (16.0, 25.0), (14.9, 23.5), (14.2, 21.8), smooth=1), "navy")])

# fix round ("symmetry"): every part is mirrored about the bubble's own axis x 17.45 (rim fit 167.49; the art's arrows sit at
# 16.9 and its line runs 9..26.8, 1 px longer on the right); heads 7.8 px across, 6.3 px tall, the same up and down.
EL_AX = 17.45
ARROWS = [("horizontal line: the ground the arrows push from", rect(EL_AX - 8.9, 15.9, EL_AX + 8.9, 17.7), "navy"),
          ("a short upright block crossing the line", rect(EL_AX - 2.15, 14.0, EL_AX + 2.15, 19.0), "navy"),
          ("up arrow: triangular head", poly((EL_AX + 0.1, 4.6), (EL_AX + 3.9, 10.9), (EL_AX - 3.9, 10.9), (EL_AX - 0.1, 4.6)), "navy"),
          ("up arrow: short neck (a light gap parts it from the block)", rect(EL_AX - 1.7, 10.5, EL_AX + 1.7, 13.2), "navy"),
          ("down arrow: short neck (a light gap parts it from the block)", rect(EL_AX - 1.7, 19.8, EL_AX + 1.7, 22.5), "navy"),
          ("down arrow: triangular head", poly((EL_AX - 3.9, 22.1), (EL_AX + 3.9, 22.1), (EL_AX + 0.1, 28.4), (EL_AX - 0.1, 28.4)), "navy")]
S(id="tab-elevation", circle=(16.0, 16.0, 15.5), ground="auto_circle", shadow=False,
  how="blue glass tab bubble; glyph: up and down arrows on a ground line, all mirrored about the bubble's axis x 17.45 "
      "(fix round): a horizontal line (x 8.55..26.35), a short upright block crossing it, and above and below it, each parted from the block by a 1 px light gap, an arrow of a short "
      "neck and a triangular head", glyph=ARROWS)
S(id="elevation-up-down-tab", circle=(16.0, 16.0, 15.5), ground="auto_circle", shadow=False,
  how="the same up and down arrows on their line as tab-elevation (same numbers, mirrored about the bubble's axis x 17.45), on the lit tab bubble", glyph=ARROWS)

# tab-house, idle cut at (0, 400)
S(id="tab-house", circle=(16.0, 16.0, 15.5), ground="auto_circle", shadow=False,
  how="blue glass tab bubble; glyph: a house: a wide gable roof with eaves overhanging both walls and a chimney on its "
      "right slope, a wall with three small light windows in a row under the eaves and a light door in the middle",
  glyph=[("roof: gable with overhanging eaves", poly((16.0, 5.6), (17.2, 5.6), (25.8, 12.4), (25.8, 13.6), (7.8, 13.6),
                                                     (7.8, 12.2)), "navy"),
         ("chimney on the right slope", rect(20.0, 6.0, 23.0, 10.0), "navy"),
         ("wall", rect(9.6, 13.4, 23.8, 24.4), "navy"),
         ("three windows under the eaves", U(rect(12.6, 13.8, 14.8, 15.9), rect(15.6, 13.8, 17.8, 15.9), rect(18.6, 13.8, 20.8, 15.9)),
          "hole"),
         ("door", rect(15.0, 17.6, 19.0, 23.6), "hole")])

# tab-grass, idle cut at (0, 400)
BLADES = [(8.4, 11.9, 1.0), (10.4, 13.6, 0.8), (12.4, 11.4, 1.0), (14.5, 13.6, 0.9), (16.4, 12.8, 1.0), (19.6, 10.3, 1.3),
          (21.6, 13.8, 0.8), (23.6, 12.2, 1.2)]
S(id="tab-grass", circle=(16.0, 16.0, 15.5), ground="auto_circle", shadow=False,
  how="blue glass tab bubble; glyph: a tuft of grass: a low solid band of turf (x 7.6..25.9, y 14.6..19.7) with eight "
      "pointed blades rising from it, tall ones (x 8.4, 12.4, 16.4, 19.6 the tallest, 23.6) and short ones between them",
  glyph=[("turf: a low solid band", rect(7.6, 14.6, 25.9, 19.7, 0.5), "navy")] +
        [(f"blade {i + 1}: a pointed spike", poly((x - h, 15.2), (x, y), (x + h, 15.2)), "navy")
         for i, (x, y, h) in enumerate(BLADES)])


if __name__ == "__main__":
    import bel
    want = sys.argv[1:]
    for sp in SPECS:
        if not want or sp["id"] in want:
            bel.build(sp)
