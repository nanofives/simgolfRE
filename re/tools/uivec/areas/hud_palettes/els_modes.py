"""The three large mode bubbles of 3mainLowerLeft.pcx (course, buildings, golfers): pale blue glass when idle, yellow when the
mode is active, each with an inner navy ring and a navy picture inside it.

Read on the ACTIVE cuts (yellow ground, clearest contrast) with bmap.py (binary map at coverage 0.5) and a 7x enlargement,
2026-11-24; the inner ring fitted on its clean top arc. Numbers below are SHEET coordinates of the active cut and are moved to
the idle cut's local frame by A() (the idle cut sits 100 px to the left; the glyph's offset between states is matched).
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from geom import rect, line, poly, circle, oval, outlined, curve, U, moved, inter, sub  # noqa: E402


def A(shape, ox, oy):
    return moved(shape, -ox, -oy)


# ---------------------------------------------------------------------------------------------------- course (active (100, 1))
def course():
    o = (100, 1)
    parts = [
        ("inner ring: a 2.8 px navy circle line concentric with the rim", outlined(circle(136.87, 37.54, 27.45), 2.8), "navy"),
        ("flag pole: curving from its top on the right down to the green", line((142.6, 14.2), (139.6, 17.0), (137.6, 21.5),
                                                                                (136.9, 30.0), (137.2, 43.5), w=1.9, smooth=2), "navy"),
        ("pennant: a triangle flying right from the pole (a 1 px light fold beside the pole)",
         poly((141.0, 17.2), (143.4, 15.4), (150.0, 21.4), (150.0, 23.0), (141.0, 23.0)), "navy"),
        ("pennant tail: a streamer from its lower corner, down to the right",
         poly((145.0, 23.6), (147.6, 22.6), (150.4, 22.6), (153.0, 24.8), (155.4, 26.2), (155.2, 27.2), (148.0, 27.2), (145.0, 24.6)), "navy"),
        ("left hill: a band falling from the ring at the left to the pole",
         poly((106.0, 29.0), (111.0, 29.4), (115.0, 30.5), (120.0, 31.5), (123.5, 32.6), (126.4, 34.4), (128.6, 35.6), (130.4, 37.4),
              (131.4, 39.0), (130.6, 40.0), (127.0, 40.8), (122.0, 41.6), (117.0, 42.6), (114.0, 43.4), (111.0, 44.8), (106.0, 46.0)),
         "navy"),
        ("right hill: a band falling from the ring at the right to the pole",
         poly((141.0, 38.4), (142.6, 36.6), (144.6, 34.6), (147.2, 33.0), (149.4, 31.8), (152.4, 30.8), (155.4, 29.8), (160.2, 28.8),
              (166.0, 28.6), (166.0, 44.6), (162.0, 44.4), (159.0, 43.4), (156.0, 42.4), (152.0, 41.4), (145.0, 40.4), (141.4, 39.8)),
         "navy"),
        ("green: a big rounded mass at the lower left, the pole standing on it",
         poly((131.0, 43.0), (139.0, 43.0), (143.4, 43.6), (146.4, 45.2), (146.2, 47.4), (144.6, 49.0), (141.6, 50.4), (140.4, 52.0),
              (142.6, 53.4), (146.0, 54.4), (148.0, 56.0), (149.0, 58.4), (149.0, 61.0), (152.6, 62.0), (150.6, 63.6), (147.6, 64.6),
              (144.6, 66.0), (140.0, 67.0), (132.0, 67.0), (126.0, 65.4), (121.0, 62.6), (117.6, 59.6), (114.6, 56.6), (112.8, 53.8),
              (111.4, 51.4), (114.6, 49.4), (117.6, 47.6), (121.6, 45.6), (126.0, 44.0)), "navy"),
        ("cup: a small light notch in the green at the foot of the pole", rect(138.9, 43.8, 141.0, 45.8), "hole"),
        ("island: a dark bean in the fairway", oval(152.6, 49.6, 5.6, 3.9, -18.0), "navy"),
        ("fairway edge: a thin dark curve on the right, from the hill down to the green",
         line((163.6, 44.6), (162.4, 48.0), (160.8, 52.4), (158.4, 55.6), (155.6, 58.6), (152.6, 61.0), (149.6, 62.6), w=2.2, smooth=2),
         "navy"),
    ]
    inside = circle(136.87, 37.54, 27.45 - 1.0)      # the picture stays inside the inner ring
    return [(n, A(s if n.startswith("inner ring") else inter(s, inside), *o), k) for n, s, k in parts]


# ------------------------------------------------------------------------------------------------- buildings (active (100, 100))
def buildings():
    o = (100, 100)
    ring = (133.51, 133.25, 25.26)
    parts = [
        ("inner ring: a 2.8 px navy circle line concentric with the rim", outlined(circle(*ring), 2.8), "navy"),
        ("house: gable roof with eaves wider than the walls", poly((133.0, 114.8), (134.4, 114.8), (147.0, 124.2), (147.0, 125.0),
                                                                    (120.6, 125.0), (120.6, 124.2)), "navy"),
        ("house: chimney on the right slope", rect(138.0, 114.9, 143.0, 119.0), "navy"),
        ("house: wall", rect(123.0, 124.6, 145.0, 140.2), "navy"),
        ("house: three windows in a row", U(rect(127.0, 126.0, 131.0, 130.0), rect(132.0, 126.0, 136.0, 130.0),
                                            rect(137.0, 126.0, 141.0, 130.0)), "hole"),
        ("house: door", rect(131.0, 132.0, 137.0, 140.0), "hole"),
        ("up arrow, left of the house: head", poly((116.5, 128.6), (120.3, 133.0), (112.7, 133.0)), "navy"),
        ("up arrow: shaft", rect(115.0, 132.6, 118.0, 138.0), "navy"),
        ("down arrow, right of the house: shaft", rect(149.0, 128.8, 152.0, 134.2), "navy"),
        ("down arrow: head", poly((146.8, 133.8), (154.2, 133.8), (150.5, 138.4)), "navy"),
        ("ground: the platform's front edge, the lower half of an ellipse (3 px)",
         inter(outlined({"t": "ellipse", "c": [133.3, 140.6], "r": [21.6, 17.9], "rot": 0.0}, 3.0), {"t": "half", "p": [0, 141.0], "n": [0, -1]}),
         "navy"),
        ("ground: the line the house stands on", rect(100.0, 140.0, 167.0, 142.0), "navy"),
        ("ground: second grid line", rect(100.0, 144.0, 167.0, 146.0), "navy"),
        ("ground: third grid line", rect(100.0, 149.0, 167.0, 151.0), "navy"),
        ("ground: left grid line, leaning out", line((123.6, 141.0), (119.4, 152.6), w=2.0), "navy"),
        ("ground: middle grid line", line((133.5, 141.0), (133.5, 158.6), w=2.0), "navy"),
        ("ground: right grid line, leaning out", line((143.6, 141.0), (148.2, 152.6), w=2.0), "navy"),
    ]
    inside = circle(ring[0], ring[1], ring[2] - 1.0)
    return [(n, A(s if n.startswith("inner ring") else inter(s, inside), *o), k) for n, s, k in parts]


# --------------------------------------------------------------------------------------------------- golfers (active (100, 203))
def golfers():
    o = (100, 203)
    ring = (130.01, 232.94, 22.94)
    parts = [
        ("inner ring: a 2.6 px navy circle line concentric with the rim", outlined(circle(ring[0], ring[1], ring[2] - 1.2), 2.6), "navy"),
        ("front golfer: cap crown", poly((118.0, 224.8), (118.6, 220.6), (120.6, 218.6), (123.0, 216.4), (125.6, 216.0),
                                         (127.6, 217.4), (129.8, 218.4), (129.2, 222.0), (127.2, 224.8)), "navy"),
        ("front golfer: cap visor jutting left", poly((113.8, 225.0), (126.8, 224.6), (126.8, 226.4), (118.0, 227.0), (113.8, 226.8)),
         "navy"),
        ("front golfer: face in profile, facing left", poly((119.8, 226.2), (126.8, 226.2), (126.8, 236.6), (121.4, 236.2),
                                                             (120.8, 233.4), (119.0, 231.4), (119.2, 228.6)), "navy"),
        ("front golfer: a curled hand raised at the chin", outlined(circle(115.7, 239.6, 2.5), 1.9), "navy"),
        ("back golfer: a bigger round head behind, a light outline parting it from the front one",
         poly((128.4, 226.0), (129.8, 221.4), (132.8, 219.0), (138.8, 218.6), (141.4, 220.0), (143.6, 223.0), (144.6, 227.0),
              (144.6, 232.0), (149.4, 233.8), (149.4, 235.6), (140.0, 235.8), (131.0, 235.4), (128.2, 233.0)), "navy"),
        ("both bodies: one dark mass down to the ring", poly((119.2, 236.4), (127.0, 236.0), (133.0, 236.4), (140.0, 235.8),
                                                             (146.4, 236.0), (148.6, 238.0), (146.6, 242.0), (153.0, 245.0), (153.0, 259.0),
                                                             (106.0, 259.0), (106.0, 250.0), (116.6, 251.2), (119.2, 248.0)), "navy"),
        ("light line parting the two bodies", line((133.6, 236.2), (133.0, 243.0), (130.6, 249.0), (130.6, 255.0), w=1.5, smooth=2),
         "hole"),
    ]
    inside = circle(ring[0], ring[1], ring[2] - 2.0)
    return [(n, A(s if n.startswith("inner ring") else inter(s, inside), *o), k) for n, s, k in parts]


SPECS = [
    dict(id="hud-course-mode", ref="active", circle=(38.5, 39.0, 38.0), ground="auto", shadow=False,
         how="large glass bubble (pale blue idle, yellow active); glyph inside an inner navy ring: a golf hole: a flag on a "
             "curving pole (pennant with a streamer tail), two hills falling toward the pole, a big rounded green at the "
             "lower left with the cup notch at the pole's foot, a dark island in the light fairway and the fairway's thin "
             "right edge curving down", glyph=course()),
    dict(id="hud-buildings-mode", ref="active", circle=(33.5, 37.0, 33.5), ground="auto", shadow=False,
         how="large glass bubble (pale blue idle, yellow active); glyph inside an inner navy ring: a house (gable roof, "
             "chimney, three windows, door) between an up arrow and a down arrow, standing on a ground grid in perspective "
             "(three cross lines, three lines leaning out, the platform's curved front edge)", glyph=buildings()),
    dict(id="hud-golfers-mode", ref="active", circle=(29.5, 36.0, 29.0), ground="auto", shadow=False,
         how="large glass bubble (pale blue idle, yellow active); glyph inside an inner navy ring: two golfers in profile "
             "facing left: a smaller front one in a cap with a visor and a hand raised at the chin, a bigger round head "
             "behind it parted by a light outline, their bodies one dark mass down to the ring with a light line between",
         glyph=golfers()),
]

if __name__ == "__main__":
    import bel
    want = sys.argv[1:]
    for sp in SPECS:
        if not want or sp["id"] in want:
            bel.build(sp)
