"""S3.14: frame-period columns of 14_pacing.tsv from the sessions the run kept in %TEMP%.

14_pacing.py's `kept` regex matched nothing, so it copied no session and left the stats columns empty (None). The 20 sessions it
ran (S7_5_KEEP=1) are still in %TEMP% as s7_5_* created 2026-10-28 03:05..03:19. They are mapped to the runs by creation order
(14_pacing.py runs c00, d00, c01, d01, ... one at a time); the mapping is checked by the trees' present counts (S3's course view
presents every in-game frame, d66aa755's idle view does not, notes section 3.1): presents of every c session must exceed every d.
Writes log/roadmap/S3/14_pacing_parsed.tsv and copies waits.tsv / frames.tsv / session.json to log/roadmap/S3/14_pacing/<v>_<k>/.
Usage: ~/bin/py -3.12 log/roadmap/S3/14_pacing_parse.py
"""
import importlib.util
import json
import os
import pathlib
import shutil
import statistics
import sys

HERE = pathlib.Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("p14", HERE / "14_pacing.py")
p14 = importlib.util.module_from_spec(spec)
spec.loader.exec_module(p14)

tmp = pathlib.Path(os.environ["TEMP"])
ds = sorted((d for d in tmp.glob("s7_5_*") if d.is_dir() and (d / "rec" / "waits.tsv").exists()),
            key=lambda d: d.stat().st_ctime)
import datetime as dt
ds = [d for d in ds if dt.datetime.fromtimestamp(d.stat().st_ctime) >= dt.datetime(2026, 10, 28, 3, 4)
      and dt.datetime.fromtimestamp(d.stat().st_ctime) <= dt.datetime(2026, 10, 28, 3, 20)]
if len(ds) != 20:
    sys.exit(f"expected 20 sessions, found {len(ds)}")
rows = []
for i, d in enumerate(ds):
    v, k = ("c", "d")[i % 2], i // 2
    rec = d / "rec"
    s = p14.stats(rec)
    sj = json.loads((rec / "session.json").read_text())
    dst = HERE / "14_pacing" / f"{v}_{k:02d}"
    dst.mkdir(parents=True, exist_ok=True)
    for f in ("waits.tsv", "frames.tsv", "session.json"):
        shutil.copyfile(rec / f, dst / f)
    rows.append({"variant": v, "k": k, "session": d.name, "presents": sj.get("session", {}).get("presents"),
                 "records": sj.get("session", {}).get("records"), **s, "over_nopass": len(s["over_nopass"]),
                 "over_nopass_list": s["over_nopass"]})
pc = [r["presents"] for r in rows if r["variant"] == "c"]
pd = [r["presents"] for r in rows if r["variant"] == "d"]
mapping_ok = min(pc) > max(pd)
cols = ["variant", "k", "session", "presents", "records", "fits", "fit_med", "fit_max", "within", "over_nopass", "np_med",
        "np_p99", "np_max", "over_nopass_list"]
with open(HERE / "14_pacing_parsed.tsv", "w", newline="\n") as fh:
    fh.write("\t".join(cols) + "\n")
    for r in rows:
        fh.write("\t".join(str(r[c]) for c in cols) + "\n")
for r in rows:
    print(f"[{r['variant']}{r['k']:02d}] {r['session']} presents {r['presents']} fits {r['fits']} fit_med {r['fit_med']} "
          f"fit_max {r['fit_max']} within {r['within']} over_nopass {r['over_nopass_list']} np_med {r['np_med']} np_max {r['np_max']}")
for v in "cd":
    rs = [r for r in rows if r["variant"] == v]
    print(f"{v}: fit_med median {statistics.median(r['fit_med'] for r in rs)} fit_max max {max(r['fit_max'] for r in rs)} "
          f"within min {min(r['within'] for r in rs)} over_nopass total {sum(r['over_nopass'] for r in rs)} "
          f"np_med median {statistics.median(r['np_med'] for r in rs)} np_max max {max(r['np_max'] for r in rs)}")
print("mapping by present count:", "OK" if mapping_ok else f"NOT CONFIRMED (c {pc}, d {pd})")
