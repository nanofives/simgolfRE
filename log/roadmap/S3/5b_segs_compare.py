"""S3.5b: segment digests of the rebased build against the committed tree's (log/roadmap/merges/S2S3-5/segs_*.tsv), x64 and
Win32, and x64 against Win32. A row that failed in a full run and passed on a single-segment re-run (course_season/08 on Win32,
5b_segs_cs08_1.tsv) is taken from the re-run; the full run's FAIL line stays in 5b_segs_win32.log.

    python log/roadmap/S3/5b_segs_compare.py > log/roadmap/S3/5b_segs_compare.log
"""
import csv


def rd(p):
    return {r["name"]: r for r in csv.DictReader(open(p, encoding="utf-8"), delimiter="\t")}


committed = {"x64": rd("log/roadmap/merges/S2S3-5/segs_x64.tsv"), "win32": rd("log/roadmap/merges/S2S3-5/segs_win32.tsv")}
new = {"x64": rd("log/roadmap/S3/5b_segs_x64.tsv"), "win32": rd("log/roadmap/S3/5b_segs_win32.tsv")}
for n, r in rd("log/roadmap/S3/5b_segs_cs08_1.tsv").items():
    if new["win32"][n]["status"] != "PASS":
        new["win32"][n] = r


def diff(a, b):
    return [n for n in a if n not in b or a[n]["digest"] != b[n]["digest"] or b[n]["status"] != "PASS"]


for k in ("x64", "win32"):
    d = diff(committed[k], new[k])
    print(f"committed vs 5b {k}", len(committed[k]), len(new[k]), "differ", len(d), d[:5])
d = diff(new["x64"], new["win32"])
print("5b x64 vs 5b win32", len(new["x64"]), len(new["win32"]), "differ", len(d), d[:5])
