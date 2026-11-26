"""P41 round 6, reports_slabs item 3: the Course Report's "Scenic Hole" legend icon.

What the port showed: a small flag (the slab scene's foot carried two traced wires, parts line7 / line8, with their cast
shadows) where the art has the scenic mark. The slab's foot is drawn over the legend, so the legend's own icon never shows.

Reading of the art (coursereport.pcx, lum grid x 422..437, y 474..490 in _read/scenic_grid.txt): a mark mirrored about
x 429.5 (pixel column 429): two top leaves (y 476..478, pointing in at y 477), thin stalks at x 426 / 432 (y 479..481), a
centre diamond (x 428..430, y 479..481), two blocks on a one-row bar (y 482..484, bar x 424.5..434.5 at y 483), a stem at
x 429 (y 484..488) and two slanted feet (y 486..488) joined to the stem at y 488. No cast shadow (the pixels around it are the
plain slab, lum 165).

The approved legend scene (round 2, part legend_bug) has exactly these parts, but 1 px to the right (its axis at x 430.5).
So: the legend's icon moved 1 px left, put into the slab scene in place of the flag; the legend scene gets the same move.
"""
import json

import fix6_common as F

DX = -1.0


def _shift(S, dx):
    S = F.deep(S)

    def walk(o):
        if isinstance(o, dict):
            t = o.get("t")
            if t == "poly":
                o["rings"] = [[[x + dx, y] for x, y in r] for r in o["rings"]]
            elif t == "seg":
                o["a"] = [o["a"][0] + dx, o["a"][1]]
                o["b"] = [o["b"][0] + dx, o["b"][1]]
            elif t in ("rrect", "circle", "ellipse"):
                o["c"] = [o["c"][0] + dx, o["c"][1]]
            elif t in ("union", "inter"):
                for q in o["of"]:
                    walk(q)
            else:
                raise ValueError(t)
    walk(S)
    return S


def icon_item(legend_scene):
    it = [q for q in legend_scene if q.get("part") == "legend_bug"]
    assert len(it) == 1
    it = F.deep(it[0])
    it["layers"][0]["shape"] = _shift(it["layers"][0]["shape"], DX)
    it["box"] = [it["box"][0] + DX, it["box"][1], it["box"][2] + DX, it["box"][3]]
    it["part"] = "legend_scenic"
    return it


def apply_slab(doc, legend_scene):
    """replace the flag (parts line7 / line8 and their two shadows) with the scenic icon"""
    sc = doc["scene"]
    flag = [i for i, q in enumerate(sc) if q.get("part") in ("line7", "line8")]
    assert len(flag) == 2, flag
    flag_pts = [json.dumps(sc[i]["layers"][0]["shape"]["pts"]) for i in flag]
    shadows = [i for i, q in enumerate(sc) if q["layers"][0]["shape"]["t"] == "move"
               and q["layers"][0]["shape"]["of"].get("pts") is not None
               and json.dumps(q["layers"][0]["shape"]["of"]["pts"]) in flag_pts]
    assert len(shadows) == 2, shadows
    out = []
    for i, q in enumerate(sc):
        if i in shadows:
            continue
        if i == flag[0]:
            out.append(icon_item(legend_scene))
            continue
        if i == flag[1]:
            continue
        out.append(q)
    doc["scene"] = out
    doc["round6_scenic"] = "the flag (line7 / line8 and shadows) replaced by the approved legend's scenic icon, 1 px left"
    return doc


def legend_doc():
    doc = F.approved("coursereport-legend")
    sc = doc["scene"]
    k = [i for i, q in enumerate(sc) if q.get("part") == "legend_bug"]
    assert len(k) == 1
    new = icon_item(sc)
    new["part"] = "legend_bug"
    sc[k[0]] = new
    doc["round6"] = "scenic icon moved 1 px left onto the art's axis (x 429.5)"
    return doc
