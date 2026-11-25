"""Buy Land lot balls: the navy digits 1-9, authored from their parts (read 2026-11-24 from the coverage runs of
buy_land_buttons.pcx, lit cells; bub.runs prints them). Coordinates are those of the LIT cell of each digit; another
instance (dim cell, the blue balls of the backdrop) is the same digit moved by the difference of the measured body centres
(plus a fitted offset of under a pixel).

What each digit IS (a bold geometric sans: strokes about 5 px, flat cuts):
  1  a vertical stem with a flag: the flag is a parallelogram leaning down-left from the stem's top, its left end cut
     vertically; flat foot, no base serif.
  2  an open bowl (ellipse arc) from a flat-cut left end over the top to the right, running on as a diagonal that thins
     toward the lower left, standing on a flat bottom bar.
  3  flat top bar; a diagonal down-left from its right end; a short flat middle stroke that turns into an open lower bowl
     ending in a flat-cut left terminal.
  4  a diagonal from the top of the stem down-left to the crossbar; a vertical stem; the crossbar reaching past the stem.
  5  flat top bar; a short vertical stem at the left; a flat middle stroke turning into an open lower bowl ending in a
     flat-cut left terminal (as in the 3).
  6  a closed lower bowl (an elliptic ring) with a curved tail rising to the upper right, its end cut square to the stroke.
  7  flat top bar; a straight diagonal down-left from its right end with a round foot.
  8  two stacked elliptic rings, the lower one larger, overlapping at the waist.
  9  the 6 turned over: a closed upper bowl with a curved tail falling to the lower left.
"""
import numpy as np


def rr(x0, y0, x1, y1, r=0.0):
    return {"t": "rrect", "c": [(x0 + x1) / 2, (y0 + y1) / 2], "half": [(x1 - x0) / 2, (y1 - y0) / 2], "r": r, "rot": 0.0}


def poly(*pts):
    return {"t": "poly", "rings": [[list(p) for p in pts]]}


def U(*s):
    return {"t": "union", "of": list(s)}


class Arc(dict):
    """A stroke along an ellipse (mid-line) from angle a0 to a1 (degrees, 0 = right, 90 = down), flat (butt) ends; `lead` /
    `tail` add a straight run before / after it. Stored as numbers and expanded to a polyline stroke when rendered."""


def arc(cx, cy, rx, ry, a0, a1, w, lead=None, tail=None):
    return {"t": "rm.arc", "c": [cx, cy], "r": [rx, ry], "a": [a0, a1], "w": w, "lead": lead, "tail": tail}


def _arc_pts(S):
    (cx, cy), (rx, ry), (a0, a1) = S["c"], S["r"], S["a"]
    n = max(8, int(abs(a1 - a0) / 6))
    t = np.deg2rad(np.linspace(a0, a1, n + 1))
    pts = [[float(cx + rx * np.cos(a)), float(cy + ry * np.sin(a))] for a in t]
    if S.get("lead"):
        pts = [list(S["lead"])] + pts
    if S.get("tail"):
        pts = pts + [list(S["tail"])]
    return pts


def band(pts, w):
    """The outline of a stroke of width w along a polyline, as one polygon (offsets along the vertex normals): flat ends square
    to the line, which a curved stroke needs (sdf's butt cap cuts a whole half plane, which eats a curve that turns back)."""
    p = np.asarray(pts, float)
    t = np.zeros_like(p)
    t[1:-1] = p[2:] - p[:-2]
    t[0], t[-1] = p[1] - p[0], p[-1] - p[-2]
    t /= np.maximum(np.hypot(t[:, 0], t[:, 1]), 1e-9)[:, None]
    nrm = np.c_[-t[:, 1], t[:, 0]] * (w / 2)
    left, right = (p + nrm).tolist(), (p - nrm).tolist()
    return {"t": "poly", "rings": [left + right[::-1]]}


def _arc_sd(S, x, y):
    import sdf
    return sdf.sd(band(_arc_pts(S), S["w"]), x, y)


def ring(cx, cy, rx, ry, w):
    """A closed elliptic ring (mid-line radii rx, ry, line width w)."""
    return {"t": "outline", "of": {"t": "ellipse", "c": [cx, cy], "r": [rx, ry], "rot": 0.0}, "w": w}


def register():
    import sdf
    sdf.EXTRA["rm.arc"] = _arc_sd


register()


def authored():
    """digit -> list of (what the part is, shape) in lit-cell coordinates."""
    return {
        1: [("stem with the flag, one outline",
             poly((27.3, 17.7), (31.7, 17.7), (31.6, 47.0), (25.8, 47.0), (26.3, 25.0), (26.3, 23.3), (20.5, 25.9),
                  (20.3, 21.6)))],
        2: [("open bowl from the flat-cut left end over the top", arc(87.35, 26.5, 6.45, 6.9, 160, 382, 5.0)),
            ("diagonal from the bowl down to the lower left, thinning",
             {"t": "qstroke", "p": [[93.4, 28.6], [91.6, 35.4], [81.3, 43.6]], "w": [5.0, 4.3]}),
            ("flat bottom bar", rr(77.6, 42.3, 96.0, 46.9, 0.8))],
        3: [("flat top bar", rr(138.3, 18.7, 154.8, 23.5, 0.5)),
            ("diagonal from the bar's right end", {"t": "stroke", "pts": [[152.6, 21.4], [144.0, 31.0]], "w": 4.4, "cap": "butt"}),
            ("middle stroke turning into the open lower bowl, flat-cut left terminal",
             arc(146.55, 39.0, 6.9, 7.0, 270, 544, 4.9, lead=[141.0, 32.0]))],
        4: [("diagonal from the top down to the crossbar", poly((209.0, 17.6), (214.0, 17.6), (201.9, 36.6), (196.8, 36.6))),
            ("vertical stem", rr(209.1, 17.6, 214.2, 46.9)),
            ("crossbar", rr(196.3, 36.4, 217.5, 40.8, 0.8))],
        5: [("flat top bar", rr(260.2, 17.9, 276.0, 22.5, 0.4)),
            ("short stem at the left", rr(260.2, 17.9, 265.2, 32.6, 0.6)),
            ("middle stroke turning into the open lower bowl, flat-cut left terminal",
             arc(267.55, 37.7, 6.75, 7.4, 270, 544, 5.0, lead=[262.5, 30.3]))],
        6: [("closed lower bowl", ring(327.55, 38.7, 7.3, 7.6, 4.9)),
            ("tail rising to the upper right, end cut square",
             {"t": "qstroke", "p": [[330.4, 18.2], [326.3, 22.6], [321.2, 31.6]], "w": [4.6, 5.0]})],
        7: [("flat top bar", rr(379.6, 17.9, 398.0, 22.5, 0.4)),
            ("straight diagonal with a round foot", {"t": "stroke", "pts": [[396.3, 20.0], [383.9, 44.6]], "w": 5.0, "cap": "round"})],
        8: [("upper ring", ring(447.75, 25.2, 5.7, 5.7, 4.7)),
            ("lower ring", ring(447.7, 38.25, 7.1, 6.95, 5.1))],
        9: [("closed upper bowl", ring(507.85, 28.3, 7.25, 8.05, 5.0)),
            ("tail falling to the lower left, end cut square",
             {"t": "qstroke", "p": [[514.6, 32.0], [512.6, 41.4], [505.0, 47.9]], "w": [5.0, 4.6]})],
    }


def shape(n, dx=0.0, dy=0.0):
    parts = authored()[n]
    s = U(*[p for _, p in parts])
    return {"t": "move", "of": s, "d": [dx, dy]} if (dx or dy) else s
