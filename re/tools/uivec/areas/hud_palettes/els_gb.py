"""GBUBBLES.pcx status icons (16 x 16 cells at y 324), as authored parts with measured flat colours (fel.py).

Read with cmap.py (colour letters per pixel) on each cell, 2026-11-24. Coordinates local to the cell: pixel (i, j) covers
[i, i+1) x [j, j+1), so a run of pixels 4..10 is the span [4, 11).
The round icons share one disc: a pixel circle of radius 7.5 round (7.5, 7.5), over its drop shadow, the same circle moved
one pixel right and down (dark grey); the eye, hand and clubs cells show the shadow without a disc (their disc colour is the
transparent one).
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from geom import rect, line, poly, circle, oval, outlined, sub, inter, U, mirror_x, moved  # noqa: E402

DISC = [("disc shadow: the disc moved one pixel right and down", circle(8.5, 8.5, 7.5), "shadow"),
        ("disc", circle(7.5, 7.5, 7.5), "disc")]
SHADOW_ONLY = [("disc shadow without its disc: a crescent at the lower right", sub(circle(8.5, 8.5, 7.5), circle(7.5, 7.5, 7.5)),
                "shadow")]


def star(cx, cy, ro, ri, rot=-90.0):
    import math
    pts = []
    for k in range(10):
        a = math.radians(rot + 36 * k)
        r = ro if k % 2 == 0 else ri
        pts.append((cx + r * math.cos(a), cy + r * math.sin(a)))
    return poly(*pts)


STAR = poly((4.5, -0.1), (5.5, 3.0), (9.0, 3.1), (6.6, 4.9), (7.0, 9.0), (4.4, 6.8), (2.0, 9.0), (2.4, 5.0), (0.0, 3.1), (3.5, 3.0))
SPECS = []


def S(id_, how, parts, **k):
    SPECS.append(dict(id=id_, how=how, parts=parts, **k))


S("gb-arrow-up-left", "mauve disc with its shadow; a black arrow pointing up-left: a right-angled triangular head in the corner "
  "(legs along the top and the left), a 3 px shaft leaving it diagonally and turning straight down",
  DISC + [("head: right-angled triangle, corner at the upper left", poly((4, 3), (11, 3), (4, 10)), "glyph"),
          ("shaft: diagonal out of the head", line((6.0, 6.0), (9.5, 9.5), w=3.0), "glyph"),
          ("shaft: straight down", rect(8, 9, 11, 13), "glyph")])
S("gb-arrow-up-right", "mauve disc with its shadow; a black arrow pointing up-right: a right-angled triangular head in the "
  "corner (legs along the top and the right), a 3 px shaft leaving it diagonally and turning straight down",
  DISC + [("head: right-angled triangle, corner at the upper right", poly((4, 3), (11, 3), (11, 10)), "glyph"),
          ("shaft: diagonal out of the head", line((9.0, 6.0), (5.5, 9.5), w=3.0), "glyph"),
          ("shaft: straight down", rect(4, 9, 7, 13), "glyph")])
S("gb-plus", "mauve disc with its shadow; a black plus of two 3 px bars with flat ends",
  DISC + [("vertical bar", rect(6, 3, 9, 12), "glyph"), ("horizontal bar", rect(3, 6, 12, 9), "glyph")])
S("gb-minus", "mauve disc with its shadow; a black minus: one 3 px bar with flat ends",
  DISC + [("bar", rect(3, 6, 12, 9), "glyph")])
S("gb-flag", "khaki disc with its shadow; a black flag: a 1 px pole and a pennant flying right from its top",
  DISC + [("pole", rect(6, 5, 7, 13), "glyph"),
          ("pennant", poly((7, 2), (11, 5), (11, 7), (9, 7), (9, 6), (6, 6), (7, 5)), "glyph")])
S("gb-speech-bubble", "khaki disc with its shadow; a black speech balloon: an oval with a short pointed tail below",
  DISC + [("balloon", oval(7.3, 6.5, 5.5, 3.5), "glyph"), ("tail", poly((6.4, 8.6), (9.6, 8.6), (7.4, 12.0)), "glyph")])
S("gb-trend-line", "khaki disc with its shadow; a thin black zigzag line rising to the right",
  DISC + [("zigzag line, 1 px", line((2.5, 10.5), (5.5, 7.5), (6.5, 8.5), (8.5, 6.5), (10.5, 8.5), (13.5, 5.5), w=1.0), "glyph")])
S("gb-dollar", "khaki disc with its shadow; a black dollar sign: a 2 px S and two thin bars through it, sticking out top and "
  "bottom", DISC + [("S: one 2 px curve", line((12.0, 3.9), (5.6, 3.9), (3.9, 5.4), (4.8, 6.9), (10.4, 7.4), (11.2, 8.8), (10.2, 11.0),
                                                (3.2, 11.0), w=2.0, smooth=1, cap="round"), "glyph"),
                      ("left bar", line((7.5, 2.0), (6.5, 13.0), w=1.0), "glyph"),
                      ("right bar", line((9.5, 2.0), (8.5, 13.0), w=1.0), "glyph")])
S("gb-info-i", "khaki disc with its shadow; a black i: a square dot over a 3 px stem",
  DISC + [("dot", rect(6, 3, 9, 5), "glyph"), ("stem", rect(6, 6, 9, 12), "glyph")])
S("gb-question-mark", "khaki disc with its shadow; a black question mark: a 2 px hook curling into a short stem, and a square dot",
  DISC + [("hook and stem", line((3.6, 6.2), (4.4, 4.2), (6.5, 3.6), (9.0, 3.8), (10.4, 5.2), (10.2, 6.8), (8.4, 8.0), (7.4, 9.8),
                                 w=2.2, smooth=1, cap="round"), "glyph"),
          ("dot", rect(6, 11, 9, 13), "glyph")])
S("gb-note-page", "khaki disc with its shadow; a dark grey notebook seen from the side: a black-framed body, a grey binding "
  "on top, the edges of its pages as light and grey stripes along the left, two short black lines of writing and a small "
  "grey tab below", DISC + [
      ("binding on top", rect(4, 2, 12, 4, 0.8), "grey"),
      ("body: black frame", rect(2, 4, 13, 11), "glyph"),
      ("body: dark grey inside the frame", rect(3, 4, 12, 10), "grey"),
      ("page edges: grey stripes", U(rect(3, 6, 6, 7), rect(3, 8, 6, 9)), "pagegrey"),
      ("page edges: light stripes", U(rect(3, 5, 7, 6), rect(3, 7, 6, 8), rect(3, 9, 6, 10)), "pagelight"),
      ("a line of writing", rect(7, 4, 10, 5), "glyph"), ("a second line of writing", rect(6, 9, 9, 10), "glyph"),
      ("small tab below", rect(6, 10, 9, 12), "grey")])
S("gb-eye", "an eye with no disc (its shadow crescent only): a thin almond outlined in black (1 px at top and bottom, 2 px at "
  "the corners) round a grey iris, a one-pixel black lash above and below its middle", SHADOW_ONLY + [
      ("almond: black outline", inter(circle(8.5, 15.7, 9.7), circle(8.5, 1.3, 9.7)), "glyph"),
      ("almond: grey inside", inter(circle(8.5, 14.5, 7.5), circle(8.5, 2.5, 7.5)), "grey"),
      ("lash above", rect(8, 5, 9, 6), "glyph"), ("lash below", rect(8, 11, 9, 12), "glyph")])
S("gb-hand-pointer", "no disc (its shadow crescent only); a dark grey arch like a fingertip on two legs, a black edge "
  "down its upper left and right sides, and a black dot out at the left", SHADOW_ONLY + [
      ("arch: dark grey top", poly((7, 5), (10, 5), (11, 6), (12, 7), (11, 8), (11, 9), (4, 9), (4, 8), (5, 7), (6, 6)), "grey"),
      ("arch: left leg", rect(5, 9, 7, 12), "grey"), ("arch: right leg", rect(10, 9, 12, 12), "grey"),
      ("black edge, upper left", line((6.5, 5.5), (4.5, 7.5), w=1.0, cap="round"), "glyph"),
      ("black edge, right", rect(12, 7, 13, 9), "glyph"),
      ("black dot, left", rect(2, 8, 3, 9), "glyph")])
S("gb-crossed-clubs", "two golf clubs crossed in an X: each a small blocky head at the top, a 1 px shaft through the centre and "
  "a 2 px grip end at the bottom (the black club runs from the top left, the dark olive one from the top right; black on top)",
  [("olive club: head", poly((10, 2), (12, 2), (12, 3), (13, 3), (13, 5), (11, 5), (11, 4), (10, 4)), "olive"),
   ("olive club: shaft", line((11.5, 5.0), (5.5, 11.0), w=1.1), "olive"),
   ("olive club: grip end", poly((5, 11), (7, 11), (6, 12), (6, 13), (4, 13), (4, 12)), "olive"),
   ("black club: head", poly((5, 2), (7, 2), (7, 4), (6, 4), (6, 5), (4, 5), (4, 3)), "glyph"),
   ("black club: shaft", line((5.5, 5.0), (11.5, 11.0), w=1.1), "glyph"),
   ("black club: grip end", poly((10, 11), (12, 11), (13, 12), (13, 13), (11, 13), (10, 12)), "glyph")])
S("gb-theatre-mask", "two theatre masks side by side: on the left a brown mask lit gold on its lower left, on the right an "
  "orange mask lit gold on its lower right, each an oval face with a dark eye and mouth in the gold; a black shadow runs "
  "down the right mask's side and curls under both", [
      ("shadow: down the right side and under", line((15.5, 6.0), (15.5, 10.5), (13.0, 13.4), (10.5, 14.6), (5.0, 15.5), w=1.1, smooth=1),
       "shadow"),
      ("left mask: brown oval face", oval(3.3, 7.0, 3.5, 5.0), "brown"),
      ("left mask: gold, lit", oval(2.5, 8.6, 2.6, 3.1), "gold"),
      ("left mask: eye", rect(2, 6, 3, 7), "brown"), ("left mask: mouth", rect(3, 9, 5.5, 10), "brown"),
      ("right mask: orange oval face", oval(11.7, 7.0, 3.4, 5.0), "orange"),
      ("right mask: gold, lit", oval(12.3, 8.8, 2.4, 3.0), "gold"),
      ("right mask: eye", rect(12, 6, 13, 7), "orange"), ("right mask: mouth", rect(9, 9, 12, 10), "orange")])
S("gb-red-dash", "a small red lozenge: a 7 px bar with a 5 px bar above and below it, lighter at the corners",
  [("lozenge", U(rect(0, 1, 7, 2), rect(1, 0, 6, 3)), "mid"), ("lighter corners", U(rect(1, 0, 2, 1), rect(5, 0, 6, 1), rect(1, 2, 2, 3), rect(5, 2, 6, 3)), "edge")])
S("gb-grey-dash", "the same lozenge in light grey with olive corners",
  [("lozenge", U(rect(0, 1, 7, 2), rect(1, 0, 6, 3)), "mid"), ("olive corners", U(rect(1, 0, 2, 1), rect(5, 0, 6, 1), rect(1, 2, 2, 3), rect(5, 2, 6, 3)), "edge")])
S("gb-blue-dots", "the same lozenge in violet with lighter corners",
  [("lozenge", U(rect(0, 1, 7, 2), rect(1, 0, 6, 3)), "mid"), ("lighter corners", U(rect(1, 0, 2, 1), rect(5, 0, 6, 1), rect(1, 2, 2, 3), rect(5, 2, 6, 3)), "edge")])
S("gb-white-ball", "a small white ball at the top left: a white disc with a grey rim on its lower right, on a pink-grey shadow "
  "that darkens under it", [
      ("shadow: a pink-grey disc below right", circle(4.9, 5.0, 4.1), "pink"),
      ("shadow: darker under the ball", oval(5.4, 7.7, 2.2, 1.2), "dark"),
      ("ball: grey rim", circle(4.3, 4.1, 3.9), "grey"),
      ("ball: white", circle(3.9, 3.7, 3.5), "white")])
S("gb-star", "a small five-pointed star at the top left (its legs set close together), yellow lit from the upper left, over "
  "its dark drop shadow one pixel to the right", [("drop shadow: the star moved one pixel right", moved(STAR, 1.0, 0.4), "shadow"),
                   ("star", STAR, "star")], paint={"star": "linear"})
_S = [(6.6, 1.5), (1.2, 1.5), (0.6, 2.6), (1.4, 3.6), (5.0, 3.6), (6.0, 4.6), (5.6, 6.5), (0.4, 6.5)]
S("gb-green-dollar", "a small green dollar at the top left: a 1 px S and two bars through it, over its dark shadow one pixel "
  "to the right and down", [
      ("shadow: S", moved(line(*_S, w=1.1), 1.0, 1.0), "shadow"),
      ("shadow: bars", moved(U(rect(2, 0, 3, 8), rect(4, 0, 5, 8)), 1.0, 0.0), "shadow"),
      ("S", line(*_S, w=1.1), "green"), ("bars", U(rect(2, 0, 3, 8), rect(4, 0, 5, 8)), "green")])
S("gb-purple-bar", "two stacked light purple blocks (a tall one and a short one below), each over a dark violet shadow one "
  "pixel right and down", [
      ("upper block: shadow", rect(3, 1, 8, 6), "shadow"), ("lower block: shadow", rect(3, 7, 8, 9), "shadow"),
      ("upper block", rect(2, 0, 7, 5), "block"), ("lower block", rect(2, 6, 7, 8), "block")])
S("gb-flag-pin", "a flag pin drawn as a little banner: a white pole with a grey edge and a dark shadow line, a white foot; "
  "the flag a white frame (top bar, sides) with a grey top edge and middle line, pink panels, and a red, orange and yellow "
  "stripe, a dark shadow on its right and under it", [
      ("pole shadow", rect(3, 1, 4, 8), "shadow"), ("flag shadow, right", rect(9, 1, 10, 6), "shadow"),
      ("flag shadow, under", rect(5, 6, 10, 7), "shadow"),
      ("pole: grey edge", rect(2, 0, 3, 8), "grey"), ("flag: grey top edge", rect(4, 0, 10, 1), "grey"),
      ("flag: grey middle line", rect(4, 3, 8, 4), "grey"),
      ("pole: white", rect(1, 1, 2, 9), "white"), ("foot", rect(0, 8, 3, 9), "white"),
      ("flag: white top bar", rect(1, 2, 10, 3), "white"),
      ("flag: white left side", rect(4, 1, 5, 6), "white"), ("flag: white right side", rect(8, 1, 9, 6), "white"),
      ("flag: pink panels", U(rect(5, 1, 8, 2), rect(5, 5, 8, 6)), "pink"),
      ("stripe: red", rect(5, 4, 6, 5), "red"), ("stripe: orange", rect(6, 4, 8, 5), "orange"), ("stripe: yellow", rect(8, 4, 9, 5), "yellow")])
S("gb-heart", "a red heart: two round lobes and a point, lit from the upper left, a dark edge on its lower right",
  [("heart: dark edge", U(circle(4.0, 3.2, 3.3), circle(8.6, 3.2, 3.3), poly((0.8, 4.4), (11.8, 4.4), (6.3, 9.2))), "dark"),
   ("heart", U(circle(3.6, 2.9, 3.1), circle(8.2, 2.9, 3.1), poly((0.5, 3.8), (11.3, 3.8), (6.0, 8.6))), "red")],
  paint={"red": "linear"})
S("gb-small-heart", "a smaller red heart, lit from the upper left, a dark edge on its lower right",
  [("heart: dark edge", U(circle(3.0, 3.0, 2.0), circle(6.0, 3.0, 2.0), poly((1.0, 3.5), (8.0, 3.5), (4.6, 7.0))), "dark"),
   ("heart", U(circle(2.8, 2.6, 1.9), circle(5.6, 2.6, 1.9), poly((0.9, 3.0), (7.5, 3.0), (4.2, 6.6))), "red")],
  paint={"red": "linear"})
S("gb-green-screen", "a little monitor: a lilac frame round a bright green screen with a white wavy line and a black "
  "corner, a grey shadow line under it",
  [("shadow line under it", rect(1, 7, 10, 8), "grey"),
   ("frame: lilac", rect(0, 0, 9, 7), "lilac"),
   ("frame shadow on the right", rect(9, 1, 10, 7), "grey"),
   ("screen: green", rect(1, 1, 8, 6), "green"),
   ("white line on the screen", line((2.5, 3.5), (2.5, 2.5), (5.5, 2.5), (5.5, 3.5), (6.5, 3.5), w=1.0), "white"),
   ("white mark, left", rect(1, 4, 2, 6), "white"),
   ("black corner", rect(3, 4, 6, 5), "black"), ("black dot", rect(2, 5, 3, 6), "black"),
   ("frame: grey base", rect(4, 6, 7, 7), "grey2")])
S("gb-trophy", "a small trophy: a pale green cup (wide rim, narrowing bowl, a light highlight on its left), a C handle on each "
  "side, a short stem, a spreading foot and a dark plinth; a stray light pixel at the cell's top right (in the art)",
  [("left handle", U(rect(0, 1, 1, 4), rect(1, 1, 2, 2), rect(1, 4, 2, 5)), "handle"),
   ("right handle", U(rect(9, 1, 10, 4), rect(8, 1, 9, 2), rect(8, 4, 9, 5)), "handle"),
   ("cup: bowl", poly((2, 0), (8, 0), (8, 4), (9, 4), (7, 6), (3, 6), (1, 5), (1, 4), (2, 4)), "cup"),
   ("cup: highlight", rect(3, 2, 5, 4), "light"),
   ("stem", rect(4, 6, 6, 7), "dark"),
   ("foot", poly((3, 7), (7, 7), (8, 9), (2, 9)), "cup"),
   ("plinth: dark", rect(1, 9, 9, 10), "dark"),
   ("stray light pixel", rect(15, 0, 16, 1), "light")], paint={"cup": "linear"})
S("gb-white-cup", "a white goblet: a white rim, a bowl narrowing from the rim (pink inside, a white left edge, a grey right "
  "edge), a short stem, a white foot and a dark base line",
  [("base line", rect(2, 9, 9, 10), "dark"),
   ("bowl: grey right edge", poly((0, 1), (10, 1), (8, 4), (8, 5), (7, 6), (6, 6), (6, 8), (8, 8), (8, 9), (1, 9), (1, 8), (3, 8), (3, 6), (2, 6),
                                   (1, 4)), "grey"),
   ("rim: white", rect(0, 0, 9, 1), "white"),
   ("bowl: pink inside", poly((1, 1), (8, 1), (7, 4), (7, 5), (6, 6), (3, 6), (2, 5), (1, 3)), "pink"),
   ("bowl: light band", rect(2, 2, 6, 3), "silver"),
   ("stem", rect(3, 6, 5, 8), "pink"),
   ("foot: white", rect(1, 8, 4, 9), "white")])

if __name__ == "__main__":
    import fel
    want = sys.argv[1:]
    for sp in SPECS:
        if not want or sp["id"] in want:
            fel.build(sp)
