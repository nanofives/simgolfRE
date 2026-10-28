"""S3.5: scene_rec.py over a scenario list, one subprocess per scenario (a fresh scenario_refs import each time), then
scenario_refs.py check of each run against det_b. Prints `[N/M] <scenario> <copies> <check>` per scenario and writes
log/roadmap/S3/5_scene_rec_all.tsv.

    SIMGOLF_ALLOW_MULTI=1 C3FT_AUDIO=none ~/bin/py -3.12 log/roadmap/S3/5_tools/scene_rec_all.py <scenario>... --instance s3_5
"""
from __future__ import annotations

import argparse
import json
import pathlib
import subprocess
import sys
import time

ROOT = pathlib.Path(__file__).resolve().parents[4]
HERE = pathlib.Path(__file__).resolve().parent
OUT = HERE.parent / "5_scene_rec_all.tsv"
LOGS = HERE.parent / "5_scene_rec_logs"


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("scenarios", nargs="+")
    ap.add_argument("--instance", default="s3_5")
    a = ap.parse_args()
    LOGS.mkdir(exist_ok=True)
    rows = []
    if OUT.exists():
        rows = OUT.read_text().splitlines()[1:]
    done = {r.split("\t")[0] for r in rows}
    n = len(a.scenarios)
    for i, s in enumerate(a.scenarios, 1):
        if s in done:
            print(f"[{i}/{n}] {s} SKIP (already recorded)", flush=True)
            continue
        t0 = time.time()
        log = LOGS / f"{s}.log"
        with log.open("w") as fh:
            rc = subprocess.call([sys.executable, str(HERE / "scene_rec.py"), s, "--instance", a.instance], stdout=fh,
                                 stderr=subprocess.STDOUT, cwd=str(ROOT))
        chk = subprocess.run([sys.executable, str(ROOT / "re" / "tools" / "scenario_refs.py"), "check", "--scenario", s, "--run",
                              "s35_scene"], capture_output=True, text=True, cwd=str(ROOT))
        line = [ln for ln in chk.stdout.splitlines() if " vs det_b: " in ln]
        verdict = line[-1].split(": ", 1)[1] if line else ("ERROR " + (chk.stdout + chk.stderr).strip().splitlines()[-1][:120]
                                                         if (chk.stdout + chk.stderr).strip() else "ERROR")
        idx = ROOT / "port" / "corpus" / "scene" / s / "index.json"
        copies = 0
        if idx.exists():
            copies = sum(1 for m in json.loads(idx.read_text())["checkpoints"].values() if not m.get("none"))
        secs = round(time.time() - t0)
        status = "PASS" if rc == 0 and verdict.startswith("SAME") else "FAIL"
        rows.append(f"{s}\t{rc}\t{copies}\t{verdict}\t{secs}")
        OUT.write_text("scenario\trc\tcopies\tcheck\tseconds\n" + "\n".join(rows) + "\n")
        print(f"[{i}/{n}] {s} {status} copies={copies} {verdict} ({secs} s)", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
