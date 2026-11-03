"""S3.32: the census (log/roadmap/S3/32_work/census_split.tsv) against I22's (log/roadmap/merges/I22/census_split.tsv), row for row:
raw status and px (total_px, beyond_px) and the split verdict (split_status, twod_px, terrain_origin_px). A row is LOST when I22 had
PASS and this tree does not, WORSE when a pixel count grew, BETTER when one fell and none grew. Rows only in one table are listed.

    ~/bin/py -3.12 log/roadmap/S3/32_census_cmp.py [<mine.tsv>]
"""
import csv
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
REF = ROOT / "log" / "roadmap" / "merges" / "I22" / "census_split.tsv"
MINE = ROOT / "log" / "roadmap" / "S3" / "32_work" / "census_split.tsv"
PX = ("total_px", "beyond_px", "twod_px", "terrain_origin_px")


def load(p):
    with open(p, encoding="utf-8") as f:
        return {(r["scenario"], r["checkpoint"]): r for r in csv.DictReader(f, delimiter="\t")}


def num(v):
    try:
        return int(v)
    except (TypeError, ValueError):
        return 0


def main(mine=MINE):
    a, b = load(REF), load(mine)
    lost, worse, better, changed = [], [], [], []
    for k in sorted(set(a) & set(b)):
        ra, rb = a[k], b[k]
        for col in ("status", "split_status"):
            if ra[col] == "PASS" and rb[col] != "PASS":
                lost.append((k, col, ra[col], rb[col]))
        up = [c for c in PX if num(rb.get(c)) > num(ra.get(c))]
        down = [c for c in PX if num(rb.get(c)) < num(ra.get(c))]
        if up:
            worse.append((k, {c: (ra.get(c), rb.get(c)) for c in up}))
        elif down:
            better.append((k, {c: (ra.get(c), rb.get(c)) for c in down}))
        if any(ra[c] != rb[c] for c in ("status", "split_status")) or up or down:
            changed.append(k)
    cnt = lambda t, col: {s: sum(1 for r in t.values() if r[col] == s) for s in sorted({r[col] for r in t.values()})}
    print(f"I22 {len(a)} rows raw {cnt(a, 'status')} split {cnt(a, 'split_status')}")
    print(f"S3.32 {len(b)} rows raw {cnt(b, 'status')} split {cnt(b, 'split_status')}")
    oa, ob = sorted(set(a) - set(b)), sorted(set(b) - set(a))
    print(f"only I22 {len(oa)} rows {oa[:10]}; only S3.32 {len(ob)} rows {ob[:10]}")
    print(f"lost {len(lost)}, worse {len(worse)}, better {len(better)}, rows changed {len(changed)}")
    for x in lost + worse + better:
        print("  ", x)
    for k in changed:
        print("   changed", k, {c: (a[k][c], b[k][c]) for c in ("status", "split_status") + PX})
    return 1 if lost or worse else 0


if __name__ == "__main__":
    sys.exit(main(pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else MINE))
