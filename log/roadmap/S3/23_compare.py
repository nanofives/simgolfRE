"""S3.23: compare the split census (23_census_split_part.tsv.d) with f2dff003's census (20_out/S3_20/census.tsv, GlRaster) on the
raw columns (status, total_px, regions) and with the host-GL census (20_out/S3_20/census_auto.tsv) on the split verdict.

    ~/bin/py -3.12 log/roadmap/S3/23_compare.py [--out log/roadmap/S3/23_compare.tsv]
"""
import argparse
import collections
import csv
import json
import pathlib

HERE = pathlib.Path(__file__).resolve().parent
D = HERE / "23_census_split_part.tsv.d"


def load_tsv(p):
    with open(p, encoding="utf-8") as f:
        return {(r["scenario"], r["checkpoint"]): r for r in csv.DictReader(f, delimiter="\t")}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=str(HERE / "23_compare.tsv"))
    a = ap.parse_args()
    base = load_tsv(HERE / "20_out/S3_20/census.tsv")
    auto = load_tsv(HERE / "20_out/S3_20/census_auto.tsv")
    rows = []
    for f in sorted(D.glob("*.json")):
        rows += json.loads(f.read_text())["rows"]
    raw_eq = raw_ne = 0
    split_c, raw_c = collections.Counter(), collections.Counter()
    agree = collections.Counter()
    out = []
    for r in rows:
        k = (r["scenario"], r["checkpoint"])
        b, h = base.get(k), auto.get(k)
        raw_c[r["status"]] += 1
        split_c[r.get("split_status", "-")] += 1
        same = b is not None and all(str(r.get(c, "")) == b.get(c, "") for c in ("status", "total_px", "regions"))
        raw_eq += same
        raw_ne += not same
        hs = h["status"] if h else "-"
        agree[(r.get("split_status"), hs)] += 1
        if not same or r.get("split_status") != hs:
            out.append([k[0], k[1], r["status"], b["status"] if b else "-", "raw_same" if same else "RAW_DIFF",
                        r.get("split_status"), hs, r.get("twod_px", ""), r.get("twod_beyond", ""), r.get("terrain_origin_px", ""),
                        r.get("terrain_origin_beyond", ""), (h or {}).get("total_px", ""), (h or {}).get("regions", ""),
                        r.get("split_detail", "")])
    with open(a.out, "w", encoding="utf-8", newline="\n") as f:
        f.write("\t".join(["scenario", "checkpoint", "raw", "raw_f2dff003", "raw_check", "split", "hostgl_census", "twod_px",
                           "twod_beyond", "terrain_origin_px", "terrain_origin_beyond", "hostgl_total_px", "hostgl_regions",
                           "split_detail"]) + "\n")
        for x in out:
            f.write("\t".join(str(v) for v in x) + "\n")
    print("scenarios", len(list(D.glob("*.json"))), "rows", len(rows))
    print("raw", dict(raw_c), "| raw equal to f2dff003 census (status, total_px, regions):", raw_eq, "differ:", raw_ne)
    print("split", dict(split_c))
    print("split vs host-GL census:", {f"{s}/{h}": n for (s, h), n in sorted(agree.items(), key=str)})
    print("->", a.out, len(out), "rows listed")


if __name__ == "__main__":
    main()
