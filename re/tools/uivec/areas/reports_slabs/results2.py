"""Round 2 (fix round, 2026-11-24): log/roadmap/P41/work2/reports_slabs/results.tsv for the elements Mariano marked fix /
redo (log/roadmap/P41/judge/decisions_final.json). Columns: id, status, how, changed, unsure, interior_mad."""
import json
import os

import common as C

OK_FIX = ("the foot's yellow OK bubble is now drawn with the approved OK recipe (ok.py: socket, rim, body bevel, specular, "
          "the 7-point check) measured on this sheet's own foot bubble, under a crisp 2 px navy socket ring and a lilac "
          "groove; round 1 had read the bubble's rim as stray navy wires")
ROWS = {
    "coursereport-cell-bands": ("done", "cells as rounded rectangles read on the grid, three colour bands with see-through gaps",
                                "the dark green band (59 171 59) was dropped by the cell reader's darkness floor; it is "
                                "now read like the red and green bands, same cell layout", ""),
    "coursereport-legend": ("done", "raised tiles (2 px bevel, every side measured on the tile), the bug pictogram as mirrored "
                            "parts, 2 px stubs in the ring colour, empty ring pills",
                            "redone from a new reading: the orange and yellow squares are raised tiles (lit top / left, "
                            "shaded right / bottom), the navy mark is drawn (it was an L before), the stubs to the pills "
                            "are drawn",
                            "the bug's two light highlight pixels left of its head are not drawn"),
    "coursereport-marks": ("done", "three hole-shape arrows (straight, dogleg right, dogleg left: stem, 45 degree shaft, "
                           "right-angle head, lit up-facing strip) and the five column tiles (raised tiles, three carrying "
                           "the bug)",
                           "redone (round 1 drew nothing): read as dogleg arrows and raised tiles; the art panel now shows "
                           "the colour sheet with its colour key, since the alpha sheet hides these cut sprites",
                           "what the bug stands for is not known to me (drawn from its parts: mandibles, head, antennae, "
                           "front legs on a bar, stem, feet)"),
    "endofyear-slab": ("done", "slab rpoly + windows, pills, wires; OK bubble in the foot", OK_FIX +
                       "; the wires are now navy (32 42 75, the darkest third of their pixels) instead of grey", ""),
    "histograph-legend": ("done", "each legend dot in a rounded-square ring (16 x 16, r 5.5, 2 px), a 2 px stub to its "
                          "ring pill",
                          "the dots now sit in rounded boxes (they had navy circles) and each is joined to its pill by a "
                          "line", ""),
    "lowscore-slab": ("done", "slab rpoly + windows, pills, wires; one sunken cream row; OK bubble in the foot", OK_FIX +
                      "; also the big cream row is one rounded box (r 6.5) through head, line band and foot with its "
                      "measured inner shade (it was two square boxes), and the wires are navy", ""),
    "playcomt-slab": ("done", "slab rpoly + windows, pills, wires; one sunken cream row; OK bubble in the foot", OK_FIX +
                      "; also the big cream row is one rounded box (r 6.5) (round 1's two square boxes drew a pink / "
                      "green streak), and the wires are navy", ""),
    "sga-slab": ("done", "slab rpoly + windows, pills, striped table, kept badge picture; OK bubble in the foot", OK_FIX +
                 " (round 1 drew it on a lilac square with stray marks)", ""),
    "tournament-slab": ("done", "slab rpoly + two end bands, capsules, cells, wires with elbows",
                        "the small pills are capsules (flat top between round ends), not ellipses; the cream capsules "
                        "that the row pieces cut are capsules (they were square boxes and wire outlines); the trunk "
                        "leaves the head's capsule with a round elbow; the short joins between pills are drawn; the light "
                        "seam rectangle in the foot tabs is gone (deep slab paint made flat)",
                        "the foot tab's bottom shade is a little thinner than the art's"),
}


def main():
    mads = {}
    d = os.path.join(C.HERE, "results2")
    for fn in os.listdir(d):
        for r in json.load(open(os.path.join(d, fn))):
            mads[r["id"]] = r.get("interior_mad", "") if r.get("interior_mad") not in (0, 0.0) else ""
    out = os.path.join(C.OUT, "results.tsv")
    with open(out, "w", encoding="utf-8") as f:
        f.write("id\tstatus\thow\tchanged\tunsure\tinterior_mad\n")
        for k, (st, how, ch, un) in ROWS.items():
            f.write("\t".join([k, st, how, ch, un, str(mads.get(k, ""))]) + "\n")
    print(out)


if __name__ == "__main__":
    main()
