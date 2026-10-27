"""S7.9: what live.recorder control 10 changes. For each inflated dispatch n of a scenario: the recording's bytes against the
plain run's, the replay (sg_run_boot over the recording) against the live run: code, records, first difference, and the replay's
gfx-free state at the end (sim hashes of the last record). Run from the mirror root:  ~/bin/py -3.12 <this> scen [n...]"""
import hashlib
import os
import sys

sys.path.insert(0, os.getcwd())
from port.test import boot as BOOT, lib as L, live as LV  # noqa: E402

scen = sys.argv[1]
ns = [int(x) for x in sys.argv[2:]] or [0]
lib = L.load()
data = (BOOT.TRACES / f"{scen}.trace").read_bytes()
_, exp = BOOT.expected_records(scen)
names = [n for n, _, _ in lib.objects("sim")]


def live(mode):
    if mode:
        lib.negctl(LV.NEGCTL, mode)
    try:
        return LV.run_live(lib, scen, LV.KIND_TRACE, data, LV.frame_ra_of(data), len(exp))
    finally:
        lib.negctl(LV.NEGCTL, 0)


base = live(0)
print("plain", "records", base.records, "inflated", base.stats.get("inflated"), "trace", len(base.trace),
      hashlib.sha256(base.trace).hexdigest()[:16], flush=True)
for n in ns:
    lv = live(10 + 16 * n)
    print(f"n={n} records {lv.records} code {lv.code} inflated {lv.stats.get('inflated')} trace {len(lv.trace)} "
          f"{hashlib.sha256(lv.trace).hexdigest()[:16]} same_as_plain {lv.trace == base.trace}", flush=True)
    code_r, nr, tsr, hsr, rd = LV.run_boot(lib, scen, lv.trace, lv.records)
    d = LV.first_diff(tsr, hsr, lv.T, lv.hashes, names)
    print(f"   replay code {code_r} records {nr} first_diff {d} detail {str(rd)[:200]}", flush=True)
