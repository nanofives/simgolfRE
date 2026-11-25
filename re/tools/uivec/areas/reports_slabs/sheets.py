"""Hand-read geometry of each report sheet (frame pixels of the 800 x 600 sheet; edges read with edges.py / linecentre.py /
charmap.py, see the notes per sheet). Corner sizes are refined by corners.py into geom/<key>.json.

outlines: name -> vertex ring, clockwise on screen, [x, y, rx, ry] (rx back along the incoming edge, ry along the outgoing).
rows: the sheet's horizontal pieces (the game stacks them; between them the sheet is empty or holds guide marks).
"""
ROUND_EQUAL = {}

SHEETS = {
    "holestats": {
        "sheet": "HoleSTAT.pcx",
        "rows": [(0, 220), (282, 298), (347, 402)],
        "outlines": {
            "slab": [
                [161, 33, 52, 91],     # the shoulder: left edge x 161 up to y ~85, top edge y 33 from x ~252
                [570, 33, 4, 4],       # title tab, top right
                [570, 70, 2, 2],       # step down (concave)
                [639, 70, 5, 5],
                [639, 402, 20, 20],    # foot tab, round bottom right
                [554, 402, 18, 18],
                [554, 373, 3, 3],      # foot meets the slab bottom (concave)
                [161, 373, 2, 2],
            ],
        },
        # the cream band (a sunken window through the head, the row pieces and the foot): x 178..620, y 214..349
        "holes": [[[178, 214, 1, 1], [620, 214, 1, 1], [620, 349, 1, 1], [178, 349, 1, 1]]],
        "plates": [[233, 42, 554, 70]],
        "plate_r": (7.0, 2.0),
        "not_pills": [[226, 36, 562, 76], [176, 212, 622, 350]],
        # wire centre lines read with linecentre.py: pill rows at y 89.4 + 21 k, trunk x 167.1, foot run y 367.35,
        # elbows r 9 (top) and 10 (bottom), right brackets centred x 431.45 r 10.5, oval bracket x 431.3 r 10
        "wires": [
            ["path", [[184, 89.4], [167.1, 89.4], [167.1, 367.35], [573.0, 367.35]], [9.0, 10.0]],
            ["path", [[167.1, 110.4], [184, 110.4]]],
            ["path", [[167.1, 131.4], [184, 131.4]]],
            ["path", [[167.1, 152.4], [184, 152.4]]],
            ["path", [[167.1, 208.05], [184, 208.05]]],
        ] + [["path", [[280, y], [299, y]]] for y in (89.4, 110.4, 131.4, 152.4)]
          + [["bracket", 431.45, y, y + 21.0] for y in (89.4, 131.4)]
          + [["path", [[483, y], [509, y]]] for y in (89.4, 131.4)]
          + [["path", [[533, y], [559, y]]] for y in (110.4, 152.4)]
          + [["path", [[444.5, 173.35], [614.5, 173.35]]]]
          + [["path", [[444.5 + 34 * k, 175.0], [444.5 + 34 * k, 192.0]]] for k in range(6)]
          + [["bracket", 431.3, 173.35, 193.35], ["path", [[404, 183.35], [421.3, 183.35]]]],
        "shadow_boxes": [[162, 80, 302, 140], [400, 160, 520, 206]],
        "details": {"holestats-slab": [("shoulder", (156, 28, 110, 64)), ("title tab", (530, 28, 114, 50)),
                                       ("foot", (520, 340, 124, 66))]},
    },
}

SHEETS["coursereport"] = {
    "sheet": "coursereport.pcx",
    "rows": [(20, 104), (129, 146), (169, 186), (205, 220), (241, 256), (275, 290), (420, 449), (468, 540), (545, 592)],
    # the slab exists only in these rows (the colour cell rows and the marks sit on the empty sheet)
    "slab_rows": [(20, 104), (129, 146), (169, 186), (420, 449), (468, 540)],
    "outlines": {"slab": [[0, 20, 46, 81], [544, 20, 5, 5], [544, 58, 7, 6], [799, 58, 5, 5], [799, 540, 19, 19],
                          [714, 540, 20, 18], [714, 513, 3, 3], [0, 513, 5, 5]]},
    "plates": [[117, 30, 529, 57]],
    "plate_r": (7.0, 2.0),
    "not_pills": [[108, 16, 540, 70]],
    "auto_cells": True,
    "cells_as_holes": True,
    # the legend's empty pills: a ring on the slab, their inside is the slab's own lilac
    "pill_colours": [[(148, 150, 198), "pill_ring"]],
    "auto_lines": True,
    "shadow_boxes": [[100, 66, 300, 104], [0, 129, 200, 146]],
    # round 2: the dark green band (59, 171, 59) is darker than the cell reader's default floor; the legend's raised
    # tiles, bug and stubs are authored in crmarks.py (the reader saw the tiles as flat cells)
    "cell_cols": [(59, 171, 59)],
    "cell_skip": [[112, 475, 130, 494], [270, 475, 288, 494], [420, 470, 438, 492]],
    "line_skip": [[420, 470, 445, 492], [126, 480, 136, 488], [284, 480, 294, 488]],
    "extras": ["crmarks.legend_items"],
    "external": {"coursereport-marks": "crmarks.py"},
    "details": {"coursereport-slab": [("shoulder", (0, 15, 110, 60)), ("title tab", (500, 15, 110, 55)),
                                      ("foot", (690, 498, 110, 45))]},
}


