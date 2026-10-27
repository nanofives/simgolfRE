"""S7.9: where (frame record) each live inflated dispatch and each replay `misaligned` delivery happens, by bisecting max_records
(both counts grow with the records run). x64 only. Run from the mirror root:  ~/bin/py -3.12 <this> scen"""
import os
import re
import sys

sys.path.insert(0, os.getcwd())
from port.test import boot as BOOT, lib as L, live as LV  # noqa: E402

scen = sys.argv[1]
lib = L.load()
data = (BOOT.TRACES / f"{scen}.trace").read_bytes()
_, exp = BOOT.expected_records(scen)
N = len(exp)
full = LV.run_live(lib, scen, LV.KIND_TRACE, data, LV.frame_ra_of(data), N)
RE = re.compile(r"(\d+) at another cursor count")


def inflated(k):
    return int(LV.run_live(lib, scen, LV.KIND_TRACE, data, LV.frame_ra_of(data), k).stats.get("inflated", 0))


def misaligned(k):
    m = RE.search(LV.run_boot(lib, scen, full.trace, k)[4] or "")
    return int(m.group(1)) if m else 0


def steps(f, total):
    out, lo = [], 1
    for target in range(1, total + 1):
        a, b = lo, N
        while a < b:
            mid = (a + b) // 2
            if f(mid) >= target:
                b = mid
            else:
                a = mid + 1
        out.append(a)
        lo = a
    return out


ni = int(full.stats.get("inflated", 0))
nm = misaligned(full.records)
print(scen, "records", N, "inflated", ni, "replay misaligned", nm, flush=True)
print("inflated reaches n at max_records:", steps(inflated, ni), flush=True)
print("replay misaligned reaches n at max_records:", steps(misaligned, nm), flush=True)
