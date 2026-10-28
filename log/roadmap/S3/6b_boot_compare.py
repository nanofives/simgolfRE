"""S3.6b: the rebased build's boot gate TSV against the committed tree's (I5, b3d61e41: log/roadmap/merges/S2S3-5/boot_x64.tsv),
row for row on status / records / expected, with the counts over HEAD's 61 scenarios (log/p5/boot_gate_loc.tsv).

    python log/roadmap/S3/6b_boot_compare.py <boot tsv>
"""
import collections
import csv
import sys


def rd(p):
    return {r["scenario"]: r for r in csv.DictReader(open(p, encoding="utf-8"), delimiter="\t")}


head = rd("log/p5/boot_gate_loc.tsv")
base = rd("log/roadmap/merges/S2S3-5/boot_x64.tsv")
new = rd(sys.argv[1])
print("HEAD scenarios", len(head), "committed rows", len(base), "new rows", len(new))
diff = 0
for n in sorted(base):
    b, w = base[n], new.get(n)
    if w is None:
        print("MISSING", n)
        diff += 1
        continue
    d = [k for k in ("status", "records", "expected") if b[k] != w[k]]
    if d:
        diff += 1
        print("DIFF", n, {k: (b[k], w[k]) for k in d}, w.get("detail", "")[:200])
print("HEAD-61 counts:", dict(collections.Counter(new[n]["status"] for n in head if n in new)))
print("rows not in the committed TSV:", {n: (new[n]["status"], new[n]["records"], new[n]["expected"]) for n in sorted(set(new) - set(base))})
print("committed rows differing:", diff)
