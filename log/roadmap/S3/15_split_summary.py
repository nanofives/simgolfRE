"""S3.15: 15_split_rows.tsv (15_split_all.py) -> 15_split_summary.tsv: per census FAIL pattern (I14's px of hud_title / hud_meters /
hud_message / toolbar, the counts the step was given), the rows, scenarios and, per region, the px with GlRaster and how many are
terrain-origin (T + TB) and 2D-made (B). A TOTAL row per region at the end.

    ~/bin/py -3.12 log/roadmap/S3/15_split_summary.py
"""
import collections
import csv
import pathlib

HERE = pathlib.Path(__file__).resolve().parent
CENSUS = HERE.parent / "merges" / "I14" / "census.tsv"
REG = ("hud_title", "hud_meters", "hud_message", "toolbar", "bottom_panel", "view")
TOP = REG[:4]


def regions(r):
    out = {}
    for p in (r.get("regions") or "").split(";"):
        if "=" in p:
            k, v = p.split("=", 1)
            out[k] = int(v.split("/")[0])
    return out


def main():
    census = {(r["scenario"], r["checkpoint"]): r for r in csv.DictReader(open(CENSUS, encoding="utf-8"), delimiter="\t")}
    rows = list(csv.DictReader(open(HERE / "15_split_rows.tsv", encoding="utf-8"), delimiter="\t"))
    agg = collections.OrderedDict()
    tot = collections.Counter()
    for r in rows:
        c = census[(r["scenario"], r["checkpoint"])]
        rc = regions(c)
        pat = "/".join(str(rc.get(n, 0)) for n in TOP)
        a = agg.setdefault(pat, {"rows": 0, "scen": collections.Counter(), "meth": collections.Counter(), "n": collections.Counter()})
        a["rows"] += 1
        a["scen"][r["scenario"]] += 1
        a["meth"][r["method"]] += 1
        for n in REG:
            px, t, tb, b = (int(r[f"{n}_{k}"]) for k in ("px", "T", "TB", "B"))
            a["n"][n + "_px"] += px
            a["n"][n + "_terrain"] += t + tb
            a["n"][n + "_2d"] += b
            tot[n + "_px"] += px
            tot[n + "_terrain"] += t + tb
            tot[n + "_2d"] += b
    out = HERE / "15_split_summary.tsv"
    cols = ["pattern_title/meters/message/toolbar", "rows", "methods", "scenarios"] + \
        [f"{n}_{k}" for n in REG for k in ("px", "terrain", "2d")]
    with out.open("w", encoding="utf-8", newline="\n") as f:
        f.write("\t".join(cols) + "\n")
        for pat, a in sorted(agg.items(), key=lambda kv: -kv[1]["rows"]):
            sc = ",".join(f"{s}:{n}" for s, n in a["scen"].most_common())
            me = ",".join(f"{m}:{n}" for m, n in a["meth"].items())
            f.write("\t".join([pat, str(a["rows"]), me, sc] + [str(a["n"][c]) for c in cols[4:]]) + "\n")
        f.write("\t".join(["TOTAL", str(len(rows)), "", ""] + [str(tot[c]) for c in cols[4:]]) + "\n")
    print(f"{len(rows)} rows, {len(agg)} patterns -> {out}")
    for n in REG:
        px = tot[n + "_px"]
        print(f"  {n:12s} px {px:8d}  terrain {tot[n + '_terrain']:8d}  2D {tot[n + '_2d']:8d}")


if __name__ == "__main__":
    main()
