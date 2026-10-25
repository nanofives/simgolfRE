"""I4 (S1 + S8 merge): compare a boot gate TSV with the baseline (log/roadmap/merges/S6-1/boot_gate.tsv, HEAD + S6), counted over
HEAD's 61 scenarios (log/p5/boot_gate_loc.tsv) plus the section scenarios S1 (5) and S8 (1) with the records their leads measured.
Usage: ~/bin/py -3.12 log/roadmap/merges/S1S8-4/boot_compare.py <boot tsv>"""
import collections
import csv
import sys


def rd(p):
    return {r["scenario"]: r for r in csv.DictReader(open(p, encoding="utf-8"), delimiter="\t")}


head = rd("log/p5/boot_gate_loc.tsv")
base = rd("log/roadmap/merges/S6-1/boot_gate.tsv")
new = rd(sys.argv[1])
SECTION = {"cov_r_bio": 687, "cov_r_flic_input": 75, "cov_r_tutorial": 1195, "cov_r_theme_cancel": 123, "cov_r_palette_hot": 565,
           "cov_r_s8_play": 216}
print("HEAD scenarios", len(head), "baseline rows", len(base), "new rows", len(new))
diff = 0
for n in sorted(head):
    b, w = base.get(n), new.get(n)
    if w is None:
        print("MISSING", n)
        diff += 1
        continue
    d = [k for k in ("status", "records", "expected") if b[k] != w[k]]
    if d:
        diff += 1
        print("DIFF", n, {k: (b[k], w[k]) for k in d}, w["detail"][:300])
print("HEAD-61 counts:", dict(collections.Counter(new[n]["status"] for n in head if n in new)))
bad = 0
for n, recs in SECTION.items():
    w = new.get(n)
    ok = w is not None and w["status"] == "PASS" and int(w["records"]) == recs == int(w["expected"])
    bad += not ok
    print("section", n, "OK" if ok else "BAD", (w or {}).get("status"), (w or {}).get("records"), (w or {}).get("expected"))
other = sorted(set(new) - set(head) - set(SECTION))
print("other rows (not HEAD's, not S1 / S8):", {n: new[n]["status"] for n in other})
print("HEAD rows differing from the baseline:", diff, "| section rows not as measured:", bad)
sys.exit(1 if diff or bad else 0)
