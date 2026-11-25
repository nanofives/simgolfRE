"""P41 hud_panels round 3: write log/roadmap/P41/work3/hud_panels/results.tsv (id, status, how, changed, unsure)."""
import os
import sys

os.environ["HUDP_WORK"] = "work3"
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import common as C  # noqa: E402

A_HOW = ("faithful: the approved surface of its neighbours, and on it every tone patch of the art's glyph as its own shape "
         "(patches.py: glyph pixels = art pixels that differ from the surface; k-means tones; each 4-connected patch its "
         "pixel outline with one round of corner cutting, in that patch's own mean colour, over a base of the whole glyph)")

ROWS = [
    ("match-plate", "done",
     "one silhouette x 297..800 with a concave cove cut out of its top left (circle fitted to the 20 edge pixels: "
     "(292.62, 302.79) r 16.45, 0.24 px residual); the strip (rows 301-312) and the plate (from 313, top left corner "
     "r 13 inside the cove) each one fitted lit surface whose edge is a rounded box (r 4 at the top right corners), so "
     "the lit top edge rolls over into the dark right edge; the strip's left-to-right brightening as one tone; grid of "
     "square sunken cells, each with its own measured left and top shadow; bars with their one-pixel navy outline "
     "(rows 356-370, 377-391), shadow row and the knob ring (307.0, 373.5) r 8",
     "'Rounded edges': round 2's strip met the plate in a straight diagonal plus a convex arc (a kink at the top left): now "
     "one concave cove as in the art; the edges are rolled (lit top 191 183 159 128 into dark right 104 78 58 34 through "
     "a rounded corner instead of a seam); round 2's bars had lost their navy outline (sampled half a pixel off) and its "
     "cells had rounded corners the art lacks: both fixed",
     "the strip's and plate's top right silhouette stays square as in the art; the rounding there is the bevel's roll"),
    ("golfer-trophy-button--a", "done", A_HOW + "; k 6, per state on that state's art",
     "new: faithful version (round 2 rejected twice)", "patch version: reads close to the art's pixels by design"),
    ("golfer-trophy-button--b", "done",
     "clean cup: rim one row x 11-21 shaded per column (light left end, olive right half); bowl a lathe x 12-20 narrowing "
     "over rows 581-582 into the stem; bowl paint one colour per column (dark wall, olive, khaki, cream, grey, dark); a "
     "cream highlight x 15-18 rows 575-580 with a near-white spot (16.5, 577.5); dark bottom band; stem x 15-17 with a "
     "knot; foot x 12.6-19.4 as a dark top row over a lit front row; handles one-pixel rings round 2 px holes closing on "
     "the walls (centres x 10.5 / 21.5, row 575)",
     "the five round-2 differences removed: handles 1.3 px rings round a 0.85 px hole -> 1 px rings round 2 px holes; flat "
     "round-cornered bowl bottom -> a taper into the stem; smooth chrome gradient -> the art's per-column bands; foot "
     "7.3 px -> 6.9 px two-tone; rim one colour -> shaded left to right", ""),
    ("joecool-tab-staff--a", "done", A_HOW + "; dark pixels only, k 4", "new: faithful version", ""),
    ("joecool-tab-staff--b", "done", "the drawing of tab-golfer-staff--b moved by the art's own offset",
     "same as tab-golfer-staff--b", ""),
    ("tab-golfer-staff--a", "done", A_HOW + "; dark pixels only, k 4", "new: faithful version", ""),
    ("tab-golfer-staff--b", "done",
     "a man beside a post: head a 3 x 3 round block with a one-pixel chin and a light row under it; shirt a dark shape "
     "with the tab's own lit colour showing at x 14 and 16 (chest); right arm down to the post with a hand; left arm a "
     "thin '<' to the waist with a light gap beside the shirt; hips x 12-17; legs x 12-13 / 15-16 with a lit gap; feet "
     "two rows, four pixels each; post one pixel with a knob r 1.3; figure painted as a dark core over a lighter edge",
     "head 3.4 x 4 drop -> 3 x 3 with chin and neck gap; thick left arm with a fist knob -> thin '<'; three equal bars -> "
     "a shirt with a lit chest; wide hips, 2 px legs, long feet -> art widths; one flat navy -> core + edge tone", ""),
    ("member-mood-faces--a", "done", A_HOW.replace("approved surface of its neighbours, and on it", "cell ground, and") +
     "; the whole face, k 8, clipped to the disc (face 10: the horned silhouette)", "new: faithful version",
     "patch faces keep the art's dither, by design"),
    ("member-mood-faces--b", "done",
     "the ten faces with round 2's measured feature positions, painted with tone: rim the face's own edge shade (median "
     "of the edge ring), skin a radial lighter at its light centre and soft into the rim, every eye / line / brow a "
     "mid-tone shape over a dark core 0.35 px inside it, a glint on an eye where the art has a lit eye pixel, teeth and "
     "grey teeth lines in their own colours",
     "round 2's near-black thick rim, solid black eyes and flat one-tone lines replaced by the art's tones (edge shade, "
     "core and edge, glints)", "faces 8-10 angry features keep round 2's shapes"),
    ("member-mood-faces-extra--a", "done", "the extra rows (faces 5-8) by the same patch method on their own pixels",
     "new: faithful version", ""),
    ("member-mood-faces-extra--b", "done", "the extra rows with the member-mood-faces--b recipe",
     "same as member-mood-faces--b", ""),
]

if __name__ == "__main__":
    path = os.path.join(C.OUT, "results.tsv")
    with open(path, "w", encoding="utf-8") as f:
        f.write("id\tstatus\thow\tchanged\tunsure\n")
        for r in ROWS:
            f.write("\t".join(r) + "\n")
    print(path, len(ROWS))
