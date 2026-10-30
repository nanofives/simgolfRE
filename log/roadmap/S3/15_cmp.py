"""S3.15: compare a framegate TSV (this step's runs) with I14's census rows of the same scenarios, per checkpoint: status and the px
of every region, and a summary of the HUD top-bar regions.

    ~/bin/py -3.12 log/roadmap/S3/15_cmp.py <new.tsv> [--base log/roadmap/merges/I14/census.tsv]
"""
import argparse
import csv
import collections

TOP = ("hud_title", "hud_meters", "hud_message", "toolbar")


def regions(r):
    out = {}
    for p in (r.get("regions") or "").split(";"):
        if "=" in p:
            k, v = p.split("=", 1)
            out[k] = int(v.split("/")[0])
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("new")
    ap.add_argument("--base", default="log/roadmap/merges/I14/census.tsv")
    ap.add_argument("--quiet", action="store_true")
    a = ap.parse_args()
    base = {(r["scenario"], r["checkpoint"]): r for r in csv.DictReader(open(a.base, encoding="utf-8"), delimiter="\t")}
    new = list(csv.DictReader(open(a.new, encoding="utf-8"), delimiter="\t"))
    st = collections.Counter()
    top_b = top_n = 0
    lost = []
    for r in new:
        k = (r["scenario"], r["checkpoint"])
        b = base.get(k)
        if not b:
            st["no base row"] += 1
            continue
        rb, rn = regions(b), regions(r)
        tb, tn = sum(rb.get(x, 0) for x in TOP), sum(rn.get(x, 0) for x in TOP)
        top_b += tb
        top_n += tn
        st[f"{b['status']} -> {r['status']}"] += 1
        if b["status"] == "PASS" and r["status"] != "PASS":
            lost.append(k)
        worse = [x for x in rn if rn[x] > rb.get(x, 0)]
        if not a.quiet:
            print(f"{k[0]:20s} {k[1]:18s} {b['status']:11s} -> {r['status']:11s} top {tb:5d} -> {tn:5d}  "
                  + " ".join(f"{x}={rb.get(x, 0)}->{rn[x]}" for x in rn if rb.get(x, 0) or rn[x])
                  + (f"  WORSE {worse}" if worse else ""))
    print("statuses:", dict(st))
    print(f"HUD top-bar px: base {top_b} -> new {top_n}; rows losing PASS: {lost or 0}")


if __name__ == "__main__":
    main()