def fam(L, T, tab_r, step, R, B, foot_l, foot_b):
    """The family outline (read with runs.py on every sheet): shoulder quarter ellipse 46 x 81 top-left, title tab with
    5 px corners, a concave step down to the right part, the round foot tab bottom-right (r ~19 / 18)."""
    return [[L, T, 46, 81], [tab_r, T, 5, 5], [tab_r, step, 6, 4], [R, step, 5, 5], [R, foot_b, 19, 19],
            [foot_l, foot_b, 18, 18], [foot_l, B, 2, 2], [L, B, 5, 5]]


def simple(sheet, outline, rows, plate, slab_rows=None, **kw):
    d = {"sheet": sheet, "rows": rows, "outlines": {"slab": outline}, "plates": [plate], "plate_r": (7.0, 2.0),
         "not_pills": [[plate[0] - 10, plate[1] - 14, plate[2] + 10, plate[3] + 10]], "auto_cells": True,
         "cells_as_holes": True, "auto_lines": True}
    if slab_rows:
        d["slab_rows"] = slab_rows
    d.update(kw)
    x0, y0 = outline[0][0], outline[0][1]
    d.setdefault("shadow_boxes", [[plate[0] - 60, plate[3] + 4, plate[0] + 140, plate[3] + 60]])
    d.setdefault("details", {})
    return d


SHEETS["endofyear"] = simple("ENDoYEAR.pcx", fam(187, 40, 576, 78, 616, 400, 531, 429),
                             [(40, 216), (291, 306), (374, 429), (482, 534)], [250, 52, 561, 77],
                             slab_rows=[(40, 216), (291, 306), (374, 429)], pill_colours=[[(255, 248, 232), "pill_cream"]],
                             external={"endofyear-ok-hover": "ok.py"},
                             # the cream band window (head bottom 211, line band 291-306, foot top 374-376), as Hole Stats'
                             holes=[[[203, 211, 1, 1], [600, 211, 1, 1], [600, 376, 1, 1], [203, 376, 1, 1]]],
                             band_fill=(255, 248, 232), cell_skip=[[201, 209, 602, 378]],
                             ok_feet=[{"origin": (551, 381), "c": (573.2, 403.3), "r": 20.1}])
SHEETS["lowscore"] = simple("lowscore.pcx", fam(195, 45, 572, 83, 606, 353, 521, 382), [(45, 124), (224, 241), (321, 382)],
                            [270, 56, 557, 82], ok_feet=[{"origin": (544, 334), "c": (566.2, 356.3), "r": 20.1}],
                            cream_rows=[{"c": [215.1, 115.3, 594.1, 329.3], "r": 6.5, "w": 2.0, "line": [34, 44, 79],
                                         "fill": [255, 247, 231]}])
SHEETS["playcomt"] = simple("PlayComt.pcx", fam(148, 45, 569, 83, 653, 353, 568, 382), [(45, 125), (224, 241), (321, 382)],
                            [223, 56, 554, 82], ok_feet=[{"origin": (591, 334), "c": (613.2, 356.3), "r": 20.1}],
                            # the cream row: line centres read at x 400 / y 232 (PlayComt.pcx), corners r 6.5 (top right)
                            cream_rows=[{"c": [168.1, 115.3, 641.1, 329.3], "r": 6.5, "w": 2.0, "line": [34, 44, 79],
                                         "fill": [255, 247, 231]}])
