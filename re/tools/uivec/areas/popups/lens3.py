"""P41 popups round 3: trans-lens (redo, two versions). TransPopups.pcx cell 30, box (0, 150, 40, 174).

What it is (read at 20x, every pixel dumped as hex): a lilac lens-shaped pad, the ellipse c (21, 162) r (21, 12.1) (edge
coverage per row matches within 0.4 px; the shape is mirror-symmetric about row 162), its edge pixels blended into the sheet's
black (a dark fringe all round, darkest at the bottom), lit from the upper left: d8d9ea / cccde4 along the upper-left rim,
body 9597c6, a dark rim along the bottom (555771 .. 272732, 4 rows). On it two raised tan triangle buttons:
  up:   apex (20.4, 152.0), base y 160.0, x 9.0-31.8; light along its left edge (f5dda4 then eed697, 2 px each), face e8ca7d,
        bottom rows in shade (row 157 d4c27b, 158 c5ac6a, 159 9e8c50, each starting further left); a dark groove hugging its
        left edge and apex (5e607e next to it, 666887, 7b7ca3) and a milder one on its right (8c8f9b / 8183ab);
  a lip between them: row 160 ababd2 (b3b5d7 on the right), row 161 9597c6, row 162 a dark shadow row 6c6d90 with tapered ends;
  down: base y 163.0, x 8.9-31.4, apex (20.1, 170.6); top two rows light (eed697, f5dda4 on the left), face e8ca7d, a 2 px
        9e8c50 band along its right edge with c5ac6a inside it, c5ac6a along its left edge from row 166, the apex dark;
        a lit lip right of it (bebfdc 1 px, b3b5d7 2 px) and a faint light line left of it (9d9db5);
  at the right end, cut by the box: a cream disc (c (41.2, 161.2) r 6.9, fefaea, its lower rows shaded eeeada / e4decb /
        dad6c9).
Round 2 against the art at 16x, the five largest differences: (1) no dark fringe or bottom rim: the art's pad is outlined and
its bottom 4 rows go to near black, round 2 faded to mid grey; (2) the lilac lit by one smooth bevel, where the art has a
sharp light crescent on the upper left and a lighter top right band; (3) the triangles banded in horizontal stripes, where the
art's light runs along the left edge and only the bottom rows are in shade; (4) the down triangle's right edge too light and
its lit lip too wide; (5) the grooves round the up triangle too faint and not wrapping over its apex.

A, faithful: every patch above is its own shape in the art's colour; the lilac levels are nested bands of the ellipse fitted
on the art (bands3.py). B, clean: the same parts drawn as clean lit surfaces with gradients, symmetric triangles and stronger
light and shadow.

py -3.12 lens3.py [a|b]
"""
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import w3               # noqa: E402
import common as c      # noqa: E402
import auto             # noqa: E402
import bands3           # noqa: E402
from auto import ell, mv, poly  # noqa: E402

w3.apply()
TP = "TransPopups.pcx"
BOX = (0, 150, 40, 174)
E = ell(21.0, 162.0, 21.0, 12.1)
U = poly((20.3, 151.9), (32.0, 160.0), (8.2, 160.0))     # edges from the per-row coverage of the AA pixels
D = poly((8.1, 163.0), (32.2, 163.0), (20.0, 170.5))
OVAL = ell(41.5, 161.8, 7.3, 6.8)   # left edge per row: x 34.2 at rows 160-163, 37.5 at 156, 36.2 at 166
BLACK = [10, 10, 13]


def hx(h):
    return [int(h[i:i + 2], 16) for i in (0, 2, 4)]


def H(y, below=True):
    """The half plane below (or above) the line y."""
    return {"t": "half", "p": [0.0, y], "n": [0.0, -1.0 if below else 1.0]}


def V(x, right=True):
    return {"t": "half", "p": [x, 0.0], "n": [-1.0 if right else 1.0, 0.0]}


def I(*S):
    return {"t": "inter", "of": list(S)}


def Sb(a, b):
    return {"t": "sub", "a": a, "b": b}


def edge_seg(P0, P1, t0, t1, off, w):
    """A round-ended segment along the edge P0 -> P1 (from t0 to t1 of its length), offset `off` px along the edge's left-hand
    normal (outside the triangle for its left edge read apex -> base corner, inside out for the right edge read base -> apex)."""
    dx, dy = P1[0] - P0[0], P1[1] - P0[1]
    n = (dx * dx + dy * dy) ** 0.5
    nx, ny = dy / n, -dx / n
    a = [P0[0] + dx * t0 + nx * off, P0[1] + dy * t0 + ny * off]
    b = [P0[0] + dx * t1 + nx * off, P0[1] + dy * t1 + ny * off]
    return {"t": "seg", "a": a, "b": b, "w": w}


