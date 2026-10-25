"""S2.9: compare the boot gate TSV of the S2.9 mirror build with S2.4's (log/roadmap/S2/4_boot_gate.tsv).

The baseline's scenario set is HEAD's 61 plus the extras the shared corpus held then; rows only in the new TSV are corpus
recordings added since (U-1102) and are listed, not counted. Run from the repo root (or the mirror, whose log/ is a junction).
"""
import collections
import csv

old = {r["scenario"]: r for r in csv.DictReader(open("log/roadmap/S2/4_boot_gate.tsv", encoding="utf-8"), delimiter="\t")}
new = {r["scenario"]: r for r in csv.DictReader(open("log/roadmap/S2/9_boot_gate.tsv", encoding="utf-8"), delimiter="\t")}
print("baseline", len(old), "new", len(new))
diff = 0
for n in sorted(set(old) | set(new)):
    o, w = old.get(n), new.get(n)
    if o is None:
        print("EXTRA (not in the baseline, U-1102)", n, w["status"], w["records"], w["expected"], w["detail"][:200])
    elif w is None:
        print("GONE", n)
        diff += 1
    else:
        d = [k for k in ("status", "records", "expected") if o[k] != w[k]]
        if d:
            diff += 1
            print("DIFF", n, {k: (o[k], w[k]) for k in d}, w["detail"][:300])
print("baseline rows compared:", len(set(old) & set(new)), "differ:", diff)
print("baseline counts:", dict(collections.Counter(r["status"] for r in old.values())))
print("new counts on the baseline's scenarios:", dict(collections.Counter(new[n]["status"] for n in old if n in new)))
