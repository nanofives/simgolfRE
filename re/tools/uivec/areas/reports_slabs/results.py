"""Merge the per-sheet result rows (results/<key>.json, written by build.emit) with the hand notes below into
log/roadmap/P41/work/reports_slabs/results.tsv (one row per element of the area)."""
import json
import os

import common as C

# what each kind of element was read as (the parts written once in parts.py / build.py and instanced)
HOW = {
    "report slab": "slab: rpoly (family outline: quarter-ellipse shoulder 46x81, title tab, step, round foot; corners fitted on "
                   "alpha) minus its sunken windows, measured bevel; on it the plate, pills, wires, cells of the sheet",
    "title plate": "sunken cream plate: quadrilateral whose top is a circular arch (sagitta from the top line fitted along x), "
                   "corners r7 / r2, navy line widths read per side, inner shade = fitted bevel",
    "pill": "sunken capsules / ovals / boxes from their line centres and read line widths (navy line + bevel inner); wires as "
            "1.6-1.8 px navy strokes with filleted elbows; soft ink shadow below-right (fitted offset / strength)",
    "table": "cells as rounded rectangles read on the grid (pale ones are windows in the slab, its bevel shades the pillars), "
             "navy rules as straight strokes (line centre + width read)",
    "small mark": "legend: flat squares / dots as cells or ovals, empty ring pills, short stubs as strokes",
    "bubble button": "socket disc + toolkit bubble recipe (rim, body bevel and specular fitted on this art per state) + the "
                     "check as one 7-point polygon read on the grid",
    "gradient bar": "one screen-size rectangle, flat black (the sheet is uniformly 0,0,0); the 25 % blend is the game's",
}

NOTES = {      # id -> (status override, how override, unsure)
    "holestats-slab": (None, None, "foot tab's bottom-left corner is a round quarter; the art's is a little flatter"),
    "holestats-pills": (None, None, "lines slightly crisper than the art's; one oval's art-side edge read is off (the drawn oval matches its neighbours)"),
    "coursereport-slab": (None, None, "the dark band under the head's column pills (top wall of the table area) is not drawn; slab bevel shows a faint seam rectangle in the foot; head pills' shared ')(' joins drawn as touching capsules"),
    "coursereport-cell-bands": (None, None, "third (dark green) band scores badly (mad ~34): check its colour / extent"),
    "coursereport-legend": (None, None, "legend squares drawn flat (the art's have a shaded bottom row); the stub from square to ring pill is under the line reader's 6 px minimum and is missing"),
    "endofyear-slab": (None, None, "cream band is an explicit window (203..600, 211..376) as in Hole Stats"),
    "lowscore-slab": (None, None, "foot piece scores 14 (mad): look at the foot tab and the cream row's ends"),
    "playcomt-slab": (None, None, "foot piece scores 15 (mad): as Low Score"),
    "histograph-slab": (None, None, "trunk wires: the rounded elbows into the top and bottom pills are drawn as straight ends; legend stubs missing (short)"),
    "histograph-legend": (None, None, "dot-to-pill stubs missing (under 6 px)"),
    "sga-slab": (None, None, "badge kept as art (pasted inside r 62 about 120, 339); the foot OK bubble (bubble recipe on this sheet's cut) renders too dark: check"),
    "tournament-slab": (None, None, "some cream capsules cut by a row piece read as square boxes (rows a and c), the small end ovals likewise; foot shows a faint bevel seam; two end bands via a second outline"),
    "finance-table": (None, None, ""),
    "shortcuts-pills": (None, None, ""),
}


def main():
    rows = []
    d = os.path.join(C.HERE, "results")
    for fn in sorted(os.listdir(d)):
        rows += json.load(open(os.path.join(d, fn)))
    import build
    kinds = {}
    for sheet in sorted({s["sheet"] for s in __import__("sheets").SHEETS.values()} | {"OkStates.pcx", "black.pcx"}):
        try:
            for eid, info in build.roster_crops(sheet).items():
                kinds[eid] = info["kind"]
        except Exception:
            pass
    seen = {r["id"] for r in rows}
    extra = json.load(open(os.path.join(d, "..", "extra_results.json"))) if os.path.exists(os.path.join(d, "..", "extra_results.json")) else []
    rows = [r for r in rows if r["id"] not in {e["id"] for e in extra}] + extra
    out = os.path.join(C.OUT, "results.tsv")
    with open(out, "w", encoding="utf-8") as f:
        f.write("id\tstatus\thow\tinterior_mad\tedge_px\tunsure\n")
        for r in sorted(rows, key=lambda r: r["id"]):
            st, how, uns = NOTES.get(r["id"], (None, None, None))
            how = how or r.get("how") or HOW.get(kinds.get(r["id"], ""), "")
            f.write(f"{r['id']}\t{st or r['status']}\t{how}\t{r.get('interior_mad', '')}\t{r.get('edge_px', '')}\t"
                    f"{uns or r.get('unsure', '')}\n")
    print(out, len(rows))


if __name__ == "__main__":
    main()
