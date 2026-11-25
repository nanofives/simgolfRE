"""Scroll arrows and tracks (Member Roster and Routing map), read 2026-11-24 from pixel dumps of memberRoster_buttons.pcx,
memberRoster_scrollbar.pcx and route screens_bottom.pcx:
  an ARROW is a plain triangle (tip, flat base; no dent), white on the lilac tabs, cream on the tracks, with a dark edge on
  its left / lower side and a light one on its right (its paint by depth and edge direction, and a 1.5 px lip outside,
  solved over every triangle of its colour). Two stacked per tab: up tip (x0 + 9.5, y0 + 1.6), base y0 + 16.4, half 5.4,
  the second 18 px lower; down base y0 + 3.6, tip y0 + 18.6, half 5.2.
  a TRACK is a lilac strip with the two up arrows at its top, the two down arrows at its foot, and a sunk slot between them
  (a dark lilac floor with a light rim line on both sides): route x 754.5..765.5, y 38.5..139.5; roster x 3.5..14.5,
  y 38.5..409.5.
"""
import json
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import common as C      # noqa: E402
import lit              # noqa: E402
import parts as P       # noqa: E402

LILAC = [148, 150, 198]
STYLE_FILE = os.path.join(C.OUT, "read", "scroll_style.json")
RB, MB, SB = "route screens_bottom.pcx", "memberRoster_buttons.pcx", "memberRoster_scrollbar.pcx"


def tri(x0, y0, up, k=0):
    dy = 18 * k
    if up:
        cx, tip, base, h = x0 + 9.5, y0 + 1.6 + dy, y0 + 16.4 + dy, 5.4
    else:
        cx, tip, base, h = x0 + 9.5, y0 + 18.6 + dy, y0 + 3.6 + dy, 5.2
    return {"t": "poly", "rings": [[[cx, tip], [cx + h, base], [cx - h, base]]]}


# (sheet, x0, y0, up, colour): every arrow pair
PAIRS = [(MB, 166, 1, True, "white"), (MB, 185, 1, False, "white"), (RB, 713, 1, True, "white"), (RB, 732, 1, False, "white"),
         (RB, 751, 0, True, "cream"), (RB, 751, 140, False, "cream"), (SB, 0, 0, True, "cream"), (SB, 0, 410, False, "cream")]
SLOTS = [(RB, (754.5, 38.5, 765.5, 139.5)), (SB, (3.5, 38.5, 14.5, 409.5))]


def solve():
    st = {}
    for col in ("white", "cream"):
        lst = []
        for sh, x0, y0, up, c in PAIRS:
            if c != col:
                continue
            rgb, m = C.sheet(sh)
            for k in (0, 1):
                S = tri(x0, y0, up, k)
                ys = [p[1] for p in S["rings"][0]]
                box = (x0, max(int(min(ys)) - 3, 0), x0 + 18, min(int(max(ys)) + 4, rgb.shape[0]))
                lst.append((S, rgb, box, None))
        # the edge light only in the outer pixel (deeper in, a first try with it painted the triangle as a pyramid)
        s = lit.new_style(ow=0.0, L=2.0, out_w=1.5)
        lit.solve(lst, s, dir_in=0.3)
        st[col] = s
    lst = []
    for sh, b in SLOTS:
        rgb, m = C.sheet(sh)
        lst.append((P.rr(*b, 2.0), rgb, (int(b[0]) - 3, int(b[1]) - 3, int(b[2]) + 3, int(b[3]) + 3), None))
    s = lit.new_style(ow=0.0, L=3.0, out_w=1.5)
    lit.solve(lst, s, dir_in=1.0)
    st["slot"] = s
    json.dump(st, open(STYLE_FILE, "w"))
    return st


def arrows(x0, y0, up, col, st):
    return [{"name": "arrow", "box": [x0 - 1, y0 - 1 + 18 * k, x0 + 19, y0 + 21 + 18 * k],
             "layers": lit.layers(tri(x0, y0, up, k), st[col])} for k in (0, 1)]


def slot(b, st):
    return [{"name": "slot", "box": [b[0] - 3, b[1] - 3, b[2] + 3, b[3] + 3], "layers": lit.layers(P.rr(*b, 2.0), st["slot"])}]


if __name__ == "__main__":
    import deliver as D
    st = solve()

    def crop(sheet, x, y, w, h):
        rgb, m = C.sheet(sheet)
        a = rgb[y:y + h, x:x + w].copy()
        a[~m[y:y + h, x:x + w]] = [70, 110, 90]
        b = np.empty_like(a); b[:] = LILAC
        b[~m[y:y + h, x:x + w]] = [70, 110, 90]
        return a, b
    how = ("a lilac tab with two stacked plain triangles (tip and flat base read off the pixel dump), white with a dark edge on "
           "the left / lower side and a light one on the right: paint by depth and edge direction plus a 1.5 px lip, solved on all "
           "eight white triangles")
    for eid, pairs in (("roster-arrows", [("up", MB, 166, 1, True), ("down", MB, 185, 1, False)]),
                       ("route-scroll", [("up", RB, 713, 1, True), ("down", RB, 732, 1, False)])):
        states = []
        for label, sh, x0, y0, up in pairs:
            a, b = crop(sh, x0, y0, 18, 37)
            states.append((label, a, b, arrows(x0, y0, up, "white", st), (x0, y0)))
        D.deliver(eid, states, how, z=8)
    a, b = crop(RB, 751, 0, 18, 178)
    D.deliver("route-scroll-track", [("idle", a, b, arrows(751, 0, True, "cream", st) + arrows(751, 140, False, "cream", st)
                                      + slot(SLOTS[0][1], st), (751, 0))],
              "a lilac strip: two cream up arrows at the top, two down at the foot (the same triangles as the tabs', cream paint "
              "solved on the eight track arrows) and a sunk slot between them (x 754.5..765.5, y 38.5..139.5, r 2: dark floor, "
              "light rim, its paint solved on both tracks' slots)", z=4)
    a, b = crop(SB, 0, 0, 18, 447)
    D.deliver("roster-scrollbar", [("idle", a, b, arrows(0, 0, True, "cream", st) + arrows(0, 410, False, "cream", st)
                                    + slot(SLOTS[1][1], st), (0, 0))],
              "the Routing map's track, longer: two cream up arrows, two down, the sunk slot between (x 3.5..14.5, y 38.5..409.5)",
              unsure="drawn at 3x (the strip is 447 px tall)", z=3)
