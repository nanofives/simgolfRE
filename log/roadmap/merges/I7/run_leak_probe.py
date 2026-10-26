"""I7: which run keeps memory in the Win32 runner: sg_run_boot (live.run_boot, as boot.py runs it) or the live core
(live.run_live over the corpus trace)? k runs of each on one scenario, the runner's private bytes after each.
Run from a mirror root: SG_BACKEND=win32 SG_PORTSIM_W32_DIR=<root>/port/build/Win32 ~/bin/py -3.12 <this> [scenario] [k]"""
import os
import sys

sys.path.insert(0, os.getcwd())
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from port.test import lib as L, live as LV, boot as BOOT  # noqa: E402
from runner_res_probe import res  # noqa: E402

scen = sys.argv[1] if len(sys.argv) > 1 else "cov_r_bio"
k = int(sys.argv[2]) if len(sys.argv) > 2 else 4
lib = L.load()
data = (BOOT.TRACES / f"{scen}.trace").read_bytes()
_, exp = BOOT.expected_records(scen)
pid = lib.runner.proc.pid
print(scen, "start", res(pid), flush=True)
for what in ("boot", "live"):
    for i in range(k):
        if what == "boot":
            r = LV.run_boot(lib, scen, data, len(exp))[0]
        else:
            r = LV.run_live(lib, scen, LV.KIND_TRACE, data, LV.frame_ra_of(data), len(exp)).code
        print(what, i, "code", r, res(pid), flush=True)
