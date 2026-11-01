"""S3.24: the default census under the span rule (24_out/census_r2_b*.tsv, framegate --no-probe) against f2dff003's
(log/roadmap/S3/20_out/S3_20/census.tsv, S3.20's default GlRaster census: 111 PASS / 577 FAIL / 0 UNSUPPORTED / 10 NOT_RUN).

    ~/bin/py -3.12 log/roadmap/S3/24_census_cmp.py   -> 24_census_cmp.tsv + summary
"""
import collections, csv, glob, pathlib
HERE = pathlib.Path(__file__).resolve().parent
base = {(r["scenario"], r["checkpoint"]): r for r in csv.DictReader(open(HERE / "20_out/S3_20/census.tsv", encoding="utf-8"), delimiter="\t")}
new = {}
for p in sorted(glob.glob(str(HERE / "24_out/census_r2_b*.tsv"))):
    for r in csv.DictReader(open(p, encoding="utf-8"), delimiter="\t"):
        new[(r["scenario"], r["checkpoint"])] = r
def px(r):
    try:
        return int(r["total_px"])
    except (ValueError, KeyError):
        return None
st_b = collections.Counter(r["status"] for r in base.values())
st_n = collections.Counter(r["status"] for k, r in new.items())
lost, more, fewer, to_pass, missing = [], [], [], [], [k for k in base if k not in new]
with open(HERE / "24_census_cmp.tsv", "w", encoding="utf-8", newline="\n") as f:
    f.write("scenario\tcheckpoint\tstatus_f2dff003\tstatus_s324\tpx_f2dff003\tpx_s324\n")
    for k, b in base.items():
        n = new.get(k)
        if n is None:
            continue
        f.write(f"{k[0]}\t{k[1]}\t{b['status']}\t{n['status']}\t{b['total_px']}\t{n['total_px']}\n")
        if b["status"] == "PASS" and n["status"] != "PASS":
            lost.append(k)
        if b["status"] != "PASS" and n["status"] == "PASS":
            to_pass.append(k)
        pb, pn = px(b), px(n)
        if pb is not None and pn is not None:
            if pn > pb:
                more.append((k, pb, pn))
            elif pn < pb:
                fewer.append((k, pb, pn))
print("f2dff003:", dict(st_b), " rows", len(base))
print("S3.24   :", dict(st_n), " rows", len(new), " missing", len(missing))
print("PASS lost", len(lost), lost[:5])
print("rows to PASS", len(to_pass), to_pass)
print("rows with more px", len(more), more[:10])
print("rows with fewer px", len(fewer), " total px", sum(px(r) or 0 for r in base.values() if (r['scenario'], r['checkpoint']) in new), "->", sum(px(r) or 0 for r in new.values()))
