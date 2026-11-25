"""The small HUD buttons of 3mainLowerLeft.pcx (zoom, rotate, info, pause, tools) as bubble specs.

Every glyph was read on the idle cut with cov.py (coverage digits) and look.py (gridded 12-14x view), 2026-11-24.
Coordinates are local to the idle cut (sheet = local + cut origin).
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from geom import rect, line, poly, circle, outlined, sub, rrect_c, curve, arc, mirror_x  # noqa: E402

SPECS = []


def S(**k):
    SPECS.append(k)


# zoom in / out: a DARK navy disc sunk in the tray, the glyph light cream at the top shading to tan at the bottom
S(id="hud-zoom-in", circle=(12.5, 11.5, 10.5), ground="auto", shadow=False, paint={"glyph": "linear"}, glyph_dark=-120,
  how="dark navy disc in a lit socket; glyph a plus: two 3 px bars with flat ends (vertical x 11..14, y 5.3..17.5; "
      "horizontal x 5.8..19.4, y 10..13), light cream shading to tan",
  glyph=[("plus: vertical bar, flat ends", rect(11.0, 5.3, 14.0, 17.5), "glyph"),
         ("plus: horizontal bar, flat ends", rect(5.8, 10.0, 19.4, 13.0), "glyph")])
S(id="hud-zoom-out", circle=(12.5, 11.5, 10.5), ground="auto", shadow=False, paint={"glyph": "linear"}, glyph_dark=-120,
  how="dark navy disc in a lit socket; glyph a minus: one 3 px bar with flat ends (x 5.5..19.3, y 10..13)",
  glyph=[("minus: one bar, flat ends", rect(5.5, 10.0, 19.3, 13.0), "glyph")])
# rotate (fix round, "center it"): the glyph's box is centred on the navy disc (filled-disc centroid (12.6, 11.85) on both idle
# cuts, the same circle on both buttons; the art's arrows sit 0.8 px left / 1.6 px right of it). The two arrows are mirror images
# about x 12.6; the head is symmetric about the bar's centre line; the bend's outer corner is round (the stroke's own join).
ROT_C = (12.6, 11.85, 10.15)
ROT_FIX = {"idle": ROT_C, "hover": (ROT_C[0], ROT_C[1] + 1.0, ROT_C[2])}
ROT_LEFT = [("bent shaft: stem up, bar to the left (3 px, flat foot, round outer corner at the bend)",
             line((17.25, 17.85), (17.25, 11.35), (11.0, 11.35), w=3.0), "glyph"),
            ("head: triangle pointing left (tip (6.35, 11.35), back edge x 11.75, 10.7 px tall)",
             poly((11.75, 6.0), (6.35, 11.35), (11.75, 16.7)), "glyph")]
S(id="hud-rotate-right", circle=ROT_C, circle_fixed=ROT_FIX, measure_shift=(1.55, 0.23), ground="auto", shadow=False, paint={"glyph": "linear"}, glyph_dark=-120,
  how="dark navy disc; glyph a bent arrow turning right, its box centred on the disc (fix round): 3 px stem (x 6.45..9.45) "
      "rising from y 17.85, a 3 px bar to the right (centre y 11.35) with a round outer corner, a triangular head (back edge "
      "x 13.45, y 6..16.7, tip (18.85, 11.35)); the mirror image of the left arrow about the disc's axis",
  glyph=[(n.replace("left", "right"), mirror_x(sh, ROT_C[0]), k) for n, sh, k in ROT_LEFT])
S(id="hud-rotate-left", circle=ROT_C, circle_fixed=ROT_FIX, measure_shift=(-0.75, -0.9), ground="auto", shadow=False, paint={"glyph": "linear"}, glyph_dark=-120,
  how="dark navy disc; glyph a bent arrow turning left, its box centred on the disc (fix round): 3 px stem (x 15.75..18.75) "
      "rising from y 17.85, a 3 px bar to the left (centre y 11.35) with a round outer corner, a triangular head (back edge "
      "x 11.75, y 6..16.7, tip (6.35, 11.35))",
  glyph=ROT_LEFT)
# info, pause, tools: tan glass bubbles with a navy rim and a navy glyph
S(id="hud-info", circle=(15.5, 15.5, 15.5), ground="auto", shadow=True,
  how="tan bubble, navy rim; glyph: a navy ring inside the rim (centre (16, 16), radius 11, 2 px), the letter i as a dot "
      "(r 2.8) over a stem with a rounded top (x 13.5..18.4, y 15..24.8)",
  glyph=[("inner ring: a 2 px navy circle line", outlined(circle(16.0, 16.0, 10.97), 2.0), "navy"),
         ("i: dot", circle(16.0, 10.4, 2.8), "navy"),
         ("i: stem, rounded top", rrect_c(15.95, 19.9, 2.45, 4.9, 1.2), "navy")])
S(id="hud-pause", circle=(14.5, 14.0, 14.0), ground="auto", shadow=True,
  how="tan bubble, navy rim; glyph: two upright 4 px bars with rounded ends (x 7..11 and 15..19, y 6.8..19.3)",
  glyph=[("left bar, rounded ends", rect(7.0, 6.8, 11.0, 19.3, 1.8), "navy"),
         ("right bar, rounded ends", rect(15.0, 6.8, 19.0, 19.3, 1.8), "navy")])
S(id="hud-tools", circle=(13.5, 13.0, 12.5), ground="auto", shadow=True,
  how="tan bubble, navy rim; glyph: a double open-ended wrench lying from the upper left to the lower right: a 2.6 px "
      "shaft and at each end a jaw drawn as a C ring (r 3, 2.2 px, round ends) whose gap opens away from the shaft "
      "(upper jaw up-left, lower jaw down-right)",
  glyph=[("shaft", line((8.6, 10.4), (15.0, 14.2), w=2.6), "navy"),
         ("upper-left jaw: a C ring (r 3, 2.2 px) open toward the upper left", arc(6.4, 8.4, 3.0, -100.0, 250.0, 2.2), "navy"),
         ("lower-right jaw: a C ring (r 3, 2.2 px) open toward the lower right", arc(17.2, 16.4, 3.0, 75.0, 290.0, 2.2), "navy")])


if __name__ == "__main__":
    import bel
    want = sys.argv[1:]
    for sp in SPECS:
        if not want or sp["id"] in want:
            bel.build(sp)
