"""S5.4 (copy of 1_boot_compare.py): compare the S5.3 mirror's boot gate TSV with S0's HEAD baseline, counted over HEAD's 61 scenarios only
(the ones of log/p5/boot_gate_loc.tsv; the shared corpus holds S1's extra recordings, U-1102)."""
import collections
import csv
import sys

def rd(p):
    return {r["scenario"]: r for r in csv.DictReader(open(p, encoding="utf-8"), delimiter="\t")}

head = rd("log/p5/boot_gate_loc.tsv")
base = rd("log/roadmap/S0/1_boot_gate.tsv")
new = rd(sys.argv[1])
print("HEAD scenarios", len(head), "S0 rows", len(base), "new rows", len(new))
diff = 0
for n in sorted(head):
    b, w = base.get(n), new.get(n)
    if w is None:
        print("MISSING", n); diff += 1; continue
    d = [k for k in ("status", "records", "expected") if b[k] != w[k]]
    if d:
        diff += 1
        print("DIFF", n, {k: (b[k], w[k]) for k in d}, w["detail"][:300])
print("HEAD-61 counts:", dict(collections.Counter(new[n]["status"] for n in head if n in new)))
print("extra rows (not HEAD's):", {n: new[n]["status"] for n in sorted(set(new) - set(head))})
print("rows differing from S0:", diff)