def L(S, col, **kw):
    d = {"shape": S, "fill": hx(col) if isinstance(col, str) else list(col)}
    d.update(kw)
    return d


LEVELS = ["272732", "373949", "4a4d65", "5e607e", "6c6d90", "7b7ca3", "8889b3", "9193c0", "9597c6",
          "a3a4ce", "b3b5d7", "bebfdc", "cccde4", "d8d9ea"]
BODY = 8


def fit_mask():
    x0, y0, x1, y1 = BOX
    jj, ii = np.mgrid[y0:y1, x0:x1]
    x, y = ii + 0.5, jj + 0.5
    rgb, m, _ = c.sheet(TP)
    a = rgb[y0:y1, x0:x1]
    yellow = (a[..., 0] - a[..., 2] > 25) & m[y0:y1, x0:x1]
    near = np.zeros_like(yellow)
    for dy in range(-2, 3):
        for dx in range(-2, 3):
            near |= np.roll(np.roll(yellow, dy, 0), dx, 1)
    inside = (c.sdf.sd(E, x, y) < -1.0) & m[y0:y1, x0:x1] & ~near
    inside &= ~((y > 159.5) & (y < 163.5) & (x > 6) & (x < 34))
    inside &= ~((x > 33.0) & (y > 153.0) & (y < 169.0))
    inside &= ~((x < 1.0) & (y > 165.5) & (y < 166.5))          # the magenta marker pixel at (0, 166)
    return inside


def version_a():
    fitted = bands3.fit_levels(TP, BOX, E, [hx(h) for h in LEVELS], BODY, fit_mask(), tag="lens A")
    lay = [L({"t": "grow", "of": E, "d": 0.5}, BLACK), L(E, LEVELS[0])]
    lay += bands3.band_layers(E, fitted, soft=0.45)
    # the up button's grooves: left and over the apex, darkest next to it; a milder one on its right
    # read per row: 3 px wide (rows 153-157), darkest next to the triangle at rows 155-156, lighter at both ends (row 153,
    # rows 158-159); the right one 2 px (rows 154-158)
    apex, bl, br = (20.3, 151.9), (8.2, 160.0), (32.0, 160.0)
    for t0, t1, w, col in ((0.04, 0.97, 3.0, "7b7ca3"), (0.1, 0.86, 2.3, "666887"), (0.3, 0.72, 1.4, "5e607e")):
        lay.append(L(I(Sb(edge_seg(bl, apex, 1.0 - t1, 1.0 - t0, w / 2, w), U), E), col, soft=0.3))
    for t0, t1, w, col in ((0.2, 0.9, 2.2, "8889b3"), (0.3, 0.75, 1.2, "8183ab")):
        lay.append(L(I(Sb(edge_seg(apex, br, t0, t1, w / 2, w), U), E), col, soft=0.3))
    lay.append(L(I(Sb(mv(U, 0.0, -1.0), U), V(16.6), V(22.0, right=False)), "8c8f9b", soft=0.3))   # the dark spot over the apex
    # the lip between the buttons: light row, body row, dark shadow row with tapered ends
    lay.append(L(c.rr(10.6, 160.0, 33.0, 161.0, 0.0), "ababd2"))
    lay.append(L(c.rr(27.6, 160.0, 33.0, 161.0, 0.0), "b3b5d7"))
    lay.append(L(c.rr(10.6, 161.0, 33.0, 162.0, 0.0), "9597c6"))
    lay.append(L(c.rr(28.0, 161.0, 33.0, 162.0, 0.0), "a3a4ce"))
    lay.append(L(c.rr(7.2, 162.0, 30.0, 163.0, 0.5), "8183ab"))
    lay.append(L(c.rr(9.6, 162.0, 28.4, 163.0, 0.5), "727397"))
    lay.append(L(c.rr(10.6, 162.0, 27.2, 163.0, 0.0), "6c6d90"))
    # the up button: face, light along the left edge, shaded bottom rows that start further left row by row
    lay.append(L(U, "e8ca7d"))
    lay.append(L(I(U, H(157.0), V(16.6)), "d4c27b"))
    lay.append(L(I(U, H(158.0), V(13.6)), "c5ac6a"))
    lay.append(L(I(U, H(159.0), V(12.6)), "9e8c50"))
    lay.append(L(I(U, H(159.0), V(10.6), V(12.6, right=False)), "c5ac6a"))
    lay.append(L(I(Sb(U, mv(U, 4.2, 0.0)), H(158.0, below=False)), "eed697", clip=U))
    lay.append(L(I(Sb(U, mv(U, 2.2, 0.0)), H(158.0, below=False)), "f5dda4", clip=U))
    lay.append(L(I(U, H(158.0), H(159.0, below=False), V(12.6, right=False)), "eed697"))
    # the down button: its right-edge shade, the left-edge shade, face, light top rows
    lay.append(L(D, "9e8c50"))
    lay.append(L(I(D, mv(D, -2.0, 0.0)), "c5ac6a"))
    lay.append(L(I(D, mv(D, -4.0, 0.0), mv(D, 1.6, 0.0)), "d4c27b"))
    lay.append(L(I(D, mv(D, -5.0, 0.0), mv(D, 3.4, 0.0), H(169.0, below=False)), "e8ca7d"))
    lay.append(L(I(D, H(165.0, below=False), mv(D, -2.4, 0.0)), "eed697"))
    lay.append(L(I(D, H(165.0, below=False), mv(D, -4.6, 0.0), H(164.0)), "e8ca7d"))
    lay.append(L(I(D, H(164.0, below=False), V(10.5), V(14.0, right=False)), "f5dda4"))
    lay.append(L(I(D, H(165.0, below=False), mv(D, -1.0, 0.0), V(27.6)), "d4c27b"))
    lay.append(L(I(D, H(169.0)), "9e8c50"))
    # the lit lip right of the down button and the faint light line left of it
    lay.append(L(I(Sb(mv(D, 2.8, 0.3), D), V(20.6), H(164.6), E), "b3b5d7", soft=0.3))
    lay.append(L(I(Sb(mv(D, 1.0, 0.0), D), V(20.6), H(165.0), E), "bebfdc", soft=0.3))
    lay.append(L(I(Sb(mv(D, -1.0, 0.0), D), V(19.6, right=False), H(164.6), E), "9d9db5", soft=0.3))
    # the cream disc at the right end, its lower rows shaded
    lay.append(L(OVAL, "dad6c9"))
    lay.append(L(I(OVAL, mv(OVAL, 0.0, -0.9)), "e4decb"))
    lay.append(L(I(OVAL, mv(OVAL, 0.0, -1.8)), "eeeada"))
    lay.append(L(I(OVAL, mv(OVAL, 0.4, -2.8)), "fefaea"))
    return c.item(BOX, lay, "trans-lens A"), fitted


