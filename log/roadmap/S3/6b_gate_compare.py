"""S3.6b: the HUD opaque gate of the rebased build (6b_hudgate.py output dirs) against S3.6's (6_gate, 6_gate2), row by row:
clean flag and bad opaque px per HUD region. Prints every row that differs and the totals.

    python log/roadmap/S3/6b_gate_compare.py <new hudgate.tsv>...
"""
import csv
import sys

HUD = ["hud_title", "hud_meters", "toolbar", "bottom_panel"]


def rd(paths):
    out = {}
    for p in paths:
        for r in csv.DictReader(open(p, encoding="utf-8"), delimiter="\t"):
            out[(r["scenario"], r["checkpoint"])] = r
    return out


old = rd(["log/roadmap/S3/6_gate/hudgate.tsv", "log/roadmap/S3/6_gate2/hudgate.tsv"])
new = rd(sys.argv[1:])
same = changed = 0
tot = {"old": dict.fromkeys(HUD, 0), "new": dict.fromkeys(HUD, 0)}
for k in sorted(new):
    n = new[k]
    o = old.get(k)
    for h in HUD:
        tot["new"][h] += int(n[f"{h}_bad"])
        if o:
            tot["old"][h] += int(o[f"{h}_bad"])
    if o is None:
        print("NEW ROW", k, n["clean"], {h: n[f"{h}_bad"] for h in HUD})
        continue
    d = {h: (int(o[f"{h}_bad"]), int(n[f"{h}_bad"])) for h in HUD if o[f"{h}_bad"] != n[f"{h}_bad"]}
    if o["clean"] != n["clean"] or d:
        changed += 1
        print("DIFF", k, "clean", o["clean"], "->", n["clean"], d, "| own now:", n["own_reasons"][:120])
    else:
        same += 1
print("rows:", len(new), "same as S3.6:", same, "changed:", changed, "missing from the new run:", len(set(old) - set(new)))
print("clean:", sum(int(r["clean"]) for r in new.values()), "of", len(new))
print("bad px S3.6 rows:", tot["old"], "rebased:", tot["new"])
