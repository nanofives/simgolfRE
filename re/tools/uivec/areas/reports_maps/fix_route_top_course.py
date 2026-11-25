"""P41 fix round: deliver route-top-course ("consistent shade")."""
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import common as C          # noqa: E402
import deliver as D         # noqa: E402
import deliver2 as D2       # noqa: E402
import route_tops as R      # noqa: E402
import route_tops2 as R2    # noqa: E402

st = R.load_styles()
ish = R2.load_ish()
bsh = R2.load_box()
rgb, m = C.sheet("route screens_course.pcx")
base = np.empty_like(rgb)
base[:] = R.LAV
sc = R2.scene("course", st, ish, bsh=bsh)
wins = [(30, 42, 44, 40), (732, 42, 44, 40), (0, 82, 44, 40), (756, 82, 44, 40), (280, 160, 44, 40), (488, 160, 44, 40)]
det = [D.tile(rgb[y:y + h, x:x + w], base[y:y + h, x:x + w], sc, (x, y), 8) for x, y, w, h in wins]
D2.deliver("route-top-course", [("idle", rgb, base, sc, (0, 0))],
           "lavender ground, the arched title plate, a tan banner sunk into the lavender (x 42.5..756.5 from y 51.5), the lavender "
           "slab in a navy line (x 5.2..795.8 from y 91.8, r 8) with the cream striped list sunk into it (x 18.5..780.5), nine "
           "lilac column pills on the slab line and a raised cream box (line x 288.9..510.9, bottom 188.9, r 6) hanging from it. "
           "Sunk recesses (banner, list): flat floor + ONE inner shadow (black at alpha %.2f with a soft rounded rectangle cut out, "
           "edges %.1f / %.1f px inside the left / top walls, softness %.1f, corner r %.1f), drawn over the stripes. Box: flat cream, "
           "its line, and the line's own shadow cast %.1f px down-right (alpha %.2f, softness %.1f) on every wall. Full view 1x, "
           "corners at 8x below (banner TL / TR, list TL / TR, box BL / BR)"
           % (ish["a"], ish["l"], ish["t"], ish["soft"], ish["rad"], bsh["dx"], bsh["a"], bsh["soft"]),
           "the shade was a nearest-edge bevel: mitred seams at every recess corner and an invented light band inside the right "
           "walls; now one inner shadow per recess (same numbers for banner and list, measured: floor x0.63 at the top / left walls "
           "on tan, cream and stripe alike, nothing on the right / bottom walls), so every corner and wall is lit the same way; the "
           "box's line now casts the same down-right shadow on all four walls (inside along the left wall, outside along the right "
           "wall and foot), as the art does",
           unsure="the outer lips (dark above / left, light right of each recess) are the round-one solved paint, unchanged; the same "
                  "inner shadow would apply to the approved employ / aura / value bands, which were not re-delivered",
           z=1, extra_tiles=det)
