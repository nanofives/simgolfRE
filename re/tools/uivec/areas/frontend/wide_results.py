"""P41W: writes results.tsv for the screens after the title and difficulty (their rows are written by hand / wide_diff)."""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import wide_screen as W   # noqa: E402

GROUND = "SPAN: the cloud ground (380-384 blobs fitted inside the frame, 2,500-2,900 seeded continuation blobs outside with the art's texture statistics; residual 1.4-1.8 lum levels incl. the palette dither)"

ROWS = {
    "load": [("load", "done",
              GROUND + ". CENTRE, defined for all x: the navy slab = the circle c (784.47, 250.30) r 527.13 united with the half plane right "
              "of its centre (runs on to any right edge), lit edge on the circle's left half only; the inset line as analytic pieces (top "
              "run y 60.6 from under the plate, 6 px fillet, circle c (783.11, 253.84) r 511.02, fillet, bottom run y 529.5 into the OK "
              "button) with its shadow; the faint ball behind the page (circle c (544.17, 252.55) r 230.51, a lit sphere, faint dimples); "
              "the page and info bar as recessed windows (dark rim above / left, lit lip below), the plate's drop shadow, the left "
              "column's soft shadow on the ground. load-backdrop (it holds every approved element) unchanged on top. No LEFT / RIGHT anchors.",
              "(1) The faint ball shows only as slivers (between plate and page, under the page): its dimples are by eye (too little to fit "
              "a lattice). (2) The recess dark rim and lip around the page are fitted (rms 21.7 levels on their band, from 31; the band "
              "also holds the faint ball's dimples).")],
    "pro": [("pro", "done",
             GROUND + ". The right half exactly as Load Game (same slab circle, inset line, faint ball, recessed page and info bar, plate "
             "shadow: Pick a Pro's slab crossings lie on Load's circle, median -0.4 px). Its own: the thin line from the ring's stub down "
             "x 19.6 into the bottom bar, in the approved stub's style (1.2 px navy over a 1.0-grown shadow moved 1.6, 1.6); the cream "
             "ball in the ring is art (opaque cut 78, 69, 125 x 124 under the ring); the ring, list and bottom bar's soft shadow on the "
             "ground. pro-backdrop unchanged on top. No LEFT / RIGHT anchors.",
             "Same as Load Game. The ball picture is the PCX pixels upscaled (art), sharp edges hidden by the ring's inner outline.")],
    "theme": [("theme", "done",
               "SPAN: the flat navy field (103, 103, 159; every sample off the elements is that value), one rectangle to x +-2000. "
               "CENTRE: the frame line as analytic pieces (top run y 44.2 from under the plate, 8 px fillet, circle c (1228.62, 296.14) "
               "r 1208.42 fitted on 140 rows at rms 0.18 px, fillet, bottom run y 547.3 into the OK button; 1.6 px) with its shadow; "
               "the same shadow under the approved header wires (the wires are approved, their shadow was in the art); the table's "
               "recess (dark rim above / left). theme-backdrop unchanged on top. Wider screens show more flat navy on both sides.",
               "A light lip under the table was fitted but rejected: it drew a glow along the name column's curved left edge that the "
               "art does not have.")],
    "world": [("world", "done",
               GROUND + ". CENTRE, defined for all x: the navy region = the disc c (-37.77, 477.0) r 541.91 united with the half plane "
               "left of its centre (runs on to any left edge), soft edge; the bottom-right navy blob = the ellipse c (796.21, 528.25) "
               "r (108.17, 143.17) united with the band right of its centre under its top tangent (y 385.08), so it runs along the "
               "bottom right at any width and the back button stays on it; the capsule chains (polylines through the capsules' dark "
               "ends, seen in the gaps), the line from the lowest right capsule (circle c (141.89, 455.10) r 528.59, 5 px fillet, run "
               "y 477 onto the blob, T-cap x 728.5) and the three plate wires, with one fitted shadow. The globe and its shadow are art "
               "(cut 4, 108, 424 x 424, ground_diff matte t0 9 t1 30 feather 4). world-backdrop + world-back unchanged on top.",
               "(1) The blob is not RIGHT-anchored: it would carry its art edge but leave the approved back button on the cloud, so it is "
               "continued as a band instead; say if it should ride the right edge (the button would then have to move too). (2) The "
               "approved capsules carry their own shadow (moved 2.5, 3.5); the art's capsule shadows look a little broader, not added.")],
    "editor": [("editor", "done",
                GROUND + ". CENTRE, defined for all x: the navy hill = the circle c (417.71, 1356.04) r 1190.46 (both sides of the "
                "screen within 0.42 px) with a measured lit rim; it keeps curving down outside the frame (edge at y 406 / 381 at x "
                "-300 / 1100). The cream sheet behind the pills (x 63..245, bottom arc through 3 read points, navy outline, cast "
                "shadow); the list's recess rim (dark ramp 8 px above / left, light lip 9 px right / below, read off the profiles); the "
                "two thin lines on the right (page to reset at y 18.2; reset to OK along the circle c (460.95, 137.39) r 314.31) with "
                "their shadow. The portrait window is art (opaque cut 327, 16, 121 x 229) with a fitted cast shadow. "
                "editor-backdrop (state female) unchanged on top.",
                "(1) The hill continues as a hill: at 21:9 and 32:9 the navy drops away toward the screen corners (cloud above it), as "
                "the circle goes; the alternative is to level it off outside the frame. (2) The sheet's left edge is hidden by the pills "
                "and taken by symmetry. (3) The window's cast shadow came out offset 16 px right (rms 4.7 from 16.8: it also darkens "
                "under the icon column)."),
               ("editor_male", "done",
                "The same backdrop and scene as editor (the male sheet differs from the female one only in 2,126 px inside the approved "
                "icons' and name card's area); the approved editor-backdrop state male on top, the window cut from the male sheet.",
                "Same as editor.")],
}


if __name__ == "__main__":
    for screen, rows in ROWS.items():
        W.write_results(screen, rows)
        print("wrote", screen)
