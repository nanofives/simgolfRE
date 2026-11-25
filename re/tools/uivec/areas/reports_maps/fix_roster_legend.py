"""P41 fix round: deliver roster-legend (redo)."""
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import common as C          # noqa: E402
import deliver as D         # noqa: E402
import deliver2 as D2       # noqa: E402
import roster_legend2 as LG  # noqa: E402

rgb, m = C.sheet(LG.SHEET)
x, y, w, h = 14, 528, 726, 26
art = rgb[y:y + h, x:x + w]
base = np.empty_like(art)
base[:] = LG.LAV
fb = np.empty_like(rgb)
fb[:] = LG.LAV
sc = LG.scene()
det = [D.tile(rgb[b:b + 26, a:a + 64], fb[b:b + 26, a:a + 64], sc, (a, b), 10) for a, b in ((14, 528), (136, 528))]
det += [D.tile(rgb[b:b + 26, a:a + 64], fb[b:b + 26, a:a + 64], sc, (a, b), 10) for a, b in ((258, 528), (380, 528))]
det += [D.tile(rgb[b:b + 26, a:a + 64], fb[b:b + 26, a:a + 64], sc, (a, b), 10) for a, b in ((494, 528), (616, 528))]
det += [D.tile(rgb[b:b + 26, a:a + 64], fb[b:b + 26, a:a + 64], sc, (a, b), 10) for a, b in ((86, 528), (690, 528))]
D2.deliver("roster-legend", [("idle", art, base, sc, (x, y))],
           "six legend slots, each a mark joined by a 2 px wire (y 540..542) to an outlined pill (2 px line, line centre y 532..550, "
           "corners r 7 with a short straight end, ends read off row 541). Marks: navy, silver, gold and red balls, each INSIDE a "
           "ring (circle line r 6.7, 2 px, the pills' colour), lit from the top left by a radial gradient solved on the ball's own "
           "pixels; the redrawn camera and heart (roster-icon-camera / -heart) moved to their legend places. Full strip 2x on top, "
           "every slot at 10x below (navy, silver, gold, red, camera, heart, then the navy and the heart pill ends)",
           "redone from a new reading: the balls now sit inside their outline rings (round one drew a bigger bare sphere and lost "
           "the ring), the ball light is the art's top-left highlight; the pill ends are the art's r 7 corners with a straight "
           "piece (were half circles r 9, too round) and the pills' ends are measured per pill; the wire is the art's 2 px; the "
           "camera and heart are the new ones",
           z=2, extra_tiles=det)
