"""P41 hud_panels fix round: write log/roadmap/P41/work2/hud_panels/results.tsv (id, status, how, changed, unsure).
Usage: py -3.12 re/tools/uivec/areas/hud_panels/results2.py
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import common as C  # noqa: E402

BLUE = ("blue glass tab: halo disc, navy rim now one lit pixel (rim_paint: lighter upper left, near black lower right, "
        "measured on rays), body shading re-measured on the corrected body circle; ")
ROWS = {
    "tab-golfer": ("done", BLUE + "golfer in the follow-through drawn with volume (figs.limb): club head hooked down, shaft "
                   "unbroken into the hands, round head, neck, front arm bent up at the elbow, torso tapering from the shoulders "
                   "and leaning, hips, back leg bent at the knee onto its toe, straight front leg with a foot",
                   "redo: round 1 was a stick figure; every part re-read at 16x and drawn with its width; rim thinned to the art's one pixel", ""),
    "tab-golfer-swing": ("done", BLUE + "the same follow-through golfer as tab-golfer (art differs by 2.6 levels), moved by the art's offset",
                         "redo: same new figure and rim as tab-golfer", ""),
    "employee-tab-golfer": ("done", BLUE + "the same follow-through golfer as tab-golfer",
                            "fix 'golf stick disconnected': the shaft now runs unbroken from the club head into the hands (one limb, round joins)", ""),
    "tab-golfer-staff": ("done", BLUE + "a man beside a post: head the dark block at the TOP (round crown, pointed chin), shirt "
                         "as three dark lines with lit gaps, right arm reaching down to the post, left arm a '<' with the hand at the "
                         "waist, solid lower torso, two legs with a gap, feet turned out; post one pixel wide with a round knob",
                         "redo: round 1 had read the head as an oval lower down; re-read pixel by pixel; figure drawn with volume", ""),
    "joecool-tab-staff": ("done", BLUE + "the man-beside-a-post figure of tab-golfer-staff (art differs by 2.7 levels)",
                          "redo: same new figure and rim as tab-golfer-staff", ""),
    "tab-golfers-group": ("done", BLUE + "three figures all standing with their hands on their hips (head, neck, shoulders, "
                          "torso, both arms bent out at the elbow and back to the waist, hips, two legs, feet turned out); the middle "
                          "one smaller and higher (further back)",
                          "redo: round 1 drew the middle figure jumping with arms up; the dump shows elbows out and hands at the waist like the others",
                          "the middle figure stands on nothing (its feet end at row 417 between the others' heads), as in the art"),
    "tab-staff-group": ("done", BLUE + "the three-figure group of tab-golfers-group (art differs by 2.6 levels)",
                        "redo: same new figures and rim as tab-golfers-group", ""),
    "golfer-tab-mood": ("done", "tan tab bubble (unchanged recipe, as on the approved tabs); smiley symmetric about x 519.05: two "
                        "upright oval eyes, one continuous smile line 1.6-1.75 px thick, a dimple tick across each end",
                        "redo: round 1's face sat 1.6 px right of the art and its strokes were grey; re-read, drawn in the art's navy, "
                        "mirror-symmetric, the smile one smooth path reaching its dimples", ""),
    "golfer-tab-photo": ("done", "tan tab bubble; camera: dark body with rounded corners, lit top-left face and a softer low-left "
                         "patch, prism on top shaded from lit upper left to dark lower right, shutter button, light line at each end, "
                         "lens = light ring round a dark core",
                         "redo: re-read (round 1 sat 1.5 px right); added the body's lit faces and the prism's shading", ""),
    "employee-hire": ("done", "tan bubble on the note box (unchanged recipe); running man: head, cap shaded lit-front to dark-back "
                      "with its peak, thin neck, torso leaning back, back arm bent with the forearm returning to the waist, front "
                      "arm to the hand gripping the shaft, back leg bent down-left to its foot, front leg with a two-row foot; arrow: "
                      "a right-pointing head with straight sides and a bowed base, dark-green outline, green inside, light streak "
                      "inside its base; a thick stub shaft (dark edges, pale top line)",
                      "redo: round 1's shaft was a thin line and the figure a stick; shaft is the art's 6 px stub, the man drawn with volume", ""),
    "employee-fire": ("done", "tan bubble; red thumbs-down: fist with a knuckle bump, lit back of the hand with a crease, a divider "
                      "down the knuckles, four fingers as lit bands with dark separators, thumb hanging to a point with a lit core "
                      "and a dark left edge; green cuff lit at its top",
                      "redo: every red tone placed where the dump has it (three reds), the cuff shaded top to bottom", ""),
    "employee-resume": ("done", "tan bubble; purple figure: hat shaded lit-left to dark-right (crown + brim), head, lit shoulders, "
                        "torso, left arm bent to the hip with the tan showing through the loop, lit forearm, two legs (left one lit at "
                        "the foot, right one turning in), ground line; clipboard: white board, olive edge with a black left side, "
                        "three dark text lines with grey bullets, two grey lines, cyan clip dome (light upper left, deeper cyan) on a "
                        "dark teal base",
                        "redo: figure drawn with volume and its arm loop, hat and clip shaded, the bullets added", ""),
    "employee-thumbs-up": ("done", "tan bubble with its rim made concentric (1.5 px) and the cast shadow refitted as a soft layer "
                           "on the lavender square; green thumbs-up: thumb with a dark left edge and a lit streak inside its right "
                           "edge, fist with light patches on the back, four fingers (lit bands, light bands, a dark separator), the "
                           "thumb's crease, dark left and bottom edges; red cuff lit in the middle",
                           "redo: round 1's idle rim was fitted off-centre and painted the shadow's dark inner part as solid rim "
                           "(now a real soft shadow); hand drawn with every green tone", ""),
    "golfer-trophy-button": ("done", "tan bubble, rim concentric 1.4 px, cast shadow refitted on the disc; cup: rim bar, straight "
                             "walls shaded across (sampled per column), a white streak, dark bottom arc, narrow stem with a knot, flat "
                             "two-row foot (dark top, lit bottom shaded across), two ring handles mirrored about the axis",
                             "redo: round 1 drew a vase; the art is a straight-walled cup on a stem with a knot and a flat foot", ""),
    "golfer-club-button": ("done", "tan bubble, rim concentric 1.4 px, cast shadow refitted; club: black grip with a lighter top, "
                           "shaft one dark pixel with a lit line on its right, blade head at the foot, ball white top / dark half",
                           "fix 'a little bit sharper': hard-edged parts at the art's pixel widths, two-tone shaft, thinner rim", ""),
    "golfer-crossed-button": ("done", "tan bubble, rim concentric 1.4 px; two clubs mirrored about x 15.5: one-pixel dark shafts, "
                              "small blade heads with a light spot, dark ferrules, black grips",
                              "fix 'a little bit sharper': shafts drawn one pixel dark (the paler pixels beside them are antialiasing), "
                              "crisp heads, grips and ferrules, thinner rim", ""),
}


def main():
    extra = {}
    try:
        import results2_more as M
        extra = M.ROWS
    except ImportError:
        pass
    allrows = dict(ROWS)
    allrows.update(extra)
    if os.path.exists(C.RESULTS):
        os.remove(C.RESULTS)
    for k, (st, how, ch, un) in allrows.items():
        C.record(k, st, how, ch, un)
    print(len(allrows), "rows ->", C.RESULTS)


if __name__ == "__main__":
    main()
