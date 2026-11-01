"""S3.26: the default census (GlRaster) of port/build/S3_26 (26_out/census_b*.tsv) against f2dff003's
(20_out/S3_20/census_raster.tsv): counts, PASS lost / gained, rows with more or fewer differing px.

    ~/bin/py -3.12 log/roadmap/S3/26_census_cmp.py [--out log/roadmap/S3/26_out/census_cmp.tsv]
"""
import argparse
import collections
import csv
import pathlib

HERE = pathlib.Path(__file__).resolve().parent


def rows(paths):
    out = {}
    for p in paths:
        for r in csv.DictReader(open(p, encoding="utf-8"), delimiter="\t"):
            out[(r["scenario"], r["checkpoint"])] = r
    return out


def px(r):
    try:
        return int(r.get("total_px") or -1)
    except ValueError:
        return -1


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=str(HERE / "26_out" / "census_cmp.tsv"))
    a = ap.parse_args()
    base = rows([HERE / "20_out" / "S3_20" / "census_raster.tsv"])
    new = rows(sorted((HERE / "26_out").glob("census_b*.tsv")))
    cnt = lambda d: dict(collections.Counter(r["status"] for r in d.values()))
    print("base", len(base), cnt(base))
    print("new ", len(new), cnt(new))
    lost, gained, worse, better, missing = [], [], [], [], []
    lines = ["scenario\tcheckpoint\tbase_status\tbase_px\tnew_status\tnew_px"]
    for k, b in sorted(base.items()):
        n = new.get(k)
        if n is None:
            missing.append(k)
            continue
        if b["status"] == "PASS" and n["status"] != "PASS":
            lost.append(k)
        if b["status"] != "PASS" and n["status"] == "PASS":
            gained.append(k)
        if px(n) > px(b):
            worse.append((k, px(b), px(n)))
        elif px(n) < px(b):
            better.append((k, px(b), px(n)))
        if b["status"] != n["status"] or px(b) != px(n):
            lines.append(f"{k[0]}\t{k[1]}\t{b['status']}\t{px(b)}\t{n['status']}\t{px(n)}")
    pathlib.Path(a.out).write_text("\n".join(lines) + "\n", encoding="utf-8")
    print("PASS lost", len(lost), lost)
    print("PASS gained", len(gained), gained)
    print("more px", len(worse), worse[:20])
    print("fewer px", len(better), better[:40])
    print("missing", len(missing), missing[:10])


if __name__ == "__main__":
    main()
