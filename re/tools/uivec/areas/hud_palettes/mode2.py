"""hud-golfers-mode, fix round (2026-11-24, verdict "redo"): a new reading of the two golfers (coverage digits of the active
cut, rows 214..262, and a 12x gridded view). Sheet coordinates of the active cut (100, 203); moved to each cut by els_modes.A.

What the art shows (two busts in profile, both facing left, the back one bigger):
  front golfer  a golf CAP: a dome crown with a small button on top (123.3, 216.2), its back sloping down to (129.5, 219.3),
                a VISOR jutting left from the crown's front to a rounded tip at (114, 226.5); under it the FACE in profile:
                brow at x 120, NOSE out to 118.3 at row 230, lips, CHIN at row 235.5; the NECK and SHOULDER going down-right
                into the body; a HAND raised to the chin as a curled fist (a ring open at its lower right, 112.5..118.5 x
                236.5..241.5) with the FOREARM running down-right from it into the body at (120.5, 243.8); the body's front
                edge straight down at x 119.5 to the ring.
  back golfer   a bigger round HEAD (top at 218.6, back of the head at x 144.4) whose own face profile is the LIGHT LINE
                that parts it from the front golfer: forehead at 128.4, NOSE out to 126.5 at row 229, lips, chin at 235.5,
                then the line runs down its throat and chest (134.6 at row 242) and fades at the ring (131.4, 254.6);
                behind the head a BUN of hair juts right to x 149.4 at rows 234..236, a light notch under it, then the
                SHOULDER and back running down-right to the ring.
Drawn as: one dark mass per figure (front head + visor + body, back head + bun + body), the light profile line on top (the
body's own paint showing through, one smooth path with round ends), the light gap between the two necks, the curled hand and
the forearm as their own parts.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import els_modes as M  # noqa: E402
from geom import poly, line, circle, outlined, arc, inter  # noqa: E402

RING = (130.01, 232.94, 22.94)
PROFILE = [(129.0, 218.6), (128.4, 221.0), (127.8, 223.6), (127.2, 226.4), (126.5, 229.0), (127.3, 231.0), (127.6, 233.6),
           (128.6, 235.4), (130.6, 236.4), (133.2, 237.4), (134.3, 239.0), (134.6, 242.0), (134.0, 245.0), (132.8, 247.0),
           (131.2, 249.0), (130.4, 251.0), (130.6, 253.0), (131.4, 254.6)]


def golfers():
    o = (100, 203)
    parts = [
        ("inner ring: a 2.6 px navy circle line concentric with the rim", outlined(circle(RING[0], RING[1], RING[2] - 1.2), 2.6), "navy"),
        ("front golfer: cap crown (a dome with a small button on top) and the head under it, face in profile facing left "
         "(brow, nose out at row 230, lips, chin)",
         poly((118.0, 224.8), (118.0, 222.2), (119.4, 220.0), (121.0, 217.2), (122.4, 216.2), (124.2, 216.1), (125.0, 217.3),
              (127.4, 218.3), (129.8, 219.2), (129.8, 236.0), (126.5, 236.6), (124.6, 235.9), (121.2, 235.8), (120.3, 234.8),
              (120.2, 232.4), (119.3, 231.4), (118.2, 230.6), (118.6, 229.6), (119.8, 228.6), (120.2, 227.0), (119.0, 226.8),
              smooth=1), "navy"),
        ("front golfer: cap visor jutting left to a rounded tip",
         poly((118.8, 224.6), (116.4, 225.0), (114.8, 225.6), (113.9, 226.4), (114.4, 227.0), (116.6, 226.8), (118.8, 227.0)), "navy"),
        ("front golfer: neck, shoulder and body, its front edge straight down to the ring",
         poly((125.0, 236.2), (129.8, 236.2), (133.6, 237.4), (134.6, 239.0), (134.6, 258.0), (119.4, 258.0), (119.5, 244.0),
              (120.6, 243.0), (122.2, 242.0), (123.6, 240.6), (124.3, 238.6)), "navy"),
        ("front golfer: a hand raised to the chin, a curled fist (a ring open at its lower right)",
         arc(115.6, 239.2, 2.15, 135.0, 265.0, 1.8), "navy"),
        ("front golfer: the forearm from the fist down into the body", line((117.4, 240.8), (119.0, 242.4), (120.8, 244.0), w=1.5,
                                                                             cap="round"), "navy"),
        ("back golfer: a bigger round head, a bun of hair jutting out behind it, the shoulder and back down to the ring",
         poly((127.4, 228.0), (128.6, 222.0), (130.4, 219.6), (132.5, 218.8), (136.0, 218.5), (139.5, 219.0), (141.6, 220.2),
              (142.8, 222.4), (143.8, 225.0), (144.3, 228.0), (144.4, 231.0), (145.2, 233.5), (147.8, 233.9), (149.4, 234.8),
              (148.8, 236.6), (146.6, 237.4), (144.9, 238.1), (144.7, 239.7), (145.8, 242.0), (148.4, 244.0), (153.5, 246.0),
              (153.5, 260.0), (130.0, 260.0), (130.4, 251.0), (132.8, 247.0), (134.6, 242.0), (133.2, 237.4), (128.6, 235.4),
              (127.4, 233.0), smooth=1), "navy"),
        ("back golfer's face in profile: the light line parting the two figures (forehead, nose, lips, chin, throat, chest)",
         line(*PROFILE, w=1.3, cap="round", smooth=1), "hole"),
        ("the light gap between the two necks", poly((128.0, 235.3), (131.0, 235.8), (133.8, 236.8), (134.5, 238.4), (131.2, 237.1),
                                                     (128.6, 236.6), smooth=1), "hole"),
    ]
    inside = circle(RING[0], RING[1], RING[2] - 2.0)
    return [(n, M.A(s if n.startswith("inner ring") else inter(s, inside), *o), k) for n, s, k in parts]


SPEC = dict(M.SPECS[2])
assert SPEC["id"] == "hud-golfers-mode"
SPEC["glyph"] = golfers()
SPEC["how"] = ("large glass bubble (pale blue idle, yellow active); glyph inside an inner navy ring (new reading): two golfers in "
               "profile facing left. Front: a cap (dome crown with a button, a visor jutting left), the face (nose, lips, chin), "
               "neck and body, a hand raised to the chin as a curled fist with its forearm running down into the body. Back, "
               "bigger: a round head with a bun of hair behind it, its face profile the light line that parts the two figures "
               "(forehead, nose, lips, chin, throat, chest), the shoulder and back down to the ring")

if __name__ == "__main__":
    import bel
    bel.build(SPEC)
