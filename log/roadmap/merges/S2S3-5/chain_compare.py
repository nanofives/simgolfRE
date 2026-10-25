"""I5: chain TSV against I4's (log/roadmap/merges/S1S8-4/chain_x64_merged.tsv) on status / frames_run / frames_expected / record / digest.
Usage: ~/bin/py -3.12 log/roadmap/merges/S2S3-5/chain_compare.py <chain tsv> [<rerun tsv whose rows replace the first's>]"""
import collections
import csv
import sys


def rd(p):
    return {(r["chain"], r["checkpoint"]): r for r in csv.DictReader(open(p, encoding="utf-8"), delimiter="\t")}


old = rd("log/roadmap/merges/S1S8-4/chain_x64_merged.tsv")
new = rd(sys.argv[1])
if len(sys.argv) > 2:
    new.update(rd(sys.argv[2]))
keys = ("status", "frames_run", "frames_expected", "record", "digest")
d = [k for k in sorted(set(old) | set(new)) if k not in old or k not in new or any(old[k][c] != new[k][c] for c in keys)]
print(len(old), len(new), "differ from I4:", d[:20], collections.Counter(r["status"] for r in new.values()))
for k in d[:20]:
    print(" ", k, {c: ((old.get(k) or {}).get(c), (new.get(k) or {}).get(c)) for c in keys}, (new.get(k) or {}).get("detail", "")[:200])
sys.exit(1 if d else 0)
