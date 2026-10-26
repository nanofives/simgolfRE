"""I7: compare a boot gate TSV with I6's baseline (log/roadmap/merges/I6/boot_x64.tsv, 70 rows) on status / records / expected,
and report cov_r_s2_screens against S2 wave 5's statement (equal through record 1156, stop at the unported showBuyLandScreen).
Usage: ~/bin/py -3.12 log/roadmap/merges/I7/boot_compare.py <boot tsv> [<other tsv to compare row for row, e.g. x64 vs Win32>]"""
import collections
import csv
import sys


def rd(p):
    return {r["scenario"]: r for r in csv.DictReader(open(p, encoding="utf-8"), delimiter="\t")}


head = rd("log/p5/boot_gate_loc.tsv")
base = rd("log/roadmap/merges/I6/boot_x64.tsv")
new = rd(sys.argv[1])
print("HEAD scenarios", len(head), "baseline rows", len(base), "new rows", len(new))
diff = 0
for n in sorted(set(base) | set(new)):
    b, w = base.get(n), new.get(n)
    if w is None or b is None:
        print("MISSING" if w is None else "NEW", n)
        diff += 1
        continue
    d = [k for k in ("status", "records", "expected") if b[k] != w[k]]
    if d:
        diff += 1
        print("DIFF", n, {k: (b[k], w[k]) for k in d}, w.get("detail", "")[:400])
print("rows differing from I6:", diff)
print("HEAD-61 counts:", dict(collections.Counter(new[n]["status"] for n in head if n in new)))
print("all counts:", dict(collections.Counter(r["status"] for r in new.values())))
s = new.get("cov_r_s2_screens")
if s:
    print("cov_r_s2_screens:", {k: s[k] for k in ("status", "records", "expected")}, s.get("detail", "")[:600])
if len(sys.argv) > 2:
    o = rd(sys.argv[2])
    dd = [n for n in sorted(set(o) | set(new)) if n not in o or n not in new or
          any(o[n][k] != new[n][k] for k in ("status", "records", "expected"))]
    print("rows differing between the two tsvs:", len(dd), dd)
