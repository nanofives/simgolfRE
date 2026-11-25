"""Member Roster (memberRoster.pcx), read 2026-11-24 from its opaque mask and pixel rows.

What it IS:
  SLAB: one lavender outline (the mask's edge read as its parts: a big shoulder curving from the left wall at y 49 to the top
    at x 86, the top at y 5, a step down at x 535.5 to y 43, small round corners, the right wall at x 795, a FOOT tab
    below the bottom right (x 709..795 to y 595, round lower corners), the bottom at y 566), no line of its own: a lit
    rim (pale toward the upper left, shaded toward the lower right): lit.py, solved.
  the WIRING line: a 2 px navy line from the grid's top along y 66.5, round the corner at x 10, down the left side and
    along y 560.5 to the foot (the line the header pills and tabs sit on).
  the arched TITLE PLATE (its own profile: middle 10.75, rise 7.9 to the ends).
  the NAME column: a 2 px navy box (x 20.5..142, y 84..524) ruled every 20 px; 22 cream cells with an inner shade.
  four cream COLUMNS sunk into the lavender (fills x 152..188, 192..228, 232..268, 272..370, y 85..524): a dark lip outside
    their left / top walls, a light one outside the right.
  the hole GRID: 2 px navy rules at x 380 + 21 k (k 0..18) and y 84 + 20 j (j 0..22): 18 x 22 cells of 19 x 18 px with an
    inner shade, the columns alternately tan (the first) and cream, and 18 lilac TABS above, alternately deeper and paler (rounded squares in a navy line, x 380 + 21 k .. 401 + 21 k, y 55..75.3).
  header PILLS (lilac, the Routing map's pill): name, the four columns.
"""
import json
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import common as C      # noqa: E402
import lit              # noqa: E402
import parts as P       # noqa: E402
import route_tops as R  # noqa: E402
import sdf              # noqa: E402

SHEET = "memberRoster.pcx"
NAVY = [36, 47, 84]
SLAB_RING = ([[4, 49], [8, 40], [12, 35], [16, 30], [20, 27], [24, 24], [28, 22], [32, 19], [36, 17], [40, 16], [44, 14],
              [48, 13], [52, 11], [56, 10], [60, 9], [64, 8], [70, 7], [78, 6], [86, 5], [530, 5], [532.5, 6], [534.5, 8.5],
              [535.5, 12], [535.5, 37], [536.5, 40.5], [538.5, 42.5], [541, 43], [789.5, 43], [792.5, 44.5], [794.5, 47],
              [795, 50], [795, 579], [793.5, 582.5], [790.5, 587], [787, 590.5], [783.5, 593], [779, 594.7], [774, 595],
              [728, 595], [724, 594.2], [720.5, 592.5], [717, 590], [714, 586.5], [711.5, 582.5], [710, 578], [709.5, 572],
              [708.5, 568], [706, 566], [9, 566], [6.5, 565], [4.8, 563.5], [4, 561]])
PLATE_G = {"box": [103.3, 522.3, 44.4], "rb": 2.0, "drop": 1.5, "lift": 0.0,
           "prof": [[0, 10.75], [25, 10.79], [50, 10.96], [75, 11.26], [100, 11.94], [125, 12.81], [150, 13.94], [170, 15.12],
                    [185, 16.52], [195, 17.71], [200, 18.62], [203.5, 19.6], [206.5, 20.4]]}
HEAD_PILLS = [[29, 60, 137, 72], [154, 60, 187, 72], [194, 60, 226, 72], [233, 60, 266, 72], [274, 60, 368, 72]]
COLS = [(152, 188), (192, 228), (232, 268), (272, 370)]
GX = [380 + 21 * k for k in range(19)]
GY = [84 + 20 * j for j in range(23)]
KINDS = {"slab": dict(ow=0.0, L=4.0, out_w=0.0), "cell": dict(ow=0.0, L=3.0, out_w=0.0), "cell_tan": dict(ow=0.0, L=3.0, out_w=0.0),
         "tab_tan": dict(ow=1.8, L=4.0, out_w=0.0),
         "col": dict(ow=0.0, L=6.0, out_w=5.0), "tab": dict(ow=1.8, L=4.0, out_w=0.0), "hpill": dict(ow=1.7, L=5.0, out_w=0.0)}
STYLE_FILE = os.path.join(C.OUT, "read", "roster_style.json")


def slab_shape():
    return {"t": "poly", "rings": [SLAB_RING], "smooth": 1}


