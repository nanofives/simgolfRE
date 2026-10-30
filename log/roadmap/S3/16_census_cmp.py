"""S3.16: compare the census with the snapshot fix (16_census.tsv) against I14's census and against the run without the fix
(16_census_tgt.tsv, the scenarios of the 7 moving checkpoints only). Prints the counts, every row that loses PASS or has more
differing px than I14, every row whose result differs between the two snapshot positions, and the per-status totals.

    ~/bin/py -3.12 log/roadmap/S3/16_census_cmp.py [> log/roadmap/S3/16_census_cmp.log]
"""
import collections
import csv
import pathlib

HERE = pathlib.Path(__file__).resolve().parent
I14 = HERE.parents[0] / "merges" / "I14" / "census.tsv"


def load(p):
    return {(r["scenario"], r["checkpoint"]): r for r in csv.DictReader(open(p), delimiter="\t")}


def px(r):
    try:
        return int(r.get("total_px") or 0)
    except ValueError:
        return 0


def main():
    old, new = load(I14), load(HERE / "16_census.tsv")
    tgt_p = HERE / "16_census_tgt.tsv"
    tgt = load(tgt_p) if tgt_p.exists() else {}
    print("I14 rows", len(old), collections.Counter(r["status"] for r in old.values()))
    print("S3.16 rows", len(new), collections.Counter(r["status"] for r in new.values()))
    lost, worse, better, gained = [], [], [], []
    for k, r in sorted(new.items()):
        o = old.get(k)
        if not o:
            continue
        if o["status"] == "PASS" and r["status"] != "PASS":
            lost.append(k)
        if o["status"] != "PASS" and r["status"] == "PASS":
            gained.append(k)
        if px(r) > px(o):
            worse.append((k, px(o), px(r), o["status"], r["status"]))
        elif px(r) < px(o):
            better.append(k)
    print(f"against I14: PASS lost {len(lost)}, PASS gained {len(gained)}, rows with more px {len(worse)}, with fewer {len(better)}")
    for k in lost:
        print("  LOST", *k, new[k]["status"], new[k]["detail"][:100])
    for k, a, b, so, sn in worse:
        print(f"  MORE {k[0]} {k[1]} {a} -> {b} ({so} -> {sn}) {new[k]['first_unsupported_drawing'][:60]}")
    print("  gained:", " ".join(f"{a}/{b}" for a, b in gained))
    if tgt:
        moved = []
        for k, t in sorted(tgt.items()):
            r = new.get(k)
            if r and (t["regions"] != r["regions"] or t["status"] != r["status"]):
                moved.append((k, t, r))
        print(f"snapshot position: {len(tgt)} rows run without the fix, {len(moved)} differ from the run with it")
        for k, t, r in moved:
            print(f"  MOVED {k[0]} {k[1]} without fix {t['status']} {px(t)} px (beyond {t['beyond_px']}) -> with fix {r['status']} "
                  f"{px(r)} px (beyond {r['beyond_px']})")


if __name__ == "__main__":
    main()
