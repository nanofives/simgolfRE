"""P41 popups fix round: head-select-left ("better shade").

The first round lit the lens with one fitted bevel paint over the intersection of its two arcs and the cell's top cut; its
nearest-edge normals jump where the edges meet, which drew a diagonal seam at the top left and a dark V at the bottom, and the
lilac wall left of the lens came out striped. Here the light is built as layers parallel to the arcs, each colour measured as
the median of the lens pixels at that distance from its arc (rows 128-235, the arrow left out):

  arc A (the left side, c (653.4, 130.5) r 221.6) is the tray's edge, not the lens': from it inward lie
    the tray's lilac wall, 6.5 px: 606088 at the edge, 585880 at 2 px, 505078 from 3.5 px;
    the lens' own left edge (circle A shrunk by 6.5): lit cell 404050 in its first px, 484858, 505060, 585868, body by 4 px;
                                                      dim cell a wide shadow, 283038 .. 585868 over 13 px;
  arc B (the right side, c (431.4, 178.2) r 81.1): lit 303040 at the edge, 404050 to 2 px, 484858 to 5, 505060 to 7,
                                                   585868 to 10, body 606070 from 11 px; dim the same without the 303040 line;
  the lens' top rows: lit 707080 at y 112, 686878 to y 116.5, 606078 to y 118.5; dim 585868 to y 114.5.

py -3.12 hs2.py
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import w2               # noqa: E402
import common as c      # noqa: E402
import auto             # noqa: E402
import headselect as hs  # noqa: E402
from auto import poly   # noqa: E402

WALL = 6.5
BANDS = {
    "lit": {"A": [(4.0, [88, 88, 104]), (3.0, [80, 80, 96]), (2.0, [72, 72, 88]), (1.0, [64, 64, 80])],
            "B": [(11.0, [88, 88, 106]), (8.0, [80, 80, 96]), (6.0, [72, 72, 88]), (2.5, [64, 64, 84]), (1.0, [48, 48, 64])],
            "top": [(116.6, [104, 104, 120]), (112.6, [112, 112, 128])], "top_edge": 118.6, "top_edge_col": [96, 96, 120]},
    "dim": {"A": [(13.5, [88, 88, 104]), (11.5, [80, 80, 96]), (9.5, [72, 72, 88]), (7.0, [64, 64, 80]), (4.0, [56, 56, 66]),
                  (2.0, [48, 48, 64]), (1.0, [40, 48, 56])],
            "B": [(11.0, [88, 88, 106]), (8.0, [80, 80, 96]), (6.0, [72, 72, 88])],
            "top": [(114.6, [88, 88, 104])], "top_edge": None, "top_edge_col": None},
}
BODY = [96, 96, 112]


def lens_layers(cell, st):
    x0, y0, x1, y1 = cell
    ox, oy = x0 - 432, y0 - 105
    A, B = hs.LEFT_ARCS
    Ac, Bc = (A[0] + ox, A[1] + oy), (B[0] + ox, B[1] + oy)
    cut = c.rr(x0 - 5, y0 + 4.5, x1 + 5, y0 + 139.0, 0.0)
    full = {"t": "inter", "of": [c.circ(*Ac, A[2]), c.circ(*Bc, B[2]), cut]}
    lens = {"t": "inter", "of": [c.circ(*Ac, A[2] - WALL), c.circ(*Bc, B[2]), cut]}
    b = BANDS[st]
    out = [
        (full, [96, 96, 136], {}),                                                         # the tray's lilac wall
        ({"t": "sub", "a": full, "b": {"t": "sub", "a": full, "b": c.circ(*Ac, A[2] - 2.0)}}, [88, 88, 128], {"soft": 0.6}),
        ({"t": "sub", "a": full, "b": {"t": "sub", "a": full, "b": c.circ(*Ac, A[2] - 3.5)}}, [80, 80, 120], {"soft": 0.6}),
        (lens, BODY, {}),
    ]
    for t, col in b["top"]:
        out.append(({"t": "inter", "of": [lens, {"t": "half", "p": [0.0, t + oy], "n": [0.0, 1.0]}]}, col, {"soft": 0.6}))
    for t, col in b["B"]:
        out.append(({"t": "sub", "a": lens, "b": c.circ(*Bc, B[2] - t)}, col, {"soft": 1.2, "clip": lens}))
    for t, col in b["A"]:
        out.append(({"t": "sub", "a": lens, "b": c.circ(*Ac, A[2] - WALL - t)}, col, {"soft": 1.0, "clip": lens}))
    return out


def item(cell, st):
    x0, y0, x1, y1 = cell
    dx = x0 - 432
    arrow = poly((458.0 + dx, 173.5 + y0 - 105), (486.5 + dx, 159.0 + y0 - 105), (485.0 + dx, 163.5 + y0 - 105),
                 (484.0 + dx, 173.5 + y0 - 105), (485.0 + dx, 183.5 + y0 - 105), (487.0 + dx, 189.0 + y0 - 105))
    bar = c.rr(x0 + 8, y0 + 139.0, x1 + 2, y0 + 147.5, 0.0)
    A = hs.LEFT_ARCS[0]
    # left of arc A the tray's surface runs from dark at the top (505078, 484870 at row 125) to lit lilac at the bottom (686898
    # from row 175 down): its own vertical gradient, measured; the rest of the cell is flat
    left = {"t": "inter", "of": [{"t": "sub", "a": c.rr(x0 - 1, y0 - 1, x1 + 1, y1 + 1, 0.0),
                                  "b": c.circ(A[0] + x0 - 432, A[1] + y0 - 105, A[2])},
                                 {"t": "half", "p": [x0 + 40.0, 0.0], "n": [1.0, 0.0]}]}
    parts = [(c.rr(x0 - 1, y0 - 1, x1 + 1, y1 + 1, 0.0), "flat"), (left, {"linear": {"n": 5, "dir": (0.0, 1.0)}}),
             (bar, {"linear": {"n": 4, "dir": (0.0, 1.0)}})]
    parts += [(S, col, kw) for S, col, kw in lens_layers(cell, st)]
    parts.append((arrow, "flat"))
    return auto.measured_item(hs.HS, cell, parts, name=f"head-select-left {st}")


if __name__ == "__main__":
    hs.arcs()
    states = [(st, cell, item(cell, st)) for st, cell in hs.LEFT_CELLS.items()]
    p = auto.deliver("head-select-left", hs.HS, states, z=3, how="")
    print(p)
