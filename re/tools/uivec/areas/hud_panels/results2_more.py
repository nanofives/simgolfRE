"""P41 hud_panels fix round: results rows for the faces, pads, pedestal, flat parts and trays (read by results2.py)."""
ROWS = {
    "member-mood-faces": ("done", "ten 16 px faces, each mirror-symmetric about the cell's x 8: disc r 8 with a one-pixel rim in "
                          "its own dark tone, body lit (radial paint measured in rings round its brightest pixels); 1 open grin "
                          "full of teeth (white rows, grey line), 2 smile line with corners up over a small open mouth with "
                          "teeth, 3-4 smile lines, 5 shallow smile, 6 straight mouth, 7 frown, 8 open frown with angry brows, "
                          "9 wide frown with slanted eyes, 10 toothy grimace (alternating white / grey teeth), slanted eyes, "
                          "brows and two horns on the notched top edge; eye glints where the art has them (faces 1, 2, 10)",
                          "redo: every face re-read from a per-face colour map (one letter per colour); round 1 missed the "
                          "glints, the horns, the teeth rows and the lighting; the comparison now shows each face at 8x",
                          "faces 8-10 are 16 px angry faces: brows and mouths are read pixel by pixel, worth a look"),
    "member-mood-faces-extra": ("done", "the two extra rows are pixel copies of faces 5-8: the same new face recipe",
                                "redo: same as member-mood-faces", ""),
    "shot-shape-low": ("done", "lavender block; pad: navy ring 1.5 px (inner edge fitted per cell), a light lip above it "
                       "with a dark one-pixel outer edge, body colour plus the big light crescent hugging the upper left "
                       "(soft edge), soft cast shadow below; shot: one continuous white line (round joins) rising, flat "
                       "top, landing and its bounce, the ball (white disc) with its brown shadow pixel, the dark ground "
                       "shadow lines; pressed cells one pixel lower",
                       "redo: round 1 painted the pad row by row (banding) and drew the bounce as a second line; now two "
                       "measured layers (body + crescent), the lip, the ball, and one path", ""),
    "shot-shape-rising": ("done", "same pad; shot: one continuous line from the lower left up to a flat top, down and on "
                          "to the lower right, ball at the top with its shadow, the ground shadow under the arc",
                          "redo: same pad rework; line re-read as one path", ""),
    "shot-shape-flat": ("done", "same pad; shot: one continuous line: long flat arc down into a V at x 55 and up to the "
                        "right; ball and shadow; sagging ground shadow",
                        "redo: the kink is now part of the one line (round 1 drew it as a separate stroke)", ""),
    "shot-shape-high": ("done", "same pad; shot: tall arc up to the pad's top, down, landing and rolling back left in "
                        "one line; the ball on the descent (no shadow pixel in the art); ground shadow",
                        "redo: same pad rework, one path", ""),
    "shot-shape-straight": ("done", "same pad; shot: nearly level line with its small hump, the landing dip and roll in "
                            "one line; ball and shadow; two short ground lines",
                            "redo: same pad rework, one path", ""),
    "employee-pedestal": ("done", "cylinder pedestal: cap = one-pixel dark outline and a convex lit surface (one bevel "
                          "paint fitted on each cell: bright upper-left inner rim, flat middle, darker lower right, thin "
                          "reflected light inside the lower-right outline); the cap's shade on the cylinder under it; the "
                          "side lit from the front left (bottom row's colours); a soft shadow to the right on the panel",
                          "redo: round 1's cap was flat with a dark strip and its right shadow a hard rectangle", ""),
    "member-bar": ("done", "white glossy bar: navy one-pixel outline, corners r 2.5 left / r 4 right (one exact per-corner "
                   "rounded rectangle), inside the art's own top-to-bottom colours, the right end darkening over its last "
                   "6 px (measured), the knob's circle cut out of its left end (fitted to the cut's edge; transparent as in "
                   "the art)",
                   "redo: round 1 drew the whole bar where the art has the knob's cut, and had no end shading", ""),
    "member-slider": ("done", "lilac block with its navy right edge line; two bars of the member-bar recipe (rounder right "
                      "corners r 4, the end shading) each with its one-pixel drop shadow; the ball knob",
                      "fix 'rounded corners': right corners r 4 read on (99, 2) -> (101, 6), left r 2.5; added the drop shadow", ""),
    "member-page-left": ("done", "lilac tab, top two rows lit; two cream triangles, each with a one-pixel shadow hugging "
                         "its upper edge (darkest at the tip, fading toward the top vertex) and the darker middle of its "
                         "upright edge",
                         "fix 'better shade': the shadow now follows the art's fade along the edge instead of one hard dark bar", ""),
    "member-page-right": ("done", "mirror layout of the left tab with the art's own shadow sides: shadow along the upper "
                          "edge and down the upright left edge",
                          "fix 'better shade': same as the left tab, plus the shaded upright edge", ""),
    "match-plate": ("done", "two parts as in the art: the strip on top (brightening left to right, lit top rows, top right "
                    "corner r 4, dark right bevel) and the plate below (top left r 13, top right r 4, one measured lit "
                    "surface: lit top edge, dark right bevel); grid of sunken cream cells; two bars (bar recipe) and the knob",
                    "fix 'rounded top corners': both top corners of strip and plate rounded (one exact per-corner rounded "
                    "rectangle, no seam); round 1 had merged strip and plate and left the right corners square", ""),
    "golfer-story-band": ("done", "lavender band as one measured lit surface (light left edge, dark right and bottom "
                          "bevels); the cream box is a sunken well: its walls one lit surface measured on the art whose "
                          "edge is the well's opening (dark left and top walls, lit right and bottom walls), opening r 8",
                          "fix 'better shading': round 1 drew the box with a navy line; the art's box is a well with shaded walls", ""),
    "member-panel": ("done", "tray: the lobe is the art's silhouette (three tab halos fitted to its arcs + the plate edge "
                     "down x 225), one measured lit surface, a dark one-pixel line on the lobe's outer edge, a lit ring on "
                     "each halo's lower right, the rivet at the foot, the tan stub; sunken list well and scroll strip; the "
                     "new blue tabs and page arrows placed from their cells; the yellow tab with the new lit rim",
                     "redo: round 1's lobe was three stacked rectangles; all parts are this round's redrawn elements",
                     "tray composed from parts: the halo sockets' light rings are sampled at one point each"),
    "golfer-action-panel": ("done", "same tray recipe (silhouette lobe, dark edge line, lit rings, rivet); raised inner "
                            "panel and three sunken boxes; the new club / crossed / trophy bubbles, stats bubble and tabs "
                            "placed from their cells; yellow tab with the new rim",
                            "redo: new lobe and lighting; all buttons are this round's redrawn ones", "tray composed from parts"),
    "employee-panel": ("done", "same tray recipe; pedestal well; the eight new lit pedestals; the note box with the art's "
                       "arched top edge (circle R 565 through rows 516-524) and its measured inner shading (10 rows down "
                       "from the top line, 6 px in from the left); hire / fire / details bubbles placed over it; the new "
                       "tabs and thumbs-up",
                       "fix 'better shading': lit lobe, sunken boxes measured over their real depth, arched note box, lit "
                       "pedestals (round 1: flat white box with a straight top and notches)",
                       "the box edge does not step up under each bubble as the art's does: the bubbles cover most of it"),
}
