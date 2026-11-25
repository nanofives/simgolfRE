"""Fix round results: log/roadmap/P41/work2/hud_palettes/results.tsv (id, status, how, changed, unsure) for this area's fix /
redo elements. how comes from each element's json; changed and unsure are written here.  py -3.12 results2.py"""
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hp  # noqa: E402

CHANGED = {
    "amenities-bridge-tab": ("better symmetry", "piers, deck, arch and posts are now one outline mirrored about x 21.3 (round 1's right pier was 2 px "
                             "wider and its posts off-axis); a dark dot the body paint left at the bubble's centre is gone", ""),
    "amenities-flowers-tab": ("root of the flowers weird still", "the three stems are round-ended smooth lines that gather into one knot and run on as a "
                              "single short stem to the root at (23.9, 34.3), instead of three thin lines meeting in a point; leaves "
                              "re-read (the right one hangs down, not up)", "the art's stems are a few noisy pixels; the knot-and-tail is my reading of them"),
    "amenities-obelisk-tab": ("better symmetry", "one profile mirrored about the shaft's axis x 21.55: round 1's point sat 1 px left of the shaft; the "
                              "pyramidion's sides now bow out a little as in the art", ""),
    "amenities-palette": ("beware of which elements are rendered over the textures", "the recess's curved left side read again (it now stays right of x 371..425) so it no "
                          "longer paints over the statue thumbnail; the kept-art region now holds all three thumbnails whole (round 1 "
                          "cut the statue tile at x 372) and keeps clear of the tabs and the bulldozer; the amenity tabs are this "
                          "round's", "the kept region also holds the bit of wall between the thumbnail tiles (it is part of that picture)"),
    "amenities-slot": ("redo", "new reading: a tile sunk in a diamond socket (not a box): socket gradient, the tile with rounded corners and "
                       "a 1 px navy outline in its measured bevel, the tile's shadow on the socket as its own soft layer",
                       "the selected state's light halo round the tile is not drawn"),
    "amenities-strip": ("redo", "new reading: faint iso cubes (top, lit left face, shaded right face, soft edges) on the panel; the "
                        "scallops and the cap as a 2 px navy line drawn as one smooth path with the panel darkening toward it; the rail "
                        "bending down along the edge; the floor and its rail beyond the edge only where the cut shows them",
                        "start's cubes are wedges read on their own (the strip turns away there)"),
    "building-palette": ("redo", "the slots are this round's boxes, their back rim left out so the wall and the dark back panel show behind "
                         "the tiles; the panel's paint is measured on its own pixels (round 1 measured it over the slots and it came "
                         "out light); the boxes' faces run on to the tray's bottom", ""),
    "building-slot": ("redo", "new reading: an iso box: tile with rounded corners (r 2.5) and a navy outline in a measured bevel, a back "
                      "rim behind its upper edges, the lit left and shaded right faces with measured gradients; no staircase "
                      "ground, no pink and green blend", ""),
    "elevation-area": ("render actual shadows of the elements", "shadows are layers now: the cylinder's on the wall, the arrows' on the wall and on "
                       "the top's rim, the top lip's on the cylinder, each offset / softness / strength fitted; the kept picture is "
                       "only the picture (round 1 pasted the arrows' shadow with straight edges); the kept mound widened to the "
                       "whole wide mound", "the arrows' wall shadow fit sits at the edge of its search box (dy 10)"),
    "elevation-point": ("render actual shadows of the elements", "as elevation-area (shadows as fitted layers; kept picture only the arrows and the mound)", ""),
    "elevation-smooth": ("render actual shadows of the elements", "as elevation-area; the casters are the ball and its three trails (authored bands with heads)", ""),
    "elevation-square": ("render actual shadows of the elements", "as elevation-area; the kept mound widened to the magenta-topped mound", ""),
    "elevation-up-down-tab": ("symettry", "every part mirrored about the bubble's axis x 17.45 (round 1's arrows sat 0.85 px left of the line's "
                              "centre); heads the same size up and down", ""),
    "hud-golfers-mode": ("redo", "new reading of the two busts: front golfer with a cap (dome crown with a button, visor), face profile "
                         "(nose, lips, chin), neck and body, a hand raised to the chin as a curled fist with its forearm; back golfer "
                         "bigger, a round head with a bun of hair behind, its face profile drawn as the light line between them", ""),
    "hud-mode-base": ("redo", "the fan's authored outline instead of the cut's staircase, its surface shaded by distance from the groove "
                      "with a lit rim; the stems under the course and buildings bubbles with their soft shadows; the thin cables "
                      "under info, pause and tools and from zoom in to rotate right; the mode bubbles composed without the ragged "
                      "ground patches their cuts carry", "the surface's radial shading shows faint concentric bands the art does not quite have"),
    "hud-rotate-left": ("center it", "the arrow's box is centred on the navy disc (it sat 0.8 px left / 0.7 px up); the head re-read (tip at "
                        "6.35, symmetric about the bar); paints measured where the art's arrow is and moved with it", ""),
    "hud-rotate-right": ("center it", "centred on the disc (the art's sits 1.6 px right); the mirror image of the left arrow; the disc circle "
                         "is the same on both buttons (round 1's fit put this one 0.6 px off)", ""),
    "tab-elevation": ("symettry", "every part mirrored about the bubble's axis x 17.45; heads the same size up and down", ""),
    "tab-shovel": ("better connected lines", "the grip is one closed loop; the shaft starts at the loop's lower corner with a round end and "
                   "runs into the blade; the blade's top is square to the shaft", ""),
}


def main():
    rows = []
    for el in sorted(hp.FIX_IDS):
        p = os.path.join(hp.OUT, el + ".json")
        png = os.path.join(hp.OUT, el + ".png")
        if not (os.path.exists(p) and os.path.exists(png)):
            rows.append([el, "blocked", "", "not built", ""])
            continue
        how = json.load(open(p)).get("how", "")
        note, changed, unsure = CHANGED[el]
        rows.append([el, "done", how.replace("\t", " "), f"note '{note}': {changed}", unsure])
    with open(os.path.join(hp.OUT, "results.tsv"), "w") as f:
        f.write("id\tstatus\thow\tchanged\tunsure\n")
        for r in rows:
            f.write("\t".join(r) + "\n")
    print(os.path.join(hp.OUT, "results.tsv"), len(rows), "rows")


if __name__ == "__main__":
    main()
