"""S3.32: compare this step's gate TSVs (log/roadmap/S3/32_work/<name>.tsv) with I22's (log/roadmap/merges/I22/<name>.tsv) row
for row on the columns that carry the verdict. Rows only in this step's table (golfer_events, new) are listed apart.

    ~/bin/py -3.12 log/roadmap/S3/32_cmp_gates.py boot_x64 segs_x64 boot_win32 segs_win32 equiv_x64
"""
import csv
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
MINE = ROOT / "log" / "roadmap" / "S3" / "32_work"
REF = ROOT / "log" / "roadmap" / "merges" / "I22"
COLS = {"boot": ("status", "records", "expected", "detail"),
        "segs": ("status", "frames_run", "frames_expected", "code", "record", "objects", "digest"),
        "equiv": ("status", "records", "expected", "boot_status", "live_vs_boot", "replay_vs_live", "violations")}


def load(p):
    with open(p, encoding="utf-8") as f:
        rows = list(csv.DictReader(f, delimiter="\t"))
    key = "name" if rows and "name" in rows[0] else "scenario"
    return {r[key]: r for r in rows}


def main(names):
    rc = 0
    for n in names:
        a, b = load(REF / f"{n}.tsv"), load(MINE / f"{n}.tsv")
        cols = COLS[n.split("_")[0]]
        diff = [k for k in a if k in b and any(a[k].get(c) != b[k].get(c) for c in cols)]
        missing = [k for k in a if k not in b]
        new = [k for k in b if k not in a]
        counts = {}
        for r in b.values():
            counts[r["status"]] = counts.get(r["status"], 0) + 1
        print(f"{n}: {len(b)} rows {counts}; vs I22 {len(a)} rows: {len(diff)} differ, {len(missing)} missing, new {new}")
        for k in diff[:10]:
            print("   differ", k, {c: (a[k].get(c), b[k].get(c)) for c in cols if a[k].get(c) != b[k].get(c)})
        for k in new:
            print("   new", k, {c: b[k].get(c) for c in cols})
        rc |= bool(diff or missing)
    return rc


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
