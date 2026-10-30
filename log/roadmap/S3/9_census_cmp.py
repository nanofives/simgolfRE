"""S3.9: the combined wave 7 census against I14's (= I15's = plain 37e58e96's: 698 rows, 0 differ) and the steps' own measurements.

    ~/bin/py -3.12 log/roadmap/S3/9_census_cmp.py --new <combined census.tsv> [--step LABEL=<tsv>[,<tsv>...]]... [--out re/roadmap/S3_census.tsv]

For every (scenario, checkpoint) of I14: I14's status / total / beyond px, the combined tree's, and each step's value where the step
measured the row (a step file listed later wins for a row measured twice). Checks:
  LOST     an I14 PASS row that is not PASS on the combined tree
  OVER     combined total_px (or beyond_px) above the smallest value any step measured for the row (or above I14's when no step did)
  CHANGED  status or px differs from I14; `explained_by` = the steps whose measurement differs from I14 for the row (empty = UNEXPLAINED)
Prints the counts and every changed / failing row; writes the table."""
import argparse
import csv
import pathlib

ROOT = pathlib.Path(__file__).resolve().parents[3]
I14 = ROOT / "log/roadmap/merges/I14/census.tsv"


def rd(p):
    with open(p, encoding="utf-8", newline="") as f:
        return {(r["scenario"], r["checkpoint"]): r for r in csv.DictReader(f, delimiter="\t")}


def px(r, k):
    try:
        return int(r.get(k) or 0)
    except ValueError:
        return 0


def sig(r):
    return (r["status"], px(r, "total_px"), px(r, "beyond_px"))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--i14", default=str(I14))
    ap.add_argument("--new", required=True)
    ap.add_argument("--step", action="append", default=[])
    ap.add_argument("--out", default=str(ROOT / "re/roadmap/S3_census.tsv"))
    a = ap.parse_args()
    base, new = rd(a.i14), rd(a.new)
    steps = []
    for s in a.step:
        lab, files = s.split("=", 1)
        rows = {}
        for f in files.split(","):
            rows.update(rd(f))
        steps.append((lab, rows))
    labs = [l for l, _ in steps]
    out, cnt = [], {"rows": 0, "LOST": 0, "OVER": 0, "CHANGED": 0, "UNEXPLAINED": 0, "MISSING": 0}
    stat_b, stat_n = {}, {}
    for k, b in base.items():
        cnt["rows"] += 1
        n = new.get(k)
        stat_b[b["status"]] = stat_b.get(b["status"], 0) + 1
        if n is None:
            cnt["MISSING"] += 1
            out.append({"scenario": k[0], "checkpoint": k[1], "i14_status": b["status"], "verdict": "MISSING"})
            continue
        stat_n[n["status"]] = stat_n.get(n["status"], 0) + 1
        meas = {l: rows[k] for l, rows in steps if k in rows}
        lim_t = min([px(r, "total_px") for r in meas.values()] + [px(b, "total_px")])
        lim_b = min([px(r, "beyond_px") for r in meas.values()] + [px(b, "beyond_px")])
        expl = [l for l, r in meas.items() if sig(r) != sig(b)]
        v = []
        if b["status"] == "PASS" and n["status"] != "PASS":
            v.append("LOST")
        if px(n, "total_px") > lim_t or px(n, "beyond_px") > lim_b:
            v.append("OVER")
        if sig(n) != sig(b):
            v.append("CHANGED")
            if not expl:
                v.append("UNEXPLAINED")
        for x in v:
            cnt[x] += 1
        row = {"scenario": k[0], "checkpoint": k[1],
               "i14_status": b["status"], "i14_total_px": px(b, "total_px"), "i14_beyond_px": px(b, "beyond_px"),
               "status": n["status"], "total_px": px(n, "total_px"), "one_level_px": px(n, "one_level_px"),
               "beyond_px": px(n, "beyond_px"), "first_unsupported": n.get("first_unsupported", ""),
               "step_limit_total_px": lim_t, "step_limit_beyond_px": lim_b,
               "explained_by": ",".join(expl), "verdict": "+".join(v) or "same"}
        for l in labs:
            r = meas.get(l)
            row[f"{l}_px"] = f"{r['status']}:{px(r, 'total_px')}/{px(r, 'beyond_px')}" if r else ""
        out.append(row)
    cols = ["scenario", "checkpoint", "i14_status", "i14_total_px", "i14_beyond_px", "status", "total_px", "one_level_px",
            "beyond_px", "first_unsupported", "step_limit_total_px", "step_limit_beyond_px", "explained_by", "verdict"] + \
        [f"{l}_px" for l in labs]
    with open(a.out, "w", encoding="utf-8", newline="") as f:
        w = csv.DictWriter(f, cols, delimiter="\t", extrasaction="ignore", lineterminator="\n")
        w.writeheader()
        w.writerows(out)
    extra = sorted(set(new) - set(base))
    print("I14", stat_b, "\ncombined", stat_n, "\n", cnt, "rows only in the combined census:", len(extra))
    for r in out:
        if r["verdict"] != "same":
            print(r["verdict"], r["scenario"], r["checkpoint"], r.get("i14_status"), r.get("i14_total_px"), "/",
                  r.get("i14_beyond_px"), "->", r.get("status"), r.get("total_px"), "/", r.get("beyond_px"),
                  "explained_by=" + r.get("explained_by", ""))
    print("->", a.out)


if __name__ == "__main__":
    main()
