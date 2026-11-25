"""course-title-badge: the open stadium frame round the course name (courseinfo.pcx, cut (48, 6), 183 x 58).

Read on the cut's runs and luminance profiles (2026-11-24): a frame whose outer edge is a box with elliptical left corners
(28 x 23, a straight stretch between them) and tighter right corners (15 x 19); the band is 3 px at the top and bottom,
5 px at the left and 4.5 px at the right; its line is light navy on the outside, mid navy, then a dark shadow on the inside
(the "line thin on top and thick below" of the flat frames). The inside is transparent. Local = sheet - (48, 6)."""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from geom import ebox, sub  # noqa: E402

OUTER = ebox(0.0, 0.0, 183.6, 58.0, 28.0, 23.0, 15.0, 19.0)
INNER = ebox(5.0, 3.0, 179.1, 55.0, 23.0, 20.0, 10.5, 16.0)
SPEC = dict(id="course-title-badge", parts=[("frame: the band between the outer box and the inner one", sub(OUTER, INNER), "frame")],
            how="an open frame: outer box with elliptical corners (left 28 x 23, right 15 x 19), band 3 px top and "
                "bottom, 5 px left, 4.5 px right, painted with a bevel measured on it (light outer line, mid navy, dark inner "
                "shadow); transparent inside")

if __name__ == "__main__":
    import json
    import numpy as np
    import fel
    import hp
    import bubble as B
    from hp import sdf
    import geom as G
    # the frame's paint: a bevel of its own shape measured on its pixels (depth from its edge, the edge's direction)
    st, src, x0, y0, rgb, mask = hp.crops(SPEC["id"])[0]
    shape = G.moved(SPEC["parts"][0][1], x0, y0)
    X, Y = B.grid(x0, y0, mask.shape[1], mask.shape[0])
    keep = mask & (sdf.sd(shape, X, Y) < 0.2)
    paint = B.measure_bevel_shape(rgb, keep, x0, y0, shape, [0.0, 0.6, 1.2, 2.0, 3.0, 4.0])
    SPEC["colours"] = {"frame": paint}
    fel.build(SPEC)
