"""S0.1: compare the HEAD boot gate TSV with the last recorded one (log/p5/boot_gate_loc.tsv)."""
import csv
import sys

old = {r["scenario"]: r for r in csv.DictReader(open("log/p5/boot_gate_loc.tsv", encoding="utf-8"), delimiter="\t")}
new = {r["scenario"]: r for r in csv.DictReader(open("log/roadmap/S0/1_boot_gate.tsv", encoding="utf-8"), delimiter="\t")}
print("old", len(old), "new", len(new))
for n in sorted(set(old) | set(new)):
    o, w = old.get(n), new.get(n)
    if o is None:
        print("NEW", n, w["status"], w["records"], w["expected"], w["detail"][:200])
    elif w is None:
        print("GONE", n)
    else:
        d = [k for k in ("status", "records", "expected") if o[k] != w[k]]
        if d:
            print("DIFF", n, {k: (o[k], w[k]) for k in d}, w["detail"][:300])
for n, r in sorted(new.items()):
    if r["status"] not in ("PASS",):
        print("NONPASS", n, r["status"], r["records"], r["expected"], r["detail"][:200])
