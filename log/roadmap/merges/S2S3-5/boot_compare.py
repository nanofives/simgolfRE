"""I5 (S2 + S3 merge): compare a boot gate TSV with the HEAD + S1 + S8 baseline (log/roadmap/merges/S1S8-4/boot_x64.tsv, 67 rows:
HEAD's 61 + S1's 5 + S8's 1) on status / records / expected, then report S2's three scenarios against what MERGE_wave4.md states:
cov_a_quit PASS 301 / 301, cov_a_quit_title NO_FRAMES, cov_r_s2_screens FAIL at record 771 (U-1253, a new row, not a regression).
Usage: ~/bin/py -3.12 log/roadmap/merges/S2S3-5/boot_compare.py <boot tsv> [<other tsv to compare row for row, e.g. x64 vs Win32>]"""
import collections
import csv
import sys


def rd(p):
    return {r["scenario"]: r for r in csv.DictReader(open(p, encoding="utf-8"), delimiter="\t")}


head = rd("log/p5/boot_gate_loc.tsv")
base = rd("log/roadmap/merges/S1S8-4/boot_x64.tsv")
new = rd(sys.argv[1])
print("HEAD scenarios", len(head), "baseline rows", len(base), "new rows", len(new))
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
        print("DIFF", n, {k: (b[k], w[k]) for k in d}, w["detail"][:300])
print("HEAD-61 counts:", dict(collections.Counter(new[n]["status"] for n in head if n in new)))
print("baseline-67 counts:", dict(collections.Counter(new[n]["status"] for n in base if n in new)))
S2 = {"cov_a_quit": ("PASS", "301"), "cov_a_quit_title": ("NO_FRAMES", None), "cov_r_s2_screens": ("FAIL", None)}
bad = 0
for n, (st, recs) in S2.items():
    w = new.get(n)
    ok = w is not None and w["status"] == st and (recs is None or (w["records"] == recs == w["expected"]))
    if n == "cov_r_s2_screens" and ok:
        ok = "771" in w["detail"]
    bad += not ok
    print("S2", n, "OK" if ok else "BAD", (w or {}).get("status"), (w or {}).get("records"), (w or {}).get("expected"),
          (w or {}).get("detail", "")[:200])
other = sorted(set(new) - set(base) - set(S2))
print("other rows (other sections' uncommitted recordings in the shared corpus):", {n: new[n]["status"] for n in other})
print("baseline rows differing:", diff, "| S2 rows not as stated:", bad)
if len(sys.argv) > 2:
    o = rd(sys.argv[2])
    nd = [n for n in sorted(set(new) | set(o)) if (new.get(n) or {}).get("status") != (o.get(n) or {}).get("status")
          or (new.get(n) or {}).get("records") != (o.get(n) or {}).get("records")]
    print("rows differing between the two tsvs:", len(nd), nd)
    diff += len(nd)
sys.exit(1 if diff or bad else 0)
