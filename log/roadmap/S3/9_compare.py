"""S3.9: the combined tree's gate tables (log/roadmap/S3/9_out/S3_9/) against plain 37e58e96's. The reference rows are I15's
(log/roadmap/merges/I15/): I15's mirror port/build/I15 is byte-equal to 37e58e96 in port/ and re/model/include ignoring CR
(9_notes.md section 1), so its tables are 37e58e96's.

    ~/bin/py -3.12 log/roadmap/S3/9_compare.py [segs_x64 segs_win32 boot_x64 boot_win32 equiv_x64 equiv_win32 app_replay_x64 app_replay_win32]
Columns compared (timing columns never): segments name/status/frames/code/record/objects/digest; boot scenario/status/records/
expected; equivalence scenario/status/records/boot_status/violations; app replay name/status/records/code/first."""
import csv
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
REF = ROOT / "log/roadmap/merges/I15"
NEW = ROOT / "log/roadmap/S3/9_out/S3_9"
SPEC = {
    "segs": ("name", ["status", "frames_run", "frames_expected", "code", "record", "objects", "digest"]),
    "boot": ("scenario", ["status", "records", "expected"]),
    "equiv": ("scenario", ["status", "records", "expected", "boot_status", "violations"]),
    "app_replay": ("name", ["status", "records", "expected", "code", "first"]),
}


def rows(p):
    lines = [l for l in open(p, encoding="utf-8") if not l.startswith("#")]
    return list(csv.DictReader(lines, delimiter="\t"))


def cmp(name):
    kind = name.rsplit("_", 1)[0]
    key, cols = SPEC[kind]
    a = {r[key]: r for r in rows(REF / f"{name}.tsv")}
    p = NEW / f"{name}.tsv"
    if not p.exists():
        print(f"{name}: no table yet")
        return None
    b = {r[key]: r for r in rows(p)}
    diff = [k for k in sorted(set(a) | set(b)) if any((a.get(k) or {}).get(c) != (b.get(k) or {}).get(c) for c in cols)]
    cnt = {}
    for r in b.values():
        cnt[r["status"]] = cnt.get(r["status"], 0) + 1
    print(f"{name}: {len(b)} rows {cnt}; {len(diff)} differ from 37e58e96 (I15)")
    for k in diff:
        print("   ", k, {c: ((a.get(k) or {}).get(c), (b.get(k) or {}).get(c)) for c in cols
                         if (a.get(k) or {}).get(c) != (b.get(k) or {}).get(c)})
    return len(diff)


def main():
    names = sys.argv[1:] or ["segs_x64", "segs_win32", "boot_x64", "boot_win32", "equiv_x64", "equiv_win32", "app_replay_x64",
                             "app_replay_win32"]
    for n in names:
        cmp(n)


if __name__ == "__main__":
    main()
