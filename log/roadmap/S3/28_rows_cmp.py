"""S3.28: segments / boot rows against the wave 8 references (status, frames / records and digest per row).

    ~/bin/py -3.12 log/roadmap/S3/28_rows_cmp.py      # 28_out/segs_x64.tsv vs w8_out/segs_x64.tsv, segs_w32 vs segs_w32, boot_* vs boot_x64
"""
import csv
import pathlib
import sys
from collections import Counter

H = pathlib.Path(__file__).resolve().parent
PAIRS = [("segs_x64", "segs_x64", ("status", "frames_run", "digest")), ("segs_w32", "segs_w32", ("status", "frames_run", "digest")),
         ("boot_x64", "boot_x64", ("status", "records")), ("boot_w32", "boot_x64", ("status", "records"))]


def load(p, key):
    with open(p, newline="") as f:
        return {r[key]: r for r in csv.DictReader(f, delimiter="\t")}


def main():
    bad = 0
    for new, ref, cols in PAIRS:
        pn, pr = H / "28_out" / f"{new}.tsv", H / "w8_out" / f"{ref}.tsv"
        if not pn.exists():
            print(f"{new}: not run")
            continue
        key = "name" if new.startswith("segs") else "scenario"
        a, b = load(pr, key), load(pn, key)
        diff = [k for k in sorted(set(a) | set(b)) if k not in a or k not in b or any(a[k].get(c) != b[k].get(c) for c in cols)]
        print(f"{new} vs w8 {ref}: rows {len(b)} / {len(a)}, {dict(Counter(r['status'] for r in b.values()))}, differing {len(diff)} {diff[:10]}")
        bad += len(diff)
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
