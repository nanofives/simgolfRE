"""S3.11: segment tables of the resubmission tree against the committed tree's (log/roadmap/merges/I6/segs_x64.tsv / segs_win32.tsv,
= I5's, I4's, I1's and S0's), x64 against Win32, and the course_season/08 Win32 re-runs (11_segs_cs08_<k>.tsv).

    ~/bin/py -3.12 log/roadmap/S3/11_segs_compare.py > log/roadmap/S3/11_segs_compare.log"""
import csv
import glob


def rd(p):
    return {r["name"]: r for r in csv.DictReader(open(p, encoding="utf-8"), delimiter="\t")}


base = {"x64": rd("log/roadmap/merges/I6/segs_x64.tsv"), "win32": rd("log/roadmap/merges/I6/segs_win32.tsv")}
new = {"x64": rd("log/roadmap/S3/11_segs_x64.tsv"), "win32": rd("log/roadmap/S3/11_segs_win32.tsv")}


def diff(a, b):
    return [n for n in a if n not in b or a[n]["digest"] != b[n]["digest"] or a[n]["status"] != b[n]["status"]
            or a[n]["frames_run"] != b[n]["frames_run"]]


for k in ("x64", "win32"):
    st = sum(1 for r in new[k].values() if r["status"] == "PASS")
    d = diff(base[k], new[k])
    print(f"{k}: {st} / {len(new[k])} PASS; against the committed table ({len(base[k])} rows): {len(d)} differ {d[:8]}")
d = diff(new["x64"], new["win32"])
print(f"x64 vs win32: {len(d)} differ {d[:8]}")
for p in sorted(glob.glob("log/roadmap/S3/11_segs_cs08_*.tsv")):
    for n, r in rd(p).items():
        b = base["win32"].get(n, {})
        print(f"{p}: {n} {r['status']} {r['frames_run']}/{r['frames_expected']} digest {r['digest']} "
              f"{'= committed' if r['digest'] == b.get('digest') else '!= committed ' + b.get('digest', '?')}")
