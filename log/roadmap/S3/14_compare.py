"""S3.14: the gate outputs of the S3_14 mirror (log/roadmap/S3/14_*) against S3.13's (log/roadmap/S3/13_*, the same S3 tree on
78ffb06d) and, where I12 measured them, I12's (log/roadmap/merges/I12/, the S3 tree on d66aa755 before S3.14's edits).

    ~/bin/py -3.12 log/roadmap/S3/14_compare.py segs|boot|census|terrain|suite
(run from anywhere)"""
import collections
import csv
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
S = ROOT / "log/roadmap/S3"
I12 = ROOT / "log/roadmap/merges/I12"


def rows(p):
    lines = [l for l in open(p, encoding="utf-8") if not l.startswith("#")]
    return list(csv.DictReader(lines, delimiter="\t"))


def keyed(p, *k):
    return {tuple(r[x] for x in k): r for r in rows(p)}


def cmp(name, new, old, keys, fields, sk="status"):
    if not old.exists():
        print(f"{name}: no {old}")
        return
    a, b = keyed(new, *keys), keyed(old, *keys)
    st = collections.Counter(r[sk] for r in a.values())
    print(f"{name}: {new.name} {len(a)} rows {dict(st)}; vs {old.relative_to(ROOT)} {len(b)} rows "
          f"{dict(collections.Counter(r[sk] for r in b.values()))}")
    diff = [(k, {f: (b.get(k, {}).get(f), a.get(k, {}).get(f)) for f in fields if b.get(k, {}).get(f) != a.get(k, {}).get(f)})
            for k in sorted(set(a) | set(b))]
    diff = [(k, d) for k, d in diff if d]
    print(f"  rows differing in {fields}: {len(diff)}")
    for k, d in diff[:30]:
        print("   ", k, d)


def main(what):
    if what == "segs":
        for be in ("x64", "win32"):
            cmp(f"segs {be}", S / f"14_segs_{be}.tsv", S / f"13_segs_{be}.tsv", ("name",), ["status", "frames_run", "digest"])
            cmp(f"segs {be} (I12)", S / f"14_segs_{be}.tsv", I12 / f"segs_{be}.tsv", ("name",), ["status", "frames_run", "digest"])
    elif what == "boot":
        for be in ("x64", "win32"):
            cmp(f"boot {be}", S / f"14_boot_{be}.tsv", S / f"13_boot_{be}.tsv", ("scenario",), ["status", "records"])
    elif what == "census":
        cmp("census", S / "14_census.tsv", S / "13_census.tsv", ("scenario", "checkpoint"), ["status", "total_px", "beyond_px"])
    elif what == "terrain":
        cmp("terrain", S / "14_terrain_verdicts.tsv", S / "13_terrain_verdicts.tsv", ("scenario", "checkpoint"),
            ["verdict", "drawn", "diff", "big"], sk="verdict")
    elif what == "suite":
        d = S / "14_suite"
        for f in sorted(d.glob("test_*.txt")):
            last = f.read_text(encoding="utf-8", errors="replace").strip().splitlines()[-1:] or [""]
            print(f"{f.stem:40s} {last[0][:150]}")


if __name__ == "__main__":
    main(sys.argv[1])
