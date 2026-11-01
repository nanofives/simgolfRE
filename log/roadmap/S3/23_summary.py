"""S3.23: summary of the split census from <out>.d (raw vs split, per scenario): ~/bin/py -3.12 log/roadmap/S3/23_summary.py [--tsv out]"""
import json, pathlib, sys, collections
d = pathlib.Path(__file__).resolve().parent / "23_census_split_part.tsv.d"
raw, spl, dis = collections.Counter(), collections.Counter(), []
rows = []
for f in sorted(d.glob("*.json")):
    rows += json.loads(f.read_text())["rows"]
for r in rows:
    raw[r["status"]] += 1
    spl[r.get("split_status", "-")] += 1
    if r["status"] == "FAIL" and r.get("split_status") == "FAIL":
        dis.append((r["scenario"], r["checkpoint"], r.get("twod_px"), r.get("twod_beyond"), r.get("terrain_origin_px"), r.get("split_detail")))
print("scenarios", len(list(d.glob('*.json'))), "rows", len(rows))
print("raw", dict(raw)); print("split", dict(spl))
for x in dis: print("\t".join(map(str, x)))