HOW_A = ("A, faithful: the lilac pad = ellipse c (21, 162) r (21, 12.1) over the sheet's black (its dark fringe), shaded as 13 "
         "nested bands of the ellipse in the art's colours (8 rings toward the dark bottom rim, 5 crescents toward the light "
         "upper-left rim, each fitted on the art's pixels); the up button with its light left edge (f5dda4 / eed697), "
         "shaded bottom rows (d4c27b, c5ac6a, 9e8c50) and the dark groove round its left edge and apex; the lip (light row, "
         "body row, dark shadow row); the down button with its 9e8c50 / c5ac6a right-edge shade, light top rows and lit lip; "
         "the cream disc at the right end with its shaded lower rows")
CHANGED_A = ("redo, rejected twice with no note; against round 2: the dark fringe and the near-black bottom rim are drawn; the "
             "lilac's light is the art's bands (light crescent upper left, lighter top right), not one smooth bevel; the "
             "triangles carry their light along the left edge and the shade only on their bottom rows; the down button's "
             "right edge is dark as in the art and its lit lip 3 px; the up button's groove wraps over its apex")


def version_b():
    """B, clean: the same parts as lit surfaces with gradients, symmetric triangles mirrored about x 20.3."""
    ax = 20.3
    Ub = poly((ax, 152.0), (ax + 11.3, 160.0), (ax - 11.3, 160.0))
    Db = poly((ax - 11.3, 163.0), (ax + 11.3, 163.0), (ax, 170.6))
    lay = [L({"t": "outline", "of": E, "w": 1.2}, "1a1c28"),
           L(E, "272732"),
           # the body: a radial light from the upper left over the pad, the bottom rim fading to near black
           {"shape": {"t": "grow", "of": E, "d": -0.6},
            "fill": {"radial": {"c": [12.0, 154.0], "r": 30.0,
                                "stops": [[0.0, hx("d8d9ea")], [0.22, hx("c4c5e0")], [0.45, hx("a3a4ce")],
                                          [0.7, hx("9193c0")], [1.0, hx("7b7ca3")]]}}},
           # the shadow rim: a crescent along the lower edges (the pad minus itself moved up-left), widest at the bottom,
           # climbing the lower right as in the art, darkening downward to near black
           {"shape": I(E, Sb(E, mv(E, -1.6, -3.4))),
            "fill": {"linear": {"a": [21.0, 160.0], "b": [21.0, 174.0],
                                "stops": [[0.0, hx("7b7ca3")], [0.4, hx("5e607e")], [0.7, hx("3c4054")], [1.0, hx("141720")]]}},
            "soft": 0.9, "clip": E},
           # a thin specular crescent on the upper-left rim
           L(I(bands3.crescent(E, -0.6, 1.4, 1.6), H(160.0, below=False)), "e6e7f4", soft=0.6, clip=E),
           ]
    # the up button's slot: a soft dark groove round its left edge and apex, a light one under its right edge
    apex, bl, br = (ax, 152.0), (ax - 11.3, 160.0), (ax + 11.3, 160.0)
    lay.append(L(I(Sb(edge_seg(bl, apex, 0.0, 1.06, 1.4, 2.8), Ub), E), "727397", soft=0.7))
    lay.append(L(I(Sb(edge_seg(bl, apex, 0.15, 0.9, 0.7, 1.4), Ub), E), "5a5c7c", soft=0.5))
    lay.append(L(I(Sb(edge_seg(apex, br, 0.1, 0.92, 0.9, 1.8), Ub), E), "8183ab", soft=0.6))
    # the lip: a lit ledge with a shadow line under it
    lay.append(L(c.rr(ax - 10.5, 160.0, ax + 12.5, 161.4, 0.6), "b3b5d7"))
    lay.append(L(c.rr(ax - 9.5, 162.0, ax + 8.0, 163.0, 0.5), "65668a", soft=0.4))
    # the up button: a raised wedge, light falling from its apex and left edge to its base
    lay.append({"shape": Ub, "fill": {"linear": {"a": [ax - 3.0, 152.5], "b": [ax + 1.0, 160.0],
                                                 "stops": [[0.0, hx("f8e4b0")], [0.35, hx("eed697")], [0.62, hx("e2c478")],
                                                           [0.82, hx("c5ac6a")], [1.0, hx("9a8650")]]}}})
    lay.append(L(I(Sb(Ub, mv(Ub, 1.6, 0.0)), H(158.6, below=False)), "fdf0c8", clip=Ub, soft=0.4))
    # the down button: light top, its right side and apex in shade
    lay.append({"shape": Db, "fill": {"linear": {"a": [ax - 2.0, 163.0], "b": [ax + 2.5, 170.0],
                                                 "stops": [[0.0, hx("f6e0a8")], [0.3, hx("eccf88")], [0.6, hx("d2b66e")],
                                                           [1.0, hx("9a8650")]]}}})
    lay.append(L(I(Sb(Db, mv(Db, -2.2, 0.0)), H(164.2)), "9e8c50", clip=Db, soft=0.5))
    lay.append(L(I(Db, H(164.0, below=False)), "f8e8bc", soft=0.4))
    lay.append(L(I(Sb(mv(Db, 1.8, 0.2), Db), V(ax + 0.6), H(164.6)), "bebfdc", soft=0.5, clip=E))
    # the cream disc, lit on top, its lower rim shaded
    lay.append({"shape": OVAL, "fill": {"linear": {"a": [41.5, 155.0], "b": [41.5, 168.6],
                                                   "stops": [[0.0, hx("fefaea")], [0.6, hx("fbf6e4")], [1.0, hx("d8d4c6")]]}}})
    return c.item(BOX, lay, "trans-lens B")


