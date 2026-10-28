"""S3.5: segment digests of the 4b mirror runs against S6-1's (x64 and Win32)."""
import csv

def rd(p):
    return {r["name"]: r for r in csv.DictReader(open(p, encoding="utf-8"), delimiter="\t")}

bad = 0
for b in ("x64", "win32"):
    ref = rd(f"log/roadmap/merges/S6-1/segs_{b}.tsv")
    new = rd(f"log/roadmap/S3/5_segs_{b}.tsv")
    d = [n for n in ref if n not in new or ref[n]["digest"] != new[n]["digest"] or new[n]["status"] != "PASS"]
    print(b, "rows", len(new), "ref", len(ref), "digest/status differences:", len(d), d[:5])
    bad += len(d)
print("total differences:", bad)
