"""S7.5: the mirror's boot gate (log/roadmap/S7/5_boot_gate.tsv) against S0's HEAD baseline over HEAD's 61 scenarios (as
6_boot_compare.py), and every row against the b3d61e41 merge's own boot table (log/roadmap/merges/S2S3-5/boot_x64.tsv).
    ~/bin/py -3.12 log/roadmap/S7/5_boot_compare.py      (repo root)"""
import collections
import csv


def rd(p):
    return {r["scenario"]: r for r in csv.DictReader(open(p, encoding="utf-8"), delimiter="\t")}


head = rd("log/p5/boot_gate_loc.tsv")
base = rd("log/roadmap/S0/1_boot_gate.tsv")
merge5 = rd("log/roadmap/merges/S2S3-5/boot_x64.tsv")
new = rd("log/roadmap/S7/5_boot_gate.tsv")
print("HEAD scenarios", len(head), "S0 rows", len(base), "b3d61e41 rows", len(merge5), "new rows", len(new))
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
        print("DIFF vs S0", n, {k: (b[k], w[k]) for k in d}, w["detail"][:300])
print("HEAD-61 counts:", dict(collections.Counter(new[n]["status"] for n in head if n in new)))
print("HEAD rows differing from S0:", diff)
d2 = 0
for n in sorted(set(merge5) | set(new)):
    a, w = merge5.get(n), new.get(n)
    if a is None or w is None:
        print("ONLY IN", "b3d61e41" if w is None else "new", n, (a or w)["status"])
        d2 += 1
        continue
    d = [k for k in ("status", "records", "expected") if a[k] != w[k]]
    if d:
        d2 += 1
        print("DIFF vs b3d61e41", n, {k: (a[k], w[k]) for k in d}, w["detail"][:300])
print("all counts:", dict(collections.Counter(r["status"] for r in new.values())))
print("rows differing from the b3d61e41 merge's boot table:", d2)
