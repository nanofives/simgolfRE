"""The amenities palette's five category tabs: a blue glass bubble on the palette's lavender corner (raised with a cast shadow
when idle, sunk into a round lit recess when selected, yellow), with a navy glyph.

Ground read on the cuts (51 x 47, the top right 8 x 24 transparent): an upper dithered lavender wall (y 0..24, x 0..43), a
light ledge line (y 24..27), the lower lavender floor (y 27..47); the selected cut also carries a dark line along the top
(y 1..3) and the recess. Glyphs read with cov.py and 9-18x enlargements (2026-11-24); local = sheet - (300, 50 * row) on
the idle cut (the selected cut sits 100 px to the right).
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from geom import rect, line, poly, circle, oval, arc, U  # noqa: E402

GROUND = [("upper wall: dithered lavender", rect(0, 0, 43, 24)),
          ("ledge: a light line", rect(0, 24, 51, 27)),
          ("floor: lavender", rect(0, 27, 51, 47))]
GROUND_SEL = GROUND + [("a dark line along the top", rect(0, 1, 43, 3)),
                       ("recess: a round well round the sunk bubble, lit as measured", circle(0, 0, 1), "well:3.5")]


def tab(id_, how, glyph):
    return dict(id=id_, ref="idle", circle=(21.7, 22.6, 18.0),
                circle_fixed={"idle": (21.7, 22.6, 18.0), "selected": (21.7, 23.6, 18.0)},     # rim profiles, row 22 / col 21
                ground=GROUND, ground_by_state={"selected": GROUND_SEL},
                shadow=True, shadow_states=["idle"], how="blue glass bubble on the lavender corner (idle raised with its shadow, selected yellow "
                                 "and sunk in a lit recess); glyph: " + how, glyph=glyph)


def sym(ax, half):
    """A polygon mirrored about x = ax from its right half [(dx, y), ...] listed top to bottom (dx 0 = on the axis)."""
    right = [(ax + dx, y) for dx, y in half]
    left = [(ax - dx, y) for dx, y in reversed(half) if dx > 0]
    return poly(*(right + left))


BR_AX = 21.3     # bridge axis: piers 308..313.5 / 328.5..334, posts 309..312 / 331..334, crown post 320..322.9 (sheet, idle cut)
OB_AX = 21.55    # obelisk axis: tip column 321, shaft 317.9..325.3 at row 120, 317.2..326.0 at row 133 (sheet, idle cut)
ROSES = [(20.8, 13.0, 3.9), (28.9, 16.3, 3.9), (12.4, 19.3, 3.9), (21.6, 21.9, 4.2)]
SPECS = [
    tab("amenities-bench-tab",
        "a park bench seen from the front: a backrest of two 3 px slats between two short end posts, a thin arm bar "
        "outside each post, a 3 px seat and legs that curve out into an arch under it",
        [("left arm bar", rect(9.0, 14.9, 10.0, 22.0), "navy"),
         ("right arm bar", rect(33.0, 14.9, 34.0, 22.0), "navy"),
         ("left post", rect(11.0, 14.0, 13.0, 23.0), "navy"),
         ("right post", rect(30.4, 14.0, 32.6, 23.0), "navy"),
         ("backrest: upper slat", rect(11.0, 15.0, 32.0, 18.0), "navy"),
         ("backrest: lower slat", rect(11.0, 19.0, 32.0, 22.0), "navy"),
         ("seat and legs: an arch under a flat seat", poly((10, 23), (33, 23), (33, 30.8), (31.2, 30.8), (30.6, 28.4), (29.0, 26.4),
                                                           (14.0, 26.4), (12.4, 28.4), (11.8, 30.8), (10, 30.8)), "navy")]),
    tab("amenities-bridge-tab",
        "an arched footbridge, mirrored about x 21.3 (fix round: symmetric): two square piers, a deck that rises from the "
        "pier tops to a flat crown over a round arch, and three railing posts on top (one over each pier, one at the crown)",
        [("piers and deck: one outline mirrored about the axis (flat pier tops, the deck rising to the crown, the arch underneath)",
          sym(BR_AX, [(1.0, 18.2), (3.0, 18.25), (5.5, 18.6), (7.8, 19.5), (9.6, 20.6), (12.7, 20.6), (13.0, 21.0), (13.0, 28.0),
                      (7.4, 28.0), (7.4, 24.0), (6.6, 22.9), (5.5, 22.2), (3.5, 21.6), (1.2, 21.42)]), "navy"),
         ("railing post over the left pier", rect(BR_AX - 13.1, 17.1, BR_AX - 10.1, 20.8), "navy"),
         ("railing post at the crown", rect(BR_AX - 1.5, 16.9, BR_AX + 1.5, 18.6), "navy"),
         ("railing post over the right pier", rect(BR_AX + 10.1, 17.1, BR_AX + 13.1, 20.8), "navy")]),
    tab("amenities-obelisk-tab",
        "an obelisk, mirrored about x 21.55 (fix round: the point now sits on the shaft's axis): a tall shaft tapering from "
        "8.8 px at its foot to 7 px under the cap, capped by a pyramid point whose sides bow out a little",
        [("obelisk: tapering shaft and pyramidion, one profile mirrored", sym(OB_AX, [(0.12, 8.9), (1.0, 10.5), (2.0, 12.5), (2.85, 14.5),
                                                                              (3.4, 15.8), (3.5, 16.5), (4.4, 35.0)]), "navy")]),
    tab("amenities-flowers-tab",
        "a bouquet of four roses: four navy round heads (each with a light curl inside) bunched at the top, a leaf at each "
        "side, and three stems drawn as smooth round-ended lines that gather into one knot and run on as a single short "
        "stem down to the root (fix round: one tied bundle instead of three lines meeting in a point)",
        [(f"rose {i + 1}: a round head", circle(x, y, r), "navy") for i, (x, y, r) in enumerate(ROSES)] +
        [(f"rose {i + 1}: the light curl inside", arc(x + 0.2, y + 0.1, 0.33 * r, 200.0, 200.0, 0.9), "hole")
         for i, (x, y, r) in enumerate(ROSES)] +
        [("left leaf", oval(13.2, 25.5, 2.4, 1.05, 22.0), "navy"),
         ("right leaf", oval(29.8, 25.6, 2.2, 1.0, 68.0), "navy"),
         ("left stem: down from the left rose, bending right into the knot", line((15.7, 22.6), (15.8, 27.4), (17.6, 29.4), (20.4, 30.2),
                                                                              (22.4, 30.7), w=1.3, cap="round", smooth=2), "navy"),
         ("middle stem: down from the front rose into the knot", line((21.6, 25.6), (22.6, 28.0), (22.6, 30.7), w=1.3, cap="round", smooth=2), "navy"),
         ("right stem: from the right leaf down-left into the knot", line((29.0, 24.0), (26.4, 26.6), (24.0, 28.6), (22.8, 30.6), w=1.3,
                                                                          cap="round", smooth=2), "navy"),
         ("the knot where the stems gather", oval(22.6, 30.7, 1.2, 0.95, 0.0), "navy"),
         ("one stem below the knot down to the root", line((22.6, 30.7), (23.3, 32.4), (23.9, 34.3), w=1.4, cap="round", smooth=1), "navy")]),
    tab("amenities-tree-tab",
        "a broad tree: a wide umbrella canopy whose ends droop, three limbs (left, middle, right) coming down from it "
        "with light gaps between them and joining into one trunk, and roots: a line along the ground with three short "
        "roots going down",
        [("canopy: a wide umbrella, flat on top, its sides sloping out to square drooping ends", poly((16.0, 10.6), (21.6, 10.6), (25.8, 11.8), (29.4, 14.8), (31.9, 16.0),
                                                         (31.9, 17.9), (28.6, 18.9), (25.0, 18.0), (16.6, 18.0), (14.6, 19.0),
                                                         (11.0, 18.0), (11.0, 16.0), (13.0, 14.8), (13.6, 12.2)), "navy"),
         ("left limb", line((15.0, 18.4), (17.0, 20.6), (19.5, 22.8), w=2.0), "navy"),
         ("middle limb", line((20.6, 17.0), (20.8, 21.4), w=2.0), "navy"),
         ("right limb", line((26.8, 18.8), (25.6, 20.8), (22.0, 21.6), w=2.0), "navy"),
         ("limbs joining", poly((18.2, 22.0), (25.6, 21.0), (25.6, 24.0), (22.6, 24.6)), "navy"),
         ("trunk", rect(22.6, 23.5, 25.5, 31.0), "navy"),
         ("roots: a line along the ground", line((16.2, 31.3), (30.6, 31.3), w=1.2), "navy"),
         ("left root", line((16.5, 31.3), (16.5, 33.9), w=1.0), "navy"),
         ("middle root", line((23.0, 31.5), (22.7, 34.6), w=1.1), "navy"),
         ("right root", line((28.6, 31.3), (29.2, 33.6), w=1.0), "navy")]),
]

if __name__ == "__main__":
    import bel
    want = sys.argv[1:]
    for sp in SPECS:
        if not want or sp["id"] in want:
            bel.build(sp)
