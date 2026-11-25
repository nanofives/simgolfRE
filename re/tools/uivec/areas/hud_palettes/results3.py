"""Round 3 results: log/roadmap/P41/work3/hud_palettes/results.tsv (id, status, changed, unsure, how) for this area's four
round-2 fix / redo entries (hud-mode-base as two versions)."""
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hp  # noqa: E402

ROWS = [
    ("amenities-strip",
     "note 'One of the squares is not same dimensions and proportions': every cube on the four pieces is now one size (top "
     "rhombus 47.6 x 31, faces 11 tall, the next row 27.8 lower and half a step aside), only its place differs; round 2 drew "
     "start's cube as hand-read wedges with a top 16 wide for 15 tall, 8 px left of the art's; start now carries the same "
     "lattice cube where a tone fit on its cut puts it (top corner (45.25, 6.5)) and the cube below it to the right; end's "
     "single cube re-read (top corner at x -1.3, y 33.5, its shaded right face below); middle 2's cube past the scallop removed",
     ""),
    ("building-palette",
     "note 'Cutoff on the edges': the top-row boxes' faces stopped in a straight line at y 568 because each composed slot kept "
     "its cut's render box; they now run to the tray's bottom edge; each box has its dark right-edge line down to the bottom "
     "(measured 85 over the next face, 58 on the panel); the recess panel re-read (x 319..761 from y 520, r 4 corners) with its "
     "shadowed left wall, the dark line along its top and the lit lip down its right side; the tray's left outline no longer "
     "covers the green gap between the two tab rings, and the lower tab's outer light ring (r 22.5) and the light rim down the "
     "left outline are drawn",
     ""),
    ("hud-golfers-mode",
     "note 'People silhouette still low detail': both golfers redrawn from a row-by-row coverage reading (edges to a tenth of a "
     "pixel) as separate navy masses with the art's own gaps between them: the front golfer's cap (dome, button, back slope to "
     "a point, a wedge visor with the light gap under it), brow, nose, lips, jutting chin and jaw, thin neck, shoulder, the "
     "chest slanting forward, the back curving in; the hooked hand (inverted U with a hole open at the bottom, its thin leg "
     "running into the chest); the back golfer's forehead, nose, lips, chin, throat and chest, the bob flipping out behind the "
     "neck with the notch under it; the inner ring moved out to the art's radius (22.85, round 2 had 21.74). Coverage "
     "difference against the art over the glyph: 193 -> 97",
     ""),
    ("hud-mode-base--a",
     "redo, version A (faithful): the fan's surface in 2-degree wedges round the groove's centre, each with the radial profile "
     "measured on its own pixels: the light lip inside the groove, the navy groove, the ridge's dark underside and bright top, "
     "the necks' shadows, the light upper left and the dark right end; navy necks under the bubbles read row by row; the three "
     "cables as raised wires (round 2 drew them dark and 2..3 px left of the art)",
     "the wedges' paint is a measurement of the art's tones, so this version is close to the art and less clean than B"),
    ("hud-mode-base--b",
     "redo, version B (clean): one continuous ridge tube with one measured profile over an outer surface graded from the light "
     "upper left to the dark right end; clean navy necks with a faint lit right edge; each bubble's and each neck's cast shadow "
     "fitted as its own soft layer; the three raised cables; the dark line closing the right end",
     ""),
]


def main():
    out = os.path.join(hp.OUT, "results.tsv")
    with open(out, "w") as f:
        f.write("id\tstatus\tchanged\tunsure\thow\n")
        for el, changed, unsure in ROWS:
            js = os.path.join(hp.OUT, el + ".json")
            png = os.path.join(hp.OUT, el + ".png")
            ok = os.path.exists(js) and os.path.exists(png)
            how = json.load(open(js)).get("how", "") if ok else ""
            f.write("\t".join([el, "done" if ok else "blocked", changed, unsure, how]) + "\n")
    print(out)


if __name__ == "__main__":
    main()
