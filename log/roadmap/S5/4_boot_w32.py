"""S5.4: the boot gate on the Win32 backend (private build port/build/S5_4_w32), one scenario per Win32 runner, sequential.
Same row format as port.test.boot's TSV. Needs S5_4_PATCH=1 PYTHONPATH=log/roadmap/S5/4_site (patched selection) and
SG_TEXT_METRICS=freetype|gdi.

    ~/bin/py -3.12 log/roadmap/S5/4_boot_w32.py --out log/roadmap/S5/4_boot_gate_ft_w32.tsv
"""
import argparse
import pathlib
import sys

from port.test import boot
from port.test.widths_check import load_win32

ap = argparse.ArgumentParser()
ap.add_argument("--w32-dir", default="port/build/S5_4_w32")
ap.add_argument("--out", required=True)
a = ap.parse_args()
names = boot.scenarios()
rows = []
for i, s in enumerate(names, 1):
    lib = load_win32(pathlib.Path(a.w32_dir))
    try:
        r = boot.run(s, lib)
    finally:
        try:
            lib.runner.proc.kill()
        except Exception:
            pass
    rows.append(r)
    print(f"[{i}/{len(names)}] {s} {r.status} {r.records}/{r.expected} {r.detail[:160]}", flush=True)
with open(a.out, "w", encoding="utf-8") as f:
    f.write("scenario\tstatus\trecords\texpected\tdetail\trun_detail\n")
    for r in sorted(rows, key=lambda r: r.scenario):
        f.write("\t".join(str(v).replace("\t", " ") for v in (r.scenario, r.status, r.records, r.expected, r.detail, r.run_detail)) + "\n")
print({k: sum(1 for r in rows if r.status == k) for k in sorted({r.status for r in rows})})
