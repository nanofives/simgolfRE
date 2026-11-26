"""P41 round 6, reports_slabs item 4: the legend's connector stubs in the Course Report's footer.

The game draws the slab's footer over the legend, and the slab scene had no stubs, so none of the three legend entries
showed the short line between its tile and its pill. The approved legend scene has them (parts legend_stub128 / 286 / 435:
a 2 px round-ended segment at y 484, fill 92, 98, 141, the pill outline's colour).

Reading of the art (coursereport.pcx, lum at y 483..484): Top 100 stub x 128..131, Top 18 x 286..289, each from the tile's
dark border pixel (x 127 / 285) into the pill's outline (x 132..133 / 290..291); Scenic Hole x 438..441 (x 438 partial),
after a plain gap x 435..437 right of the icon (its right edge x 434). The legend's first two segments match; its scenic
one starts at x 435.1, 3 px early, so here it starts at 439.0 (its round end reaches 438). Drawn just before the pills, so
each pill's outline covers the stub's right end, as in the legend.
"""
import fix6_common as F

SCENIC_A = 439.0
# the slab's approved tiles end at x 126 / 284 (the art's tile border is x 127 / 285): the slab's stubs start at 127 / 285 so
# their round ends meet the tile (the legend's 127.7 / 285.7 leave a 0.7 px gap against these tiles)
TILE_A = {"legend_stub128": 127.0, "legend_stub286": 285.0}


def stub_items(legend_scene):
    out = []
    for part in ("legend_stub128", "legend_stub286", "legend_stub435"):
        it = [q for q in legend_scene if q.get("part") == part]
        assert len(it) == 1, part
        it = F.deep(it[0])
        if part in TILE_A:
            S = it["layers"][0]["shape"]
            S["a"] = [TILE_A[part], S["a"][1]]
            it["box"][0] = TILE_A[part] - 5.0
        if part == "legend_stub435":
            S = it["layers"][0]["shape"]
            assert abs(S["a"][0] - 435.1) < 0.01
            S["a"] = [SCENIC_A, S["a"][1]]
            it["box"][0] = SCENIC_A - 5.0
        out.append(it)
    return out


def apply_slab(doc, legend_scene):
    sc = doc["scene"]
    k = [i for i, q in enumerate(sc) if q.get("part") == "pill_ring13"]
    assert len(k) == 1
    assert not any(str(q.get("part", "")).startswith("legend_stub") for q in sc)
    doc["scene"] = sc[:k[0]] + stub_items(legend_scene) + sc[k[0]:]
    doc["round6_stubs"] = "the legend's three connector stubs, before the pills; the scenic one from x 439 (art x 438), the other two from the tiles' ends"
    return doc


def fix_legend(doc):
    """the legend scene's own scenic stub, started where the art's starts"""
    for q in doc["scene"]:
        if q.get("part") == "legend_stub435":
            S = q["layers"][0]["shape"]
            S["a"] = [SCENIC_A, S["a"][1]]
            q["box"][0] = SCENIC_A - 5.0
    doc["round6"] += "; scenic stub from x 439 (art x 438), was 435.1"
    return doc
