"""hud-golfers-mode, round 3 (2026-11-24, verdict "fix": "People silhouette still low detail").

Round 2 drew each golfer as a smoothed blob with a light line painted over the join, a ring for the hand and a fedora-like cap;
at 16x its outlines were 1..2 px off the art's in most rows and the small features (visor gap, nose, chin, the hand's hole,
the hair's flip and notch) were rounded away. This round reads every outline row by row from the ACTIVE cut's coverage
(coverage = (200 - lum) / (200 - 32), so a pixel's dark share gives the edge to a tenth of a pixel)
and draws the two figures as separate navy masses with the art's own gaps between them.

Sheet coordinates of the active cut (100, 203); pixel (i, j) covers [i, i+1) x [j, j+1). Moved to each cut by els_modes.A.

FRONT golfer (smaller, facing left)
  cap      a dome crown (left edge x 120.6 at row 219 .. 118.7 at row 223) with a small button on top (123.8..126, row 216.3) and
           its back sloping down to a point at (131.6, 218.9); a wedge VISOR from the crown's front at x 118.5 to a rounded tip
           at (113.8, 226.6): top edge (118.8, 224.5) .. (114.6, 225.9), bottom edge 227.3 at x 114..116 rising to 226.3 at
           x 118..119; under it a light gap (x 116.5..120, rows 226.4..229) where the face is set back
  face     brow x 120.3 (row 227), nose tip (118.7, 230.5), upper lip 120.4 (rows 232..233), chin (121.1, 235.6) with its
           underside at 236.3, the jaw's underside at 235.6 from x 122.5 to the neck at 124.5; the back of the head (the gap's
           left side) 126.7 at rows 223..227, pinched to 124.4 at row 230 where the back golfer's nose comes in
  neck     x 124.45..127.4 (rows 235.6..237), the shoulder spreading to 132.6 at row 237
  body     the chest slanting forward from (124.8, 238) to (119.7, 243.6), then nearly straight (118.8..119.6) down to the ring;
           its back (the lower gap's left side) 134.2 at rows 239..241 curving in to 128.8 at rows 250..251 and out again
  hand     a hooked hand raised to the chest: an inverted U (outer 112.45..118.7, top 236.45), a hole open at the bottom
           (115.2..118.45, from row 238.8), the left leg 2.5..3 px ending round at row 242, the right leg about 1.2 px running
           down and to the right into the chest at (120..122, 242..243)
BACK golfer (bigger, facing left)
  head     top (136, 218.8); its face is the right side of the upper gap: forehead 132.7 (row 219) .. 128.45 (rows 224..227),
           nose tip (126.7, 229.5), lips 127.6..128.3 (rows 230..232), chin 128.35..128.45 (rows 233..235.3), the chin's
           underside at 235.5 out to x 132.4, then the throat and chest (the lower gap's right side) 133.3 (236.5) .. 136.2
           (240.5) .. 131.55 (250.5) .. 133.7 (254.5)
  hair     the back of the head 139.5 (219) .. 144.6 (226..228) .. 143.8 (230.5), then a bob that FLIPS OUT to a tip at
           (149.6, 235.2) (top edge 233.2..234.8, bottom edge 236.6..237.6), a notch under it at (144.55, 238.5..239.5)
  back     the shoulder running down and out from the notch (145.5, 241.5) .. (148.5, 244.4) into the ring
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import els_modes as M  # noqa: E402
from geom import poly, circle, outlined, inter  # noqa: E402

RING = (130.01, 232.94, 22.94)
# the inner navy ring: round 2 drew it at r 21.74 (RING r - 1.2); its coverage peaks at r 22.5..23.5 in every 10-degree sector
# of the active cut (radial profile 2026-11-24), so it is drawn at r 22.85, 2.3 px wide (21.7..24.0)
INNER_R = 22.85

FRONT = [
    # button and crown top, going clockwise (right side first)
    (124.9, 216.3), (126.0, 216.8), (126.6, 217.6), (128.2, 218.0), (130.2, 218.4), (131.6, 218.9),
    # the cap's back slope, then the back of the head (the upper gap's left side)
    (130.0, 219.5), (128.9, 220.3), (128.5, 221.5), (127.6, 222.5), (126.8, 223.5), (126.7, 225.0), (126.7, 226.5),
    (126.4, 227.5), (125.8, 228.4), (124.7, 229.4), (124.4, 230.4), (125.4, 231.4), (126.6, 232.3), (126.8, 233.5),
    (126.8, 235.0), (127.3, 236.3),
    # neck into the shoulder, then the back (the lower gap's left side)
    (127.6, 236.8), (128.6, 237.0), (131.0, 236.9), (132.6, 237.3), (133.6, 238.2), (134.2, 239.2), (134.2, 241.0),
    (133.7, 242.4), (133.2, 243.6), (132.6, 244.6), (132.2, 245.5), (131.6, 246.5), (131.0, 247.4), (130.2, 248.4),
    (129.4, 249.4), (128.8, 250.5), (128.8, 251.5), (129.4, 252.4), (130.5, 253.4), (131.8, 254.4), (132.6, 257.0),
    # bottom (clipped by the ring), then the front edge of the body going up
    (118.6, 258.0), (119.5, 252.0), (119.6, 250.4), (118.8, 249.0), (118.8, 247.5), (119.3, 246.4), (119.5, 245.0),
    (119.7, 243.6),
    # the chest slanting back up to the neck
    (121.0, 242.6), (122.3, 242.2), (123.2, 241.3), (124.1, 240.4), (124.7, 239.3), (124.8, 238.2), (124.45, 237.0),
    (124.45, 236.0),
    # jaw underside, chin, lips, nose, brow
    (124.5, 235.6), (123.0, 235.6), (122.5, 236.0), (121.6, 236.3), (121.1, 235.6), (120.7, 234.5), (120.45, 233.4),
    (120.4, 232.4), (119.4, 231.4), (118.7, 230.5), (119.4, 229.5), (119.8, 228.5), (120.3, 227.4), (120.2, 226.4),
    # the crown's front (the visor is its own wedge) and its rounded top left
    (118.6, 225.2), (118.3, 224.2), (118.7, 223.0), (118.9, 222.0), (119.2, 221.0), (119.6, 220.0), (120.6, 219.2),
    (121.6, 218.5), (122.8, 217.9), (123.8, 217.4), (124.0, 216.7),
]
VISOR = [(118.8, 224.5), (116.5, 225.2), (114.6, 225.9), (113.8, 226.6), (114.2, 227.3), (115.5, 227.33), (116.5, 227.2),
         (117.2, 226.6), (118.2, 226.2), (119.0, 226.33), (120.4, 226.7), (120.4, 225.0)]
HAND = [(113.4, 242.0), (112.7, 241.0), (112.5, 239.8), (112.45, 238.5), (112.7, 237.4), (113.4, 236.8), (114.5, 236.5),
        (116.2, 236.45), (117.6, 236.6), (118.4, 237.2), (118.7, 238.2), (119.2, 239.2), (119.45, 240.3), (119.6, 241.3),
        (120.4, 242.2), (122.4, 242.4), (121.0, 243.3), (119.6, 243.3), (119.3, 242.1), (118.7, 241.1), (118.45, 240.2),
        (118.2, 239.3), (117.6, 238.85), (116.8, 238.8), (116.2, 239.3), (115.6, 240.4), (115.2, 241.5), (114.8, 242.15)]
BACK = [
    # top and the back of the head, clockwise
    (136.0, 218.8), (137.8, 218.85), (139.3, 219.2), (140.6, 219.8), (141.6, 220.6), (142.4, 221.5), (142.8, 222.5),
    (143.3, 223.5), (143.7, 224.5), (144.3, 225.5), (144.6, 226.6), (144.6, 228.0), (144.1, 229.4), (143.8, 230.5),
    (144.2, 231.5), (144.9, 232.4),
    # the bob flipping out, its tip, the notch under it
    (146.0, 233.2), (147.5, 233.8), (148.8, 234.2), (149.6, 234.8), (149.6, 235.6), (148.8, 236.6), (147.4, 237.3),
    (145.8, 237.6), (144.6, 238.2), (144.55, 239.5),
    # the shoulder and back into the ring, the bottom (clipped)
    (144.8, 240.5), (145.5, 241.5), (146.2, 242.5), (147.0, 243.5), (148.5, 244.4), (152.0, 246.5), (152.0, 258.0),
    (133.6, 258.0),
    # the chest and throat going up (the lower gap's right side)
    (133.7, 254.5), (133.4, 253.5), (131.8, 252.5), (131.55, 251.5), (131.55, 250.5), (131.8, 249.5), (132.45, 248.5),
    (132.8, 247.5), (133.45, 246.5), (134.3, 245.5), (134.7, 244.5), (135.3, 243.5), (135.7, 242.5), (135.8, 241.5),
    (136.2, 240.5), (135.7, 239.5), (135.3, 238.5), (134.3, 237.5), (133.3, 236.5),
    # the chin's underside, chin, lips, nose, forehead
    (132.4, 235.6), (130.0, 235.5), (128.4, 235.3), (128.35, 234.3), (128.45, 233.4), (128.3, 232.4), (127.8, 231.5),
    (127.6, 230.5), (126.7, 229.5), (127.4, 228.5), (128.3, 227.5), (128.45, 226.0), (128.45, 224.5), (129.2, 223.4),
    (129.6, 222.4), (130.2, 221.3), (130.6, 220.3), (131.5, 219.6), (132.7, 219.1), (134.0, 218.9),
]


def golfers():
    o = (100, 203)
    parts = [
        ("inner ring: a 2.3 px navy circle line concentric with the rim", outlined(circle(RING[0], RING[1], INNER_R), 2.3), "navy"),
        ("front golfer: cap (dome crown with a button, its back sloping to a point), face in profile (brow, nose, lips, "
         "jutting chin, jaw), thin neck, shoulder, the chest slanting forward and the back curving in above the waist",
         poly(*FRONT, smooth=1), "navy"),
        ("front golfer: the cap's visor, a wedge jutting left to a rounded tip (a light gap under it where the face is set back)",
         poly(*VISOR), "navy"),
        ("front golfer: a hooked hand raised to the chest (an inverted U with a hole open at the bottom; the thin right leg "
         "runs down into the chest)", poly(*HAND, smooth=1), "navy"),
        ("back golfer: a bigger head (forehead, nose, lips, chin), the bob flipping out behind the neck with a notch under it, "
         "throat, chest and the shoulder down to the ring",
         poly(*BACK, smooth=1), "navy"),
    ]
    inside = circle(RING[0], RING[1], INNER_R - 1.0)
    return [(n, M.A(s if n.startswith("inner ring") else inter(s, inside), *o), k) for n, s, k in parts]


SPEC = dict(M.SPECS[2])
assert SPEC["id"] == "hud-golfers-mode"
SPEC["glyph"] = golfers()
SPEC["how"] = ("large glass bubble (pale blue idle, yellow active); glyph inside an inner navy ring: two golfers in profile facing "
               "left, every outline read row by row from coverage (round 3). Front: a cap (dome crown with a button, its back "
               "sloping to a point, a wedge visor with a light gap under it), face (brow, nose, lips, jutting chin, jaw), thin "
               "neck, shoulder, chest slanting forward, a hooked hand raised to the chest (inverted U with a hole, its thin leg "
               "running into the chest). Back, bigger: forehead, nose, lips, chin, throat and chest, a bob flipping out behind "
               "the neck with a notch under it, the shoulder down to the ring. The light gaps between the two figures are the "
               "art's own gaps (the figures are separate masses)")

if __name__ == "__main__":
    import bel
    bel.build(SPEC)
