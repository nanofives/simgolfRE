"""P41 hud_panels: write results.tsv from the last full run's scores (log/roadmap/P41/work/hud_panels/_run.log).
Usage: py -3.12 re/tools/uivec/areas/hud_panels/results.py
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import common as C  # noqa: E402

HOW = {
    "member-panel": ("lavender tray plate (top y 494, lobe on the left) as one measured lit surface; recessed list well and scroll strip (measured inner shading); the golfer / staff tabs and both page arrows placed from their own redrawn cells; the yellow group tab drawn in place with the blue-tab recipe",
                     "tray composed from parts: the plate's outline under the three tabs is hidden and authored loosely; scroll track between the arrows is only the strip's shading"),
    "member-slider": ("lilac block, two white glossy bars (navy line, the bar's own top-to-bottom colours), ball knob over their left ends, dark right edge line", ""),
    "member-bar": ("white glossy bar: navy outline rounded rectangle, inside the art's top-to-bottom profile (grey and white versions)", ""),
    "member-hearts": ("four small hearts per cell (two round lobes + a point), dark outline line, flat grey or red fill with a lighter middle, a light edge to the lower right", "the hearts are 7 px: the lobes may read rounder than the art's flat-topped shield"),
    "member-ball": ("glossy white ball: navy circle line, radial paint from a light spot up-left through the art's diagonal colours", ""),
    "member-mood-faces": ("ten faces from one recipe: a disc with a darker rim, eyes (hollow on the grins), mouth per face: open grin with teeth, smile with teeth, smile lines, flat lines, frown, angry brows with an open mouth, angry brows with teeth", "16 px faces: eyes and mouths were read from character dumps; faces 8-10 (brows, open mouths) need Mariano's eye"),
    "member-mood-faces-extra": ("the two extra rows are pixel copies of faces 5-8 (checked: mean difference 0-2 levels); drawn with the same face recipe", "same reading as member-mood-faces 5-8"),
    "member-page-left": ("lilac tab with its top rows lit, two cream triangles (tip on the left, upright base 14 px away) with a dark line along each upper edge", ""),
    "member-page-right": ("mirror of the left page arrows", ""),
    "member-skill-bar": ("seven separate sunken sockets 11 px apart (dark rounded slot, light line under it), each lit in its own colour (red to bright green) or navy", "interior score stays high (15): the sockets are 2 px tall, every pixel is an edge pixel"),
    "member-pips": ("the two 7 px cells are single hearts (red, grey) cut from the heart column: same heart recipe", ""),
    "golfer-stats-button": ("tan bubble (rim + body circles measured, body paint a measured lit surface, cast shadow fitted) on the flat lavender square; glyph: golfer (round head with cap peak, shoulder bar, hanging arms, torso, two legs, feet on a ground line) beside a list of three rows (dash + round dot)", "the list's marks are read as round dots 3 px wide; they could be meant as small plus signs"),
    "tab-golfer": ("blue glass tab: halo disc (fitted to the cell outline, vertical gradient), navy rim (first dark ring in from the halo), blue body (measured paint), golfer in the follow-through: club held level behind the head with its head turned down, slim body, straight front leg, back leg trailing to a toe", ""),
    "tab-golfer-staff": ("blue tab recipe; glyph: a man (head as a dark oval line, raised left arm, right arm to the post, torso block, two legs, feet) beside an upright post with a knob", "tiny glyph: the man's arms and the post's top read roughly; worth a look"),
    "golfer-action-panel": ("tray plate as one measured lit surface + raised inner panel (top y 506, left x 332) + three lilac boxes (measured inner shading); club / crossed / trophy (hover cells), stats bubble, blue group and staff tabs placed from their redrawn cells; yellow swing tab drawn in place", "tray composed from parts; the stats bubble is the idle cell's (the tray's copy matches it at 13 levels)"),
    "match-plate": ("lavender plate (rounded top-left r 13, measured lit edge, flat face) + two white bars and the ball knob + navy grid (18 cells 18 px apart, a double line, a wide cell; 4 rows) of sunken cream cells", ""),
    "shot-shape-low": ("lavender block (column profile) with an oval pad: navy ellipse line (fitted), inside the art's row-by-row colour (shot lines left out), light crescent upper left, soft shadow below; shot: one white line rising in a low flat arc, landing and a small bounce, above its dark ground line", ""),
    "shot-shape-rising": ("same pad; shot: a tall arch from the lower left up to x 40 and steeply down, then a tail running down to the lower right, over its ground shadow line", ""),
    "shot-shape-flat": ("same pad; shot: a low flat curve with a separate short rising stroke at the right (the kink), over a sagging ground shadow", ""),
    "shot-shape-high": ("same pad; shot: a tall pointed arc up to the pad's top, landing and rolling back left, over its ground line", ""),
    "shot-shape-straight": ("same pad; shot: a nearly level line with a small hump, over a short ground line", ""),
    "joecool-stats-button": ("same golfer-beside-a-list glyph as the Member list copy (moved by the art's own offset, 1 px); lavender square with its top-right corner cut on the art's slant", ""),
    "tab-golfers-group": ("blue tab recipe; glyph: three figures, two standing with hands on hips, one jumping between and above them with arms up", ""),
    "tab-golfer-swing": ("blue tab recipe; the same follow-through golfer glyph as tab-golfer (the art is the same drawing)", "the roster calls it mid swing: it is the same follow-through pose as the golfer tab"),
    "joecool-tab-staff": ("blue tab recipe; the man-beside-a-post glyph of tab-golfer-staff", "see tab-golfer-staff"),
    "golfer-club-button": ("tan bubble on its lavender disc and stub (strip with lighter top edge); glyph: upright club (black grip, brown shaft, dark head turned left) with a ball beside its head (white top, dark lower half)", ""),
    "golfer-crossed-button": ("tan bubble on disc and stub; glyph: two clubs crossed in an X: light-brown shafts, black grips at the bottom, small blade heads at the top", "heads are 3-4 px blades: read them enlarged"),
    "golfer-trophy-button": ("tan bubble on disc and stub; glyph: two-handled cup, one profile mirrored about x 16 (bowl, stem, foot), shaded across from the art's columns, ring handles, rim and foot lines", ""),
    "employee-panel": ("tray plate as one measured lit surface + pedestal well and note box (measured inner shading); eight pedestals, hire / fire / details (hover cells), thumbs-up bubble and blue tabs placed from their redrawn cells; the yellow staff tab drawn in place", "tray composed from parts; thumbs-up is the idle cell's (13-21 levels from the tray's copy)"),
    "employee-pedestal": ("cylinder: top ellipse (navy line fitted, the art's colours down its middle: lilac / green / dark), side down to the cell foot with a dark right band; second-row cells also show the side of the pedestal above", ""),
    "employee-hire": ("tan bubble on the note box's top edge (lavender plate, light line over dark line, white box with inner shadow; the edge steps under the bubble); glyph: navy running man in a cap (head, leaning torso, two arms, striding legs, foot) pulling a green arrow (triangle pointing right, dark outline, light streak, short shaft)", ""),
    "employee-fire": ("same bubble and ground; glyph: red thumbs-down: green cuff, fist with darker left side and three finger lines, thumb hanging to a point", ""),
    "employee-resume": ("same bubble and ground (the box edge here runs down to the right); glyph: purple figure in a hat (head, body block, legs, foot line) holding up a white clipboard (olive edge, three dark and two grey text lines, cyan clip)", ""),
    "employee-page-left": ("flat lavender tab with one long dart pointing out (tip at mid height, back a shallow chevron); lighter, white, or darker than the tab per state", ""),
    "employee-page-right": ("mirror of the left tab", ""),
    "tab-staff-group": ("blue tab recipe; the three-figure group glyph of tab-golfers-group", ""),
    "employee-tab-golfer": ("blue tab recipe; the follow-through golfer glyph of tab-golfer", ""),
    "employee-thumbs-up": ("tan bubble on the lavender square; glyph: green thumbs-up: thumb standing up (dark outline, lighter inside), fist with four finger lines on its left, cuff at the right", ""),
    "golfer-info-panel": ("lavender slab (plate minus the sunken text hole) as one measured lit surface; arched title plate (one arc R 516, navy line, cream with its shading, tan box below with left shading, divider line), cream box (navy line, measured sunken inside), 2 x 18 sunken score cells + the wide one on a navy block; the hole left empty as in the art", ""),
    "golfer-story-band": ("lavender band as one measured lit surface with the large cream box (boxes.py measurement: navy line, corner r 9.75)", ""),
    "golfer-tab-info": ("tan tab bubble on the black cell with the lilac strip behind (row profile) and its dark shadow band; glyph: the golfer-beside-a-list (moved by the art's own offset) also holding a light club down to the lower left", "list marks read as dots, as on golfer-stats-button"),
    "golfer-tab-mood": ("tan tab bubble; glyph: smiling face: two upright oval eyes, a 2 px smile with slightly thicker ends", ""),
    "golfer-tab-swing": ("tan tab bubble; glyph (it is not a swing): a big boot tipped back (cuff up at the left, heel, toe turned up) kicking a running figure (head, sweat drop, torso, arms, two legs)", "the roster's 'golfer mid swing' is a boot kicking a figure; Mariano to confirm the reading"),
    "golfer-tab-photo": ("tan tab bubble; glyph: camera: body block, prism on top, small button, light lines down both ends, lens = light ring round a dark core", ""),
    "golfer-tab-story": ("tan tab bubble; glyph: an open book in outline: per page an upright outer edge, a top edge curving into the spine, two stacked bottom curves into the spine foot, an inner page line; the right page mirrored; the spine", ""),
    "golfer-tab-ok": ("blue / yellow / dark tab bubble (disabled rim taken from the idle cell, see bubble_geom.json note); glyph: bold check mark with its two arm ends and its foot cut level", ""),
    "golfer-tab-history": ("tan tab bubble; glyph: the story tab's outline book moved down, under a green half-ring arch with a dark edge and a light streak (its right foot a little lower)", "the roster's 'green and yellow arrows': only one green arch is in the art"),
    "golfer-tab-thought": ("tan tab bubble (the cell starts under a cream band, kept); glyph: navy speech bubble (ellipse + curved tail) with a light exclamation mark cut out of it", ""),
}
ART = ["heads-halo-male", "heads-flat-male", "heads-halo-female", "heads-flat-female", "bodies", "barrel", "portrait-backing"]


def main():
    scores = {}
    log = os.path.join(C.OUT, "_run.log")
    if os.path.exists(log):
        for line in open(log, encoding="utf-8", errors="replace"):
            m = re.match(r"([\w-]+): interior_mad ([\d.na]+) edge_px ([\d.na]+)", line.strip())
            if m:
                scores[m.group(1)] = (float(m.group(2)), float(m.group(3)))
    for el, (how, unsure) in HOW.items():
        mad, edge = scores.get(el, ("", ""))
        status = "done" if os.path.exists(os.path.join(C.OUT, el + ".png")) else "blocked"
        C.record(el, status, how, mad, edge, unsure)
    for el in ART:
        C.record(el, "kept_art", "marked art by Mariano: stays a texture", "", "", "")
    print(C.RESULTS)


if __name__ == "__main__":
    main()
