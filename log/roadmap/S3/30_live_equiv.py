"""S3.30: the live equivalence gate one process per scenario, one at a time (port.test.live.run_parallel with jobs 1; `live equiv
--all --jobs 1` runs every scenario in ONE process, which died silently after 28 scenarios on 2026-11-02, rc 1 with no traceback).
Run from the tree (cwd port/build/S3_30) with SG_PORTSIM_DLL set; writes <out> in `live equiv --out`'s format.

    ~/bin/py -3.12 log/roadmap/S3/30_live_equiv.py <out.tsv> [--backend win32]
"""
import json
import os
import pathlib
import sys

T = pathlib.Path.cwd()
sys.path.insert(0, str(T))
if "--backend" in sys.argv:
    os.environ["SG_BACKEND"] = sys.argv[sys.argv.index("--backend") + 1]
from port.test import boot as BOOT  # noqa: E402
from port.test import live as LV  # noqa: E402


def main():
    out = pathlib.Path(sys.argv[1])
    rows = LV.run_parallel("equiv", BOOT.scenarios(), 1)
    keys = []
    for r in rows:
        for k in r:
            if k not in keys:
                keys.append(k)
    with out.open("w", encoding="utf-8") as f:
        f.write("\t".join(keys) + "\n")
        for r in sorted(rows, key=lambda r: str(r.get(keys[0]))):
            f.write("\t".join(str(r.get(k, "")).replace("\t", " ") for k in keys) + "\n")
    counts = {}
    for r in rows:
        counts[r["status"]] = counts.get(r["status"], 0) + 1
    print(json.dumps({"counts": counts}))


if __name__ == "__main__":
    main()
