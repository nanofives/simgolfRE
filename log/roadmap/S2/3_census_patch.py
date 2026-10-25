"""S2.3: replace the rows of the U-1120 scenarios in a census TSV by their 3_rerun_noend.py re-runs (as S0.3 did).
usage: ~/bin/py -3.12 log/roadmap/S2/3_census_patch.py <census.tsv> <out.tsv> <rerun.json>...
"""
import csv
import json
import sys


def main():
    src, out, reruns = sys.argv[1], sys.argv[2], sys.argv[3:]
    with open(src, encoding="utf-8") as f:
        lines = f.read().splitlines()
    head = [l for l in lines if l.startswith("#")]
    rows = list(csv.DictReader([l for l in lines if not l.startswith("#")], delimiter="\t"))
    cols = list(rows[0].keys())
    repl = {}
    for p in reruns:
        with open(p, encoding="utf-8") as f:
            last = [l for l in f.read().splitlines() if l.strip()][-1]
        for r in json.loads(last):
            repl[(r["scenario"], r["checkpoint"])] = r
    n = 0
    with open(out, "w", encoding="utf-8", newline="") as f:
        for h in head:
            f.write(h + "\n")
        w = csv.DictWriter(f, cols, delimiter="\t", extrasaction="ignore", lineterminator="\n")
        w.writeheader()
        for r in rows:
            k = (r["scenario"], r["checkpoint"])
            if k in repl:
                r = {c: repl[k].get(c, r.get(c, "")) for c in cols}
                n += 1
            w.writerow(r)
    print(f"replaced {n} rows of {len(rows)}")


if __name__ == "__main__":
    main()
