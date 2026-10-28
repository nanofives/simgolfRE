"""S3.6: attribute every residual bad-pixel group of the HUD gate (6_gate/sheet.tsv) to the step that owns its cause.

Each class below was established by looking at the crops (port f0 | reference | mask) of the rows named in 6_notes.md; a group
matches a class by its region and bounding box. Anything else is printed as UNATTRIBUTED.

    ~/bin/py -3.12 log/roadmap/S3/6_attrib.py log/roadmap/S3/6_gate
"""
import collections
import csv
import pathlib
import sys

HUD = ("hud_title", "hud_meters", "toolbar", "bottom_panel")
# (region, bbox) -> class
BOX = {
    ("bottom_panel", "361,499-384,532"): "S3.5 hole flag (addSprite, the sprite queue)",
    ("toolbar", "8,469-78,478"): "S3.8 radial-menu tip bar (drawPanelBackdrop's drawMapLine)",
    ("toolbar", "8,469-78,477"): "S3.8 radial-menu tip bar (drawPanelBackdrop's drawMapLine)",
    ("toolbar", "78,491-148,500"): "S3.8 radial-menu tip bar (drawPanelBackdrop's drawMapLine)",
    ("hud_title", "179,7-230,62"): "S2 golfer info panel portrait (drawGolferInfoPanel)",
    ("hud_title", "225,40-229,56"): "S2 golfer info panel portrait (drawGolferInfoPanel)",
    ("hud_title", "228,23-229,30"): "S2 golfer info panel portrait (drawGolferInfoPanel)",
    ("hud_meters", "649,30-675,48"): "S2 dialog frame over the meters (drawWindow)",
    ("hud_meters", "649,30-675,47"): "S2 dialog frame over the meters (drawWindow)",
    ("bottom_panel", "413,497-453,501"): "S3.5 building sprite over a walker label",
    ("hud_meters", "699,122-726,139"): "S3.8 terrain grid lines (Terrain::drawLine)",
    ("hud_meters", "699,122-728,139"): "S3.8 terrain grid lines (Terrain::drawLine)",
    ("hud_title", "159,2-231,81"): "S3.8 terrain grid lines and S3.5 a building over the title strip (cov_b_locations end)",
    ("toolbar", "185,487-207,574"): "S3.8 terrain grid lines (Terrain::drawLine)",
    ("bottom_panel", "208,496-253,580"): "S3.8 terrain grid lines (Terrain::drawLine)",
    ("bottom_panel", "208,496-253,574"): "S3.8 terrain grid lines (Terrain::drawLine)",
    ("hud_title", "0,7-143,69"): "modal frame: the original's back buffer holds no HUD there (terrain over the region)",
}
# cov_b_inject / cov_b_inject2: grid lines through the meters region, plus the event popup's right edge on zoom_box
TOP_RIGHT = "S3.8 terrain grid lines through the meters region (with the S2 event popup's edge on cov_b_inject zoom_box)"


def classify(scen, chk, reg, v, full, own_gap):
    n, bb = v.split("@")
    if full:
        return "S2 full-screen screen (results, report, character editor, golfer info)"
    if own_gap:
        return "S3.6 open: drawEmployeeViewPanel's golfer list (reported, row not clean)"
    if (reg, bb) in BOX:
        return BOX[(reg, bb)]
    x0, y0 = (int(t) for t in bb.split("-")[0].split(","))
    if reg == "bottom_panel" and y0 == 496 and x0 >= 400 and bb.split("-")[1].split(",")[1] in ("530", "536", "543"):
        return "S3.5 amenity items in the amenities palette (drawAmenityRow's addSprite)"
    if reg == "hud_meters" and x0 >= 640 and int(n) > 150:
        return TOP_RIGHT
    if reg == "bottom_panel" and (bb.endswith(",520") or bb.endswith(",556") or bb.endswith(",521")):
        return "S3.7 / S3.8 tool hover tip (tooltip bar under the tool label)"
    return None


def main():
    out = pathlib.Path(sys.argv[1])
    gate = {(r["scenario"], r["checkpoint"]): r for r in csv.DictReader(open(out / "hudgate.tsv", encoding="utf-8"), delimiter="\t")}
    tot = collections.Counter()
    rows = collections.Counter()
    left = []
    for r in csv.DictReader(open(out / "sheet.tsv", encoding="utf-8"), delimiter="\t"):
        vals = {k: r[k] for k in HUD}
        full = all(v != "0" and int(v.split("@")[0]) > 1000 for v in vals.values())
        own_gap = gate[(r["scenario"], r["checkpoint"])]["clean"] == "0"
        for k, v in vals.items():
            if v == "0":
                continue
            c = classify(r["scenario"], r["checkpoint"], k, v, full, own_gap)
            if c is None:
                left.append((r["scenario"], r["checkpoint"], k, v))
                continue
            tot[c] += int(v.split("@")[0])
            rows[c] += 1
    for c, n in tot.most_common():
        print(f"{n:8d} px  {rows[c]:4d} groups  {c}")
    for x in left:
        print("UNATTRIBUTED", *x)


if __name__ == "__main__":
    main()
