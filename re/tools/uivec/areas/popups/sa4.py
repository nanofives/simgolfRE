"""P41 popups round 4 (2026-11-25): trans-small-arrows, built on round 3's version B. His note: "shading looks weird on
bottom arrow". The up cells keep round 3's B unchanged; the down cells (cream 220, orange 270) are redrawn.

The down cell read again at 16x with every pixel dumped as hex (TransPopups.pcx rows 220-231 / 270-281):
- the triangle's face is lit TOP to BOTTOM, not across: two light cream rows under the bar (f5e9c8; orange fee88a) with a white
  corner at the upper left (faf6e5 x 10-12; fef1c6), then eed697 -> e8ca7d -> d4c27b -> c5ac6a row by row toward the apex
  (orange facc3d -> fcc102 -> f6b402 -> f6a402);
- both lower edges carry a band of their own: the LEFT edge a thin dim olive-grey line (a8a398 / b8a988; orange c5ac6a /
  ceae40) that turns into the plate's lilac; the RIGHT edge a white band about 1.7 px across (f5f6f9) from the top right corner
  down to the apex, with the face darkest (c5ac6a; f6a402) just inside it and light lilac (e8e8f2 -> d8d9ea -> bebfdc) just
  outside it, one continuous highlight straddling the edge that fades into the plate (no darker gap between them);
- the plate: the dark bar on top (8183ab / 7b7ca3 rows 220-221; one row of 6870ba in the orange pair), a light rim at its two
  upper ends (a3a4ce left, ababd2 / b3b5d7 right), the face 9597c6 / 9193c0, and its lower edges darkening row by row
  (8183ab -> 7b7ca3 -> 666887 -> 555771 -> 4a4d65 -> 3c4054), darker on the right at the same row (row 227: 666887 left,
  373949 right); the bottom row near black.
Round 3's B against that, what read as weird: (1) its face was a diagonal gradient, light on the left and dark toward the
lower right, so with the white band the triangle looked lit from both sides; (2) the left edge had no shade line, so the left
half was the brightest part; (3) outside the white band a separate lilac stroke sat on the shadow with a dark gap between,
reading as two parallel lines; (4) the plate's shadow was a soft blob at the bottom instead of rows darkening along the rim;
(5) the white band ended in a hard square top and ran past the apex into the dark bottom.

py -3.12 sa4.py
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sa3              # noqa: E402  (imports w3 / lens3 / smalls2; w4.apply() below repoints the outputs)
import common as c      # noqa: E402
import w4               # noqa: E402
from auto import mv     # noqa: E402
from lens3 import H, V, I, Sb, edge_seg  # noqa: E402
from sa3 import LB, lin, plate_b, TP, CELLS  # noqa: E402

w4.apply()

PD = {
    1: {"body": "9597c6", "bar": "7e80a7", "barEnd": "8c8ebb", "rimL": "a3a4ce", "rimR": "b0b2d6",
        "rimDkL": "666887", "rimDkR": "3a3c4e", "bottom": "3a3c4e",
        "top": "f5e9c8", "corner": "faf6e5", "f1": "eed697", "f2": "e8ca7d", "f3": "d4c27b", "f4": "c5ac6a",
        "edgeL": "b0a68a", "valley": "c5ac6a", "white": "f5f6f9", "glow": "d8d9ea", "glow2": "b3b5d7"},
    2: {"body": "9597c6", "bar": "6870ba", "barEnd": "8183ab", "rimL": "a3a4ce", "rimR": "b0b2d6",
        "rimDkL": "666887", "rimDkR": "3a3c4e", "bottom": "2f303e",
        "top": "fee88a", "corner": "fdf0c8", "f1": "facc3d", "f2": "fcc102", "f3": "f6b402", "f4": "f6a402",
        "edgeL": "c9ab4a", "valley": "f4a402", "white": "f7f8fb", "glow": "d8d9ea", "glow2": "b3b5d7"},
}


def down_geom4(y0, k):
    """The down triangle (round 3's corners; the apex lifted to the art's last light row, 230.6)."""
    o = y0 - 220 - (1 if k == 2 else 0)
    tl, tr, cr, apex = (8.6, 222.0 + o), (31.7, 222.0 + o), (31.7, 224.4 + o), (21.6, 230.6 + o)
    D = {"t": "poly", "rings": [[list(tl), list(tr), list(cr), list(apex)]]}
    return o, tl, tr, cr, apex, D


def down_b4(y0, k):
    C = PD[k]
    P = plate_b(True, y0)
    o, tl, tr, cr, apex, D = down_geom4(y0, k)
    bar_bot = (222.0 if k == 1 else 221.0) + o + (0 if k == 1 else 1)   # pair 1: rows 220-221; pair 2: row 270 only
    lay = [
        # the plate: its face lit evenly, the lower rows darkening toward a near-black bottom
        LB(P, lin((0.0, y0 + 4.0), (0.0, y0 + 12.0), [(0.0, C["body"]), (0.2, "9193c0"), (0.4, "8889b3"),
                                                       (0.56, "7b7ca3"), (0.7, "5e607e"), (0.83, "44475b"),
                                                       (0.95, C["bottom"])])),
        # the rim's shadow along the lower edges, row by row, darker on the right at the same row
        LB(I(Sb(P, mv(P, -0.8, -2.4)), H(y0 + 4.6)),
           lin((5.0, 0.0), (35.0, 0.0), [(0.0, C["rimDkL"]), (0.5, "555771"), (1.0, C["rimDkR"])]), soft=0.7, clip=P),
        # light at the plate's two upper ends
        LB(I(Sb(P, mv(P, 1.0, 0.0)), H(y0 + 3.2, below=False)), C["rimL"], soft=0.4, clip=P),
        LB(I(Sb(P, mv(P, -1.0, 0.0)), H(y0 + 4.6, below=False)), C["rimR"], soft=0.4, clip=P),
        # the dark bar on top, its ends easing into the plate
        LB(c.rr(4.0, y0, 32.0, bar_bot, 0.0), lin((4.0, 0.0), (32.0, 0.0),
                                                  [(0.0, C["body"]), (0.2, C["barEnd"]), (0.3, C["bar"]),
                                                   (0.82, C["bar"]), (0.92, C["barEnd"]), (1.0, C["body"])])),
        # the highlight's outer half: light lilac just outside the right edge, fading into the plate (drawn under the
        # triangle so it meets the white band with no gap)
        LB(I(edge_seg(cr, apex, 0.02, 0.84, 0.6, 2.6), H(bar_bot + 0.8)),
           lin(cr, apex, [(0.0, C["glow"]), (0.55, C["glow"]), (1.0, C["glow2"])]), soft=0.9, clip=P),
        # the triangle: lit from the top, darkening row by row toward the apex
        LB(D, lin((0.0, 222.0 + o), (0.0, 229.5 + o), [(0.0, C["top"]), (0.22, C["top"]), (0.36, C["f1"]),
                                                      (0.52, C["f2"]), (0.72, C["f3"]), (1.0, C["f4"])])),
        # the left edge: a thin dim line (its shaded side), below the light top rows
        LB(I(edge_seg(apex, tl, 0.0, 0.86, -0.55, 1.2), H(223.6 + o)), C["edgeL"], soft=0.45, clip=D),
        # the face darkest just inside the white band (the valley), up to the top right corner
        LB(edge_seg(cr, apex, -0.25, 0.9, -2.3, 1.8), C["valley"], soft=0.8, clip=D),
        # the light top rows over the valley's top end, and the white corner at the upper left
        LB(I(D, H(223.9 + o, below=False), V(27.0, right=False)), C["top"], soft=0.6, clip=D),
        LB(I(D, H(223.0 + o, below=False), V(13.0, right=False)), C["corner"], soft=0.5, clip=D),
        # the white band along the right edge, round at both ends, from the top right corner to just above the apex
        LB(I(edge_seg(cr, apex, -0.05, 0.9, -0.55, 1.45), H(bar_bot + 0.9)), C["white"], soft=0.4,
           clip={"t": "grow", "of": D, "d": 0.35}),
    ]
    return lay


CHANGED = ("his note 'shading looks weird on bottom arrow': the down triangle's face is now lit top to bottom as in the art "
           "(light cream top rows with the white corner, then darker row by row to the apex; round 3 shaded it across, light "
           "left to dark right, so with the white band it looked lit from both sides); its left edge has the art's thin dim "
           "line; the white band on the right edge is one highlight with light lilac just outside it fading into the plate "
           "(round 3 had a separate lilac stroke with a dark gap, two parallel lines), round at both ends and stopping at the "
           "apex; the face is darkest just inside the band; the plate's shadow is its lower rim darkening row by row, darker on "
           "the right, near black at the bottom (round 3: a soft blob). Up cells unchanged from round 3's B")


def deliver():
    import numpy as np
    w4.apply()
    w4.check()
    states, rows, out = [], [], {}
    for st, y0, kind, k in CELLS:
        box = (0, y0, 35, y0 + 12)
        lay = sa3.up_b(y0, k) if kind == "up" else down_b4(y0, k)
        it = c.item(box, lay, f"trans-small-arrows {st}")
        sc = [it]
        mad, w16 = c.score(sc, TP, box, c.FLAT)
        print(f"  {st:8s} mad {mad:6.2f} w16 {w16:5.3f}", flush=True)
        rows.append((sc, TP, box, c.FLAT))
        out[st] = {"box": list(box), "scene": sc}
        states.append(mad)
    w4.check()
    p = c.compare(rows, "trans-small-arrows", zoom_k=8)
    c.save_scene("trans-small-arrows", {"id": "trans-small-arrows", "sheet": "Interface/" + TP, "states": out})
    w4.record("trans-small-arrows", "done", CHANGED, "")
    print("mean mad", round(float(np.mean(states)), 2))
    return p


if __name__ == "__main__":
    print(f"[1/1] trans-small-arrows {deliver()}", flush=True)