SHEETS["hire"] = simple("hire.pcx", fam(183, 94, 533, 131, 617, 440, 532, 469), [(94, 469)], [234, 105, 521, 131])
SHEETS["histograph"] = simple("histograph.pcx", fam(30, 5, 596, 43, 769, 569, 684, 598), [(5, 598)], [168, 15, 580, 42],
                              paper={"box": [84, 62, 711, 523], "fill": (251, 247, 238),
                                     "v": (106, 32, 682, 5, (184, 185, 190), (219, 217, 226)), "h": (72, 50, 522, (201, 201, 202))},
                              cell_skip=[[82, 60, 713, 525], [100, 539, 120, 555], [258, 539, 276, 555], [414, 539, 430, 555], [570, 539, 586, 555]],
                              line_skip=[[82, 60, 713, 525]],
                              pill_colours=[[(148, 150, 198), "pill_ring"]],
                              dot_box={"half": 8.0, "r": 5.5, "w": 2.0, "col": (97, 103, 146)}, scale={"histograph-legend": 4},
                              # legend dots read on the grid: 12 x 12 fills centred on x 110 / 267 / 422 / 578, y 547
                              dots=[(110.0, 547.0, 6.0, (132, 3, 133)), (267.0, 547.0, 6.0, ((2, 2, 3), (255, 65, 66))),
                                    (422.0, 547.0, 6.0, (0, 255, 0)), (578.0, 547.0, 6.0, (80, 143, 168))])
SHEETS["shortcuts"] = simple("shortcuts.pcx", fam(23, 12, 481, 50, 776, 380, 691, 409), [(12, 409)], [108, 22, 466, 49])
SHEETS["finance"] = simple("FINANCEreport.pcx", fam(8, 7, 539, 45, 792, 267, 707, 296), [(7, 296)], [112, 17, 524, 44])
SHEETS["sga"] = simple("SGA.pcx", fam(35, 26, 595, 64, 765, 416, 680, 445), [(26, 445)], [167, 36, 579, 63],
                       kept_art={"sga-badge": "kept as art (Mariano's mark: art): the round SGA badge picture"},
                       cell_skip=[[50, 270, 190, 405], [690, 390, 750, 440]], line_skip=[[50, 270, 190, 405], [690, 390, 750, 440]],
                       keep_art_circles=[(120.0, 339.0, 62.0)],
                       ok_feet=[{"origin": (701, 397), "c": (723.2, 419.3), "r": 20.1}])
SHEETS["tournament"] = simple("tournament result.pcx", fam(2, 10, 533, 48, 799, 379, 714, 408),
                              [(10, 106), (125, 147), (180, 206), (224, 242), (274, 325), (357, 408)], [106, 20, 518, 47],
                              slab_rows=[(10, 106), (125, 147), (180, 206), (224, 242), (357, 408)],
                              extra_slabs=[{"verts": [[2, 250, 0, 0], [799, 250, 0, 0], [799, 325, 19, 19], [714, 325, 18, 18],
                                                      [714, 296, 2, 2], [2, 296, 5, 5]], "rows": [(274, 325)]}],
                              # round 2 (Mariano: "shape are not quite there"): the small pills are capsules (flat top
                              # between round ends, 29 x 19 / 30 x 20), not ellipses
                              ellipse_as_capsule=True,
                              # the cream capsules that rows a / c / the end band cut (line centres: top / bottom line read
                              # by the line reader, sides x 14.4 / 159.0 and 654.3 / 683.9 as row b's full ones)
                              pills_manual=[("pill_cream", "capsule", [14.4, 130.15, 159.0, 151.15]),
                                            ("pill_cream", "capsule", [654.3, 130.13, 683.9, 150.63]),
                                            ("pill_cream", "capsule", [14.4, 225.06, 159.0, 246.06]),
                                            ("pill_cream", "capsule", [654.3, 225.0, 683.9, 245.5]),
                                            ("pill_cream", "capsule", [14.4, 254.06, 159.0, 275.06]),
                                            ("pill_cream", "capsule", [654.3, 254.54, 683.9, 275.04])],
                              line_skip=[[11, 124, 166, 150], [649, 124, 690, 150], [11, 222, 166, 245], [649, 222, 690, 245],
                                         [11, 270, 166, 281], [649, 270, 690, 281]],
                              cell_skip=[[11, 124, 166, 150], [649, 124, 690, 150], [11, 222, 166, 245], [649, 222, 690, 245],
                                         [11, 270, 166, 281], [649, 270, 690, 281]],
                              # the trunk leaves the head's lilac capsule with a round elbow (r 6, read at 8..17 x 66..72);
                              # short joins between pills (under the line reader's 6 px): 154 -> 163, 649 -> 654, 684 -> 690
                              wires=[["path", [[17.5, 66.1], [8.08, 66.1], [8.08, 106.0]], [6.0]],
                                     ["path", [[153.5, 66.1], [163.3, 66.1]]], ["path", [[649.0, 66.1], [654.6, 66.1]]],
                                     ["path", [[683.6, 66.1], [690.6, 66.1]]]])
