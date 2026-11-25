"""The bulldozer bubble (terrain, elevation and amenities palettes: bulldozer-button, elevation-bulldozer, amenities-bulldozer).

Read (cmap.py on bulldozer-button idle and selected, 2026-11-24): a side view of a bulldozer facing LEFT, about 17 x 12 px:
  blade      a thin curved plate at the front (x 7, y 561..570), its foot bending forward
  push arm   one navy pixel row from the blade into the body (y 563, x 8..10)
  cab        a yellow box with a 1 px navy outline (x 11..16, y 559..564), its back wall 2 px thick (x 16..17, y 560..561)
  engine     a low yellow block behind the cab (x 16..22, y 562..564) with a short navy top edge (x 18..19) and the
             exhaust stack rising from its back (navy, x 20..21, y 558..561)
  frame      a 2 px navy bar under cab and engine (x 7.5..22.5, y 564..566)
  track      a navy stadium under the frame (x 10..23.5, y 566..570) whose top run is lighter grey, with two staggered
             rows of light tread dots (y 567: x 14..15, 18; y 568: x 12..13, 16..17, 20..21)
All in local coordinates of the reference cut (bulldozer-button idle at (0, 550)): y_local = y - 550.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from geom import rect, line, seg, rrect_c, oval, poly  # noqa: E402


def glyph(dx=0.0, dy=0.0):
    """The bulldozer's parts, local to bulldozer-button idle; (dx, dy) moves it."""
    Y = -550.0 + dy
    X = dx

    def r(x0, y0, x1, y1, rr=0.0):
        return rect(x0 + X, y0 + Y, x1 + X, y1 + Y, rr)

    def p(x, y):
        return (x + X, y + Y)

    dots = []
    for (x0, x1, yy) in ((14.0, 16.0, 567.0), (18.0, 19.2, 567.0), (12.0, 14.0, 568.0), (16.0, 18.0, 568.0), (20.0, 22.0, 568.0)):
        dots.append(r(x0 + 0.1, yy + 0.1, x1 - 0.1, yy + 0.95, 0.4))
    return [
        ("track: navy stadium under the frame", r(10.0, 565.6, 23.6, 570.2, 2.2), "navy"),
        ("track top run: lighter grey band", r(11.0, 566.0, 22.4, 567.0), "trackgrey"),
        ("track tread: light dots in two staggered rows", {"t": "union", "of": dots}, "tread"),
        ("frame: 2 px navy bar", r(7.4, 564.0, 22.6, 566.0), "navy"),
        ("blade: curved plate, foot bending forward", line(p(7.5, 560.6), p(7.5, 567.0), p(6.9, 570.0), w=1.25,
                                                           cap="butt"), "navy"),
        ("push arm", seg(p(7.6, 563.5), p(11.2, 563.5), 1.0), "navy"),
        ("cab: navy box (back wall 2 px)", poly(p(11.0, 559.1), p(16.0, 559.1), p(16.0, 560.0), p(18.0, 560.0), p(18.0, 564.2), p(11.0, 564.2)), "navy"),
        ("engine: navy block behind the cab", r(16.0, 562.0, 23.0, 564.2), "navy"),
        ("exhaust stack", r(20.0, 558.0, 22.0, 562.0), "navy"),
        ("cab: yellow inside", r(12.0, 560.0, 16.0, 564.0), "yellow"),
        ("engine: yellow inside (cab floor to the stack)", poly(p(12.0, 562.0), p(18.0, 562.0), p(18.0, 563.0), p(20.0, 563.0),
                                                               p(20.0, 562.0), p(22.0, 562.0), p(22.0, 564.0), p(12.0, 564.0)),
         "yellow"),
    ]
