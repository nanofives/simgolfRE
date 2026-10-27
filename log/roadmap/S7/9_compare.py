"""S7.9: compare two gate TSVs row for row on the columns that carry the verdict (key = first column).
    ~/bin/py -3.12 log/roadmap/S7/9_compare.py <kind> <a.tsv> <b.tsv>
kinds: segs (status frames_run frames_expected code record digest), boot (status records expected), equiv (status records expected
boot_status live_vs_boot replay_vs_live violations), app (status records expected code first; key name + backend).
Prints the status counts of each side, the rows that differ, and `0 rows differ` / `N rows differ`."""
import collections
import csv
import sys

COLS = {"segs": ["status", "frames_run", "frames_expected", "code", "record", "digest"],
        "boot": ["status", "records", "expected"],
        "equiv": ["status", "records", "expected", "boot_status", "live_vs_boot", "replay_vs_live", "violations"],
        "app": ["status", "records", "expected", "code", "first"]}


def rows(p, kind):
    with open(p, encoding="utf-8", newline="") as f:
        r = list(csv.DictReader(f, delimiter="\t"))
    key = (lambda x: (x["name"], x["backend"])) if kind == "app" else (lambda x: x[next(iter(x))])
    return {key(x): x for x in r}


kind, a, b = sys.argv[1:4]
A, B = rows(a, kind), rows(b, kind)
for name, d in (("a", A), ("b", B)):
    print(name, len(d), "rows", dict(collections.Counter(x["status"] for x in d.values())))
bad = []
for k in sorted(set(A) | set(B), key=str):
    if k not in A or k not in B:
        bad.append((k, "missing in " + ("a" if k not in A else "b")))
        continue
    diff = [(c, A[k].get(c), B[k].get(c)) for c in COLS[kind] if A[k].get(c) != B[k].get(c)]
    if diff:
        bad.append((k, diff))
for k, d in bad:
    print("DIFF", k, d)
print(f"{len(bad)} rows differ")
