"""I7 (copy of I6's): compare the merged mirror's ranked census (frame_census.tsv) with I5's re/roadmap/frame_census.tsv (698 rows, PASS 82) and,
over S0's 609 rows, with S0's (git show c6d7d78b:re/roadmap/frame_census.tsv, 45 PASS). Prints status counts, transitions, PASS rows
lost / gained, rows new since I6, primary causes, and the section rows the merge requests name: cov_r_s2_screens (S2.7 / S2.8 / S2.9,
now judged up to the boot's stop at record 1156), the dialog checkpoints (S2.9), and the terrain_drawn column (S3.2).
Usage: ~/bin/py -3.12 log/roadmap/merges/I6/census_compare.py <new frame_census.tsv> [<old>] [<raw framegate tsv>]"""
import collections
import csv
import io
import subprocess
import sys


def rd_text(t):
    lines = [l for l in t.splitlines(True) if not l.startswith("#")]
    return {(r["scenario"], r["checkpoint"]): r for r in csv.DictReader(lines, delimiter="\t")}


def rd(p):
    with open(p, encoding="utf-8") as f:
        return rd_text(f.read())


new = rd(sys.argv[1])
old = rd(sys.argv[2] if len(sys.argv) > 2 and sys.argv[2] != "-" else "re/roadmap/frame_census.tsv")
s0 = rd_text(subprocess.run(["git", "show", "c6d7d78b:re/roadmap/frame_census.tsv"], capture_output=True, text=True,
                            encoding="utf-8").stdout)
for name, base in (("I6", old), ("S0", s0)):
    over = {k: new[k] for k in base if k in new}
    print(f"== against {name}: rows old {len(base)} new {len(new)}, missing {len(base) - len(over)}")
    print("  status old:", dict(collections.Counter(r["status"] for r in base.values())))
    print("  status new over old rows:", dict(collections.Counter(r["status"] for r in over.values())))
    tr = collections.Counter((base[k]["status"], new[k]["status"]) for k in over)
    print("  transitions:", {f"{a}->{b}": n for (a, b), n in sorted(tr.items())})
    lost = sorted(k for k in over if base[k]["status"] == "PASS" and new[k]["status"] != "PASS")
    print("  PASS rows lost:", len(lost), lost[:20])
    gained = sorted(k for k in over if base[k]["status"] != "PASS" and new[k]["status"] == "PASS")
    print("  PASS rows gained:", len(gained))
    for k in gained:
        print("    +", "/".join(k), base[k]["status"])
print("status new:", dict(collections.Counter(r["status"] for r in new.values())))
added = sorted(set(new) - set(old))
print("rows new since I6:", len(added), dict(collections.Counter(new[k]["status"] for k in added)))
print("primary causes new:", collections.Counter(r["primary_cause"] for r in new.values() if r["status"] != "PASS").most_common(12))
ks = sorted(k for k in new if k[0] == "cov_r_s2_screens")
print("cov_r_s2_screens:", dict(collections.Counter(new[k]["status"] for k in ks)))
for k in ks:
    print(f"   {k[1]:16} {old.get(k, {}).get('status', '-'):12} -> {new[k]['status']:12} {new[k].get('total_px', '')}")
if len(sys.argv) > 3:
    raw = list(csv.DictReader(open(sys.argv[3], encoding="utf-8"), delimiter="\t"))
    if raw and "terrain_drawn" in raw[0]:
        print("terrain_drawn (raw framegate):", dict(collections.Counter((r.get("terrain"), r["terrain_drawn"]) for r in raw)))
