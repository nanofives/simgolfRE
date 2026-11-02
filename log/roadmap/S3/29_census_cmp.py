"""S3.29: a census of port/build/S3_29 (29_census.sh on / off) against the wave 8 stack's (log/roadmap/S3/w8_out/census_split.tsv:
raw 123 / 565 / 0 / 10, split 677 / 11 / 0 / 10), on the rows both have.

    ~/bin/py -3.12 log/roadmap/S3/29_census_cmp.py <new.tsv> [--base <tsv>] [--out <rows.tsv>]
Prints raw and split status counts of both, transitions, PASS rows lost (raw and split), rows with more total / one-level / beyond px
(raw) or more terrain-origin / 2D px (split), and the summed raw px."""
import argparse
import collections
import csv
import pathlib

HERE = pathlib.Path(__file__).resolve().parent
BASE = HERE / "w8_out" / "census_split.tsv"


def load(p):
    return {(r["scenario"], r["checkpoint"]): r for r in csv.DictReader(open(p, encoding="utf-8"), delimiter="\t")}


def num(r, k):
    try:
        return int(r.get(k) or 0)
    except ValueError:
        return 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("new")
    ap.add_argument("--base", default=str(BASE))
    ap.add_argument("--out")
    a = ap.parse_args()
    B, N = load(a.base), load(a.new)
    keys = sorted(set(B) & set(N))
    print(f"rows: base {len(B)}, new {len(N)}, common {len(keys)}")
    for col in ("status", "split_status"):
        cb = collections.Counter(B[k][col] for k in keys)
        cn = collections.Counter(N[k][col] for k in keys)
        print(f"{col}: base {dict(sorted(cb.items()))}  new {dict(sorted(cn.items()))}")
        tr = collections.Counter((B[k][col], N[k][col]) for k in keys if B[k][col] != N[k][col])
        print(f"  transitions {dict(tr)}")
        lost = [k for k in keys if B[k][col] == "PASS" and N[k][col] != "PASS"]
        print(f"  PASS lost: {len(lost)} {lost[:10]}")
    worse = {c: [k for k in keys if num(N[k], c) > num(B[k], c)] for c in ("total_px", "one_level_px", "beyond_px", "terrain_origin_px",
                                                                         "twod_px")}
    better = {c: [k for k in keys if num(N[k], c) < num(B[k], c)] for c in worse}
    for c in worse:
        print(f"{c}: base {sum(num(B[k], c) for k in keys)} new {sum(num(N[k], c) for k in keys)}  rows more {len(worse[c])}"
              f" fewer {len(better[c])}  {worse[c][:6]}")
    if a.out:
        with open(a.out, "w", encoding="utf-8", newline="") as f:
            f.write("scenario\tcheckpoint\tstatus_base\tstatus_new\tsplit_base\tsplit_new\ttotal_base\ttotal_new\tone_base\tone_new\t"
                    "beyond_base\tbeyond_new\n")
            for k in keys:
                b, n = B[k], N[k]
                f.write("\t".join([k[0], k[1], b["status"], n["status"], b["split_status"], n["split_status"]] +
                                  [str(num(x, c)) for c in ("total_px", "one_level_px", "beyond_px") for x in (b, n)]) + "\n")


if __name__ == "__main__":
    main()
