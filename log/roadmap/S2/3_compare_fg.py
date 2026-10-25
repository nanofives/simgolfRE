"""S2.3: compare two framegate TSVs row by row (scenario, checkpoint): status and total_px.
usage: ~/bin/py -3.12 log/roadmap/S2/3_compare_fg.py <baseline.tsv> <new.tsv>
A row is WORSE when its total_px grew or it left PASS, BETTER when total_px fell or it became PASS; rows missing on either
side are listed. Exit code 1 when any row is WORSE.
"""
import csv
import sys


def load(p):
    rows = {}
    with open(p, encoding="utf-8") as f:
        for r in csv.DictReader((l for l in f if not l.startswith("#")), delimiter="\t"):
            rows[(r["scenario"], r["checkpoint"])] = r
    return rows


def px(r):
    try:
        return int(r["total_px"])
    except (ValueError, TypeError):
        return None


def main():
    a, b = load(sys.argv[1]), load(sys.argv[2])
    worse, better, same = [], [], 0
    for k in sorted(set(a) & set(b)):
        ra, rb = a[k], b[k]
        pa, pb = px(ra), px(rb)
        if ra["status"] == "PASS" and rb["status"] != "PASS":
            worse.append((k, ra["status"], rb["status"], pa, pb))
        elif rb["status"] == "PASS" and ra["status"] != "PASS":
            better.append((k, ra["status"], rb["status"], pa, pb))
        elif pa is not None and pb is not None and pb > pa:
            worse.append((k, ra["status"], rb["status"], pa, pb))
        elif pa is not None and pb is not None and pb < pa:
            better.append((k, ra["status"], rb["status"], pa, pb))
        elif ra["status"] != rb["status"]:
            worse.append((k, ra["status"], rb["status"], pa, pb))
        else:
            same += 1
    print(f"rows compared {len(set(a) & set(b))}: same {same}, better {len(better)}, worse {len(worse)}")
    print(f"only in baseline {len(set(a) - set(b))}, only in new {len(set(b) - set(a))}")
    for tag, lst in (("BETTER", better), ("WORSE", worse)):
        for k, sa, sb, pa, pb in lst:
            print(f"{tag}\t{k[0]}\t{k[1]}\t{sa} -> {sb}\t{pa} -> {pb}")
    for k in sorted(set(a) ^ set(b)):
        print(f"MISSING\t{k[0]}\t{k[1]}\t{'baseline only' if k in a else 'new only'}")
    return 1 if worse else 0


if __name__ == "__main__":
    sys.exit(main())
