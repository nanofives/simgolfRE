"""S3.7: compare two framegate runs (<a>.tsv.d and <b>.tsv.d) row by row: status and px per region (total / beyond one level).

    ~/bin/py -3.12 log/roadmap/S3/7_cmp.py <a.tsv.d> <b.tsv.d> [--all] [--region hud_message]
Prints the rows whose regions changed (all rows with --all) and a summary: rows better / worse / same per region.
"""
import collections
import json
import pathlib
import sys


def load(d):
    out = {}
    for p in pathlib.Path(d).glob("*.json"):
        for r in json.loads(p.read_text())["rows"]:
            out[(r["scenario"], r["checkpoint"])] = r
    return out


def regions(s):
    out = {}
    for part in (s or "").split(";"):
        if "=" in part:
            k, v = part.split("=", 1)
            f = v.split("/")
            out[k] = (int(f[0]), int(f[3]) if len(f) > 3 else 0)
    return out


def main():
    a, b = load(sys.argv[1]), load(sys.argv[2])
    show_all = "--all" in sys.argv
    only = sys.argv[sys.argv.index("--region") + 1] if "--region" in sys.argv else None
    summ = collections.defaultdict(collections.Counter)
    status = collections.Counter()
    for k in sorted(set(a) & set(b)):
        ra, rb = a[k], b[k]
        status[(ra["status"], rb["status"])] += 1
        ga, gb = regions(ra.get("regions")), regions(rb.get("regions"))
        changed = []
        for reg in sorted(set(ga) | set(gb)):
            if only and reg != only:
                continue
            pa, pb = ga.get(reg, (0, 0)), gb.get(reg, (0, 0))
            summ[reg]["better" if pb[1] < pa[1] else "worse" if pb[1] > pa[1] else "same"] += 1
            if pa != pb:
                changed.append(f"{reg} {pa[0]}/{pa[1]} -> {pb[0]}/{pb[1]}")
        if changed or show_all:
            print(f"{k[0]}\t{k[1]}\t{ra['status']}->{rb['status']}\t" + "; ".join(changed))
    print()
    for (sa, sb), n in sorted(status.items()):
        print(f"status {sa} -> {sb}: {n}")
    for reg, c in sorted(summ.items()):
        print(f"{reg}: beyond-one-level px better {c['better']} worse {c['worse']} same {c['same']}")
    print(f"rows compared {len(set(a) & set(b))}, only in a {len(set(a) - set(b))}, only in b {len(set(b) - set(a))}")


if __name__ == "__main__":
    main()
