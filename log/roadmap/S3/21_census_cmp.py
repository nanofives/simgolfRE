"""S3.21: a census (or a sample of it) of the tree with the vertex and clip rules against f2dff003's census (I19:
log/roadmap/merges/I19/census.tsv, 111 PASS / 577 FAIL / 0 UNSUPPORTED / 10 NOT_RUN), on the rows the new file has.

    ~/bin/py -3.12 log/roadmap/S3/21_census_cmp.py <new.tsv> [--base <tsv>] [--out <diff.tsv>]
Prints the status counts of both on the common rows, status transitions, PASS rows lost, rows with more total / one-level / beyond
px, and the summed px per region (parsed from the `regions` column: name=total/one/pct/beyond)."""
import argparse
import collections
import csv
import pathlib

HERE = pathlib.Path(__file__).resolve().parent
BASE = HERE.parent / "merges" / "I19" / "census.tsv"


def load(p):
    return {(r["scenario"], r["checkpoint"]): r for r in csv.DictReader(open(p, encoding="utf-8"), delimiter="\t")}


def regions(r):
    out = {}
    for part in (r.get("regions") or "").split(";"):
        if "=" not in part:
            continue
        name, v = part.split("=", 1)
        f = v.split("/")
        try:
            out[name] = (int(f[0]), int(f[1]), int(f[3]))
        except (ValueError, IndexError):
            pass
    return out


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
    N, B = load(a.new), load(a.base)
    keys = [k for k in N if k in B]
    cb = collections.Counter(B[k]["status"] for k in keys)
    cn = collections.Counter(N[k]["status"] for k in keys)
    print(f"rows {len(keys)} (new file {len(N)}, base {len(B)})")
    print("base", dict(cb))
    print("new ", dict(cn))
    tr = collections.Counter((B[k]["status"], N[k]["status"]) for k in keys)
    for (s0, s1), n in sorted(tr.items()):
        print(f"   {s0} -> {s1}: {n}")
    lost = [k for k in keys if B[k]["status"] == "PASS" and N[k]["status"] != "PASS"]
    print("PASS lost:", len(lost), lost[:10])
    tot = {"total": [0, 0], "one": [0, 0], "beyond": [0, 0]}
    worse = []
    reg = collections.defaultdict(lambda: [0, 0])
    for k in keys:
        for col, nm in (("total_px", "total"), ("one_level_px", "one"), ("beyond_px", "beyond")):
            tot[nm][0] += num(B[k], col); tot[nm][1] += num(N[k], col)
        if num(N[k], "total_px") > num(B[k], "total_px"):
            worse.append((k, num(B[k], "total_px"), num(N[k], "total_px")))
        rb, rn = regions(B[k]), regions(N[k])
        for name in set(rb) | set(rn):
            reg[name][0] += rb.get(name, (0, 0, 0))[0]; reg[name][1] += rn.get(name, (0, 0, 0))[0]
    for nm, (x, y) in tot.items():
        print(f"px {nm}: {x} -> {y}")
    print("rows with more total px:", len(worse))
    for k, x, y in sorted(worse, key=lambda w: w[2] - w[1], reverse=True)[:15]:
        print(f"   {k[0]} {k[1]}: {x} -> {y}")
    big = sorted(reg.items(), key=lambda kv: -kv[1][0])[:10]
    print("region px (base -> new):", "; ".join(f"{n} {x} -> {y}" for n, (x, y) in big))
    if a.out:
        with open(a.out, "w", encoding="utf-8", newline="") as f:
            w = csv.writer(f, delimiter="\t")
            w.writerow(["scenario", "checkpoint", "base_status", "new_status", "base_total", "new_total", "base_one", "new_one",
                        "base_beyond", "new_beyond"])
            for k in keys:
                w.writerow([k[0], k[1], B[k]["status"], N[k]["status"], num(B[k], "total_px"), num(N[k], "total_px"),
                            num(B[k], "one_level_px"), num(N[k], "one_level_px"), num(B[k], "beyond_px"), num(N[k], "beyond_px")])


if __name__ == "__main__":
    main()