def wire():
    pts = [[759.0, 66.5], [18.0, 66.5]] + P.corner(18.0, 74.5, 8.0, 270, 180, 6)[1:] + [[10.0, 552.5]] + \
          P.corner(18.0, 552.5, 8.0, 180, 90, 6)[1:] + [[731.0, 560.5]]
    return {"t": "stroke", "pts": pts, "w": 2.0, "cap": "butt"}


def components():
    """[(name, kind, shape)] bottom to top."""
    out = [("slab", "slab", slab_shape()), ("wiring line", ("flat", NAVY), wire())]
    for x0, x1 in COLS:
        out.append(("cream column", "col", P.rr(x0, 85.0, x1, 524.0)))
    # name column: the box line, the rules, the cells
    out.append(("name column", ("flat", NAVY), P.rr(20.0, 83.0, 143.0, 525.0)))
    for j in range(22):
        out.append(("name cell", "cell", P.rr(22.0, GY[j] + 1.0, 141.0, GY[j + 1] - 1.0)))
    out.append(("grid", ("flat", NAVY), P.rr(GX[0] - 1.0, GY[0] - 1.0, GX[-1] + 1.0, GY[-1] + 1.0)))
    for j in range(22):
        for k in range(18):
            # the columns alternate: tan from the first (x 381), cream; their tabs alternate deeper / paler lilac with them
            out.append(("grid cell", "cell_tan" if k % 2 == 0 else "cell",
                        P.rr(GX[k] + 1.0, GY[j] + 1.0, GX[k + 1] - 1.0, GY[j + 1] - 1.0)))
    for k in range(18):
        out.append(("tab", "tab_tan" if k % 2 == 0 else "tab", P.rr(GX[k], 55.0, GX[k + 1], 75.3, 4.0)))
    for f in HEAD_PILLS:
        out.append(("header pill", "hpill", R.pill(f)))
    out.append(("legend", ("legend", None), P.rr(12.0, 526.0, 742.0, 556.0)))
    out.append(("title plate", "plate", P.title_plate(PLATE_G)))
    return out


def bbox(S, m=8.0):
    if S["t"] == "rrect":
        c, h = S["c"], S["half"]
        return [c[0] - h[0] - m, c[1] - h[1] - m, c[0] + h[0] + m, c[1] + h[1] + m]
    return [0, 0, 800, 600]


def item(name, kind, S, styles):
    if isinstance(kind, tuple):
        return {"name": name, "box": bbox(S), "layers": [{"shape": S, "fill": kind[1]}]}
    if kind == "plate":
        st = json.load(open(os.path.join(C.OUT, "read", "caps_fit.json")))["style"]["cream"]
        return {"name": name, "box": [0, 0, 800, 600], "layers": P.plate(S, dict(st, line=list(NAVY)))}
    return {"name": name, "box": bbox(S), "layers": lit.layers(S, styles[kind])}


def scene(styles, legend=True):
    out = []
    for n, k, S in components():
        if isinstance(k, tuple) and k[0] == "legend":
            if legend:
                import roster_marks
                out += roster_marks.legend_scene()
            continue
        out.append(item(n, k, S, styles))
    return out


def solve():
    rgb, m = C.sheet(SHEET)
    jj, ii = np.mgrid[0:600, 0:800]
    x, y = ii + 0.5, jj + 0.5
    cover = ~m.copy()
    insts = {}
    for name, kind, S in components()[::-1]:
        d = sdf.sd(S, x, y)
        if not isinstance(kind, tuple) and kind in KINDS:
            box = (0, 0, 800, 600)
            keep = ~cover
            if kind in ("cell", "cell_tan", "tab", "tab_tan"):          # a small part: its own box only (faster, same pixels)
                c, h = S["c"], S["half"]
                box = (int(c[0] - h[0] - 3), int(c[1] - h[1] - 3), int(c[0] + h[0] + 4), int(c[1] + h[1] + 4))
                keep = keep[box[1]:box[3], box[0]:box[2]]
            insts.setdefault(kind, []).append((S, rgb, box, keep))
            reach = 2.0 + KINDS[kind]["out_w"] + KINDS[kind]["ow"] / 2
        else:
            reach = 1.5
        cover |= d < reach
    styles = {}
    for kind, lst in insts.items():
        st = lit.new_style(**KINDS[kind])
        lit.solve(lst, st, dir_in=0.5 if kind.startswith("cell") else 1.0)
        styles[kind] = st
        print(f"[solve] {kind}: {len(lst)} instances, line {st['line']}", flush=True)
    json.dump(styles, open(STYLE_FILE, "w"))
    return styles


def load_styles():
    return json.load(open(STYLE_FILE))


if __name__ == "__main__":
    solve()
