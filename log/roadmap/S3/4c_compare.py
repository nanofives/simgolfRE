"""S3.4c: segment and boot TSVs of the S3_4c build against the committed tree's (integrator I6, HEAD bbc9072f:
log/roadmap/merges/I6/segs_x64.tsv, segs_win32.tsv, boot_x64.tsv).

    ~/bin/py -3.12 log/roadmap/S3/4c_compare.py segs <new tsv> <committed tsv>
    ~/bin/py -3.12 log/roadmap/S3/4c_compare.py boot <new tsv>
"""
import collections
import csv
import sys


def rd(p, key):
    return {r[key]: r for r in csv.DictReader(open(p, encoding="utf-8"), delimiter="\t")}


def main():
    kind, new_p = sys.argv[1], sys.argv[2]
    if kind == "segs":
        base, new = rd(sys.argv[3], "name"), rd(new_p, "name")
        fields = ("status", "frames_run", "frames_expected", "digest")
    else:
        base, new = rd("log/roadmap/merges/I6/boot_x64.tsv", "scenario"), rd(new_p, "scenario")
        fields = ("status", "records", "expected")
    diff = 0
    for n in sorted(base):
        w = new.get(n)
        if w is None:
            print("MISSING", n)
            diff += 1
            continue
        d = [k for k in fields if base[n][k] != w[k]]
        if d:
            diff += 1
            print("DIFF", n, {k: (base[n][k], w[k]) for k in d})
    print("rows", len(new), "committed", len(base), "extra", sorted(set(new) - set(base)))
    print("status counts", dict(collections.Counter(r["status"] for r in new.values())))
    if kind == "boot":
        head = rd("log/p5/boot_gate_loc.tsv", "scenario")
        print("HEAD-61 counts", dict(collections.Counter(new[n]["status"] for n in head if n in new)))
    print("committed rows differing:", diff)


if __name__ == "__main__":
    main()
