"""S7 lead: the boot gate (port.test.boot) with switch sound.golfSfxAlias at 0. One process per scenario, as boot.py's
--jobs path (a long scenario holds its whole trace); `--one <scenario>` is that process. TSV to argv[1].
Usage (from port/build/S7_12_root): ~/bin/py -3.12 <this> <out.tsv> [--jobs 3]"""
import concurrent.futures as cf
import json
import pathlib
import subprocess
import sys

ROOT = pathlib.Path.cwd()
sys.path.insert(0, str(ROOT))
from port.test import boot as B  # noqa: E402
from port.test import lib as L  # noqa: E402

if sys.argv[1] == "--one":
    lib = L.load()
    lib.defect("sound.golfSfxAlias", False)
    assert lib.defects()["sound.golfSfxAlias"]["on"] == 0
    r = B.run(sys.argv[2], lib)
    print(json.dumps({"scenario": r.scenario, "status": r.status, "records": r.records, "expected": r.expected,
                      "detail": r.detail[:300]}))
    sys.exit(0)

out, jobs = sys.argv[1], int(sys.argv[sys.argv.index("--jobs") + 1]) if "--jobs" in sys.argv else 3
names = B.scenarios()
rows, counts = [], {}
with cf.ThreadPoolExecutor(jobs) as ex:
    futs = {ex.submit(subprocess.run, [sys.executable, __file__, "--one", s], capture_output=True, text=True,
                      cwd=str(ROOT)): s for s in names}
    for i, f in enumerate(cf.as_completed(futs), 1):
        s, lines = futs[f], f.result().stdout.strip().splitlines()
        row = json.loads(lines[-1]) if lines and lines[-1].startswith("{") else {
            "scenario": s, "status": "ERROR", "records": 0, "expected": 0, "detail": f.result().stderr[-300:]}
        rows.append(row)
        counts[row["status"]] = counts.get(row["status"], 0) + 1
        print(f"[{i}/{len(names)}] {s} {row['status']} {row['records']}/{row['expected']}", flush=True)
with open(out, "w", encoding="utf-8") as fh:
    fh.write("scenario\tstatus\trecords\texpected\tdetail\n")
    for r in sorted(rows, key=lambda r: r["scenario"]):
        fh.write(f"{r['scenario']}\t{r['status']}\t{r['records']}\t{r['expected']}\t{r['detail']}\n")
print(json.dumps({"switch": "sound.golfSfxAlias=0", "counts": counts}), flush=True)
