"""S3.11: per-row px of the resubmission census (11_census.tsv) against the committed census run (log/roadmap/merges/I6/census.tsv):
rows better / same / worse in total_px and beyond_px, the worst rows, and per region where a row got worse.
    ~/bin/py -3.12 log/roadmap/S3/11_census_px.py > log/roadmap/S3/11_census_px.log"""
import csv


def rd(p):
    lines = [l for l in open(p, encoding="utf-8") if not l.startswith("#")]
    return {(r["scenario"], r["checkpoint"]): r for r in csv.DictReader(lines, delimiter="\t")}


old, new = rd("log/roadmap/merges/I6/census.tsv"), rd("log/roadmap/S3/11_census.tsv")
cnt = {"better": 0, "same": 0, "worse": 0, "unjudged": 0}
worse = []
for k, n in new.items():
    o = old.get(k)
    try:
        a, b = int(o["total_px"]), int(n["total_px"])
    except (TypeError, ValueError, KeyError):
        cnt["unjudged"] += 1
        continue
    c = "better" if b < a else "same" if b == a else "worse"
    cnt[c] += 1
    if c == "worse":
        worse.append((b - a, k, a, b, o["regions"], n["regions"]))
print("rows", len(new), cnt)
tot_o = sum(int(old[k]["total_px"]) for k in new if k in old and old[k]["total_px"].isdigit() and new[k]["total_px"].isdigit())
tot_n = sum(int(new[k]["total_px"]) for k in new if k in old and old[k]["total_px"].isdigit() and new[k]["total_px"].isdigit())
print("total px over rows judged in both:", tot_o, "->", tot_n)
for d, k, a, b, ro, rn in sorted(worse, reverse=True)[:40]:
    reg_o = {x.split("=")[0]: int(x.split("=")[1].split("/")[0]) for x in ro.split(";") if "=" in x}
    reg_n = {x.split("=")[0]: int(x.split("=")[1].split("/")[0]) for x in rn.split(";") if "=" in x}
    up = {r: (reg_o.get(r, 0), v) for r, v in reg_n.items() if v > reg_o.get(r, 0)}
    print(f"+{d:7d} {k[0]}/{k[1]}: {a} -> {b}  regions up: {up}  status {old[k]['status']} -> {new[k]['status']}")
