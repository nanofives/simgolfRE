"""S3.28: compare the S3.28 census (28_out/census_split.tsv) with the wave 8 stack's (w8_out/census_split.tsv), raw and split.

    ~/bin/py -3.12 log/roadmap/S3/28_census_cmp.py            # writes 28_out/census_cmp.tsv, prints the counts and every moved row

A row moves when its raw status, raw total_px / beyond_px, split status or split twod_px / terrain_origin_px differ. Gate: no PASS lost
and no row with more differing px, in raw and in split.
"""
import csv
import pathlib
import sys
from collections import Counter

HERE = pathlib.Path(__file__).resolve().parent
REF = HERE / "w8_out" / "census_split.tsv"
NEW = HERE / "28_out" / "census_split.tsv"
OUT = HERE / "28_out" / "census_cmp.tsv"


def rows(p):
    with open(p, newline="") as f:
        return {(r["scenario"], r["checkpoint"]): r for r in csv.DictReader(f, delimiter="\t")}


def num(v):
    try:
        return int(v)
    except (TypeError, ValueError):
        return 0


def main():
    a, b = rows(REF), rows(NEW)
    missing = sorted(set(a) - set(b))
    extra = sorted(set(b) - set(a))
    ca = Counter(r["status"] for r in a.values()); cb = Counter(r["status"] for r in b.values())
    sa = Counter(r["split_status"] for r in a.values()); sb = Counter(r["split_status"] for r in b.values())
    lost_raw, lost_split, worse_raw, worse_split, moved = [], [], [], [], []
    for k in sorted(set(a) & set(b)):
        x, y = a[k], b[k]
        f = lambda r: (r["status"], num(r["total_px"]), num(r["beyond_px"]), r["split_status"], num(r.get("twod_px")),
                       num(r.get("terrain_origin_px")))
        if f(x) == f(y):
            continue
        moved.append((k, f(x), f(y)))
        if x["status"] == "PASS" and y["status"] != "PASS":
            lost_raw.append(k)
        if x["split_status"] == "PASS" and y["split_status"] != "PASS":
            lost_split.append(k)
        if num(y["total_px"]) > num(x["total_px"]) or num(y["beyond_px"]) > num(x["beyond_px"]):
            worse_raw.append(k)
        if num(y.get("twod_px")) > num(x.get("twod_px")) or num(y.get("terrain_origin_px")) > num(x.get("terrain_origin_px")):
            worse_split.append(k)
    with open(OUT, "w", newline="") as f:
        w = csv.writer(f, delimiter="\t", lineterminator="\n")
        w.writerow(["scenario", "checkpoint", "raw_before", "raw_after", "total_px_before", "total_px_after", "beyond_before",
                    "beyond_after", "split_before", "split_after", "twod_before", "twod_after", "terr_before", "terr_after"])
        for (s, c), p, q in moved:
            w.writerow([s, c, p[0], q[0], p[1], q[1], p[2], q[2], p[3], q[3], p[4], q[4], p[5], q[5]])
    print(f"rows ref {len(a)} new {len(b)} missing {len(missing)} extra {len(extra)}")
    print("raw   ref", dict(ca), "new", dict(cb))
    print("split ref", dict(sa), "new", dict(sb))
    print(f"moved {len(moved)}; PASS lost raw {lost_raw} split {lost_split}; worse raw {worse_raw} split {worse_split}")
    for (s, c), p, q in moved:
        print(f"  {s}/{c}: raw {p[0]} {p[1]}px (beyond {p[2]}) -> {q[0]} {q[1]}px (beyond {q[2]}); split {p[3]} 2D {p[4]} terr {p[5]}"
              f" -> {q[3]} 2D {q[4]} terr {q[5]}")
    return 0 if not (missing or lost_raw or lost_split or worse_raw or worse_split) else 1


if __name__ == "__main__":
    sys.exit(main())