HOW_B = ("B, clean: the same ellipse pad with a crisp navy outline, lit by a radial light from the upper left (d8d9ea to "
         "7b7ca3) with a thin specular crescent on the upper-left rim and a bottom rim fading to near black; the two "
         "triangles mirrored about x 20.3, each a raised wedge with a gradient (up: light apex and left edge down to a dark "
         "base; down: light top down to a dark right side and apex); a soft groove round the up button, a lit ledge with a "
         "shadow line between them, a lit lip right of the down button; the cream disc lit on top")
CHANGED_B = ("redo, rejected twice with no note; against round 2: stronger baked light (outline, specular crescent, near-black "
             "bottom rim, deeper groove), both triangles shaded as raised wedges with continuous gradients instead of "
             "stripes, the down button's right side in shade, symmetric triangles")


def deliver(which):
    w3.apply()
    if which == "a":
        it, _ = version_a()
        how, ch, eid = HOW_A, CHANGED_A, "trans-lens--a"
    else:
        it = version_b()
        how, ch, eid = HOW_B, CHANGED_B, "trans-lens--b"
    p = auto.deliver(eid, TP, [("idle", BOX, it)], z=16, how="")
    w3.record(eid, "done", how, ch, "the cream disc at the right end is cut by the cell (it may belong to the next cell); "
              "the magenta pixel at (0, 166) is a sheet marker and is not drawn")
    return p


if __name__ == "__main__":
    todo = sys.argv[1:] or ["a", "b"]
    for k, w in enumerate(todo):
        print(f"[{k + 1}/{len(todo)}] trans-lens {w} {deliver(w)}", flush=True)
