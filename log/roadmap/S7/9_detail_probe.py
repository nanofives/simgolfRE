"""S7.9: sg_run_boot's input accounting (run_frames_detail) over the live recording, plain and with live.recorder control 10.
Run from the mirror root: ~/bin/py -3.12 <this> scen [n...]"""
import os, sys
sys.path.insert(0, os.getcwd())
from port.test import boot as BOOT, lib as L, live as LV  # noqa: E402
scen = sys.argv[1]
lib = L.load()
data = (BOOT.TRACES / f"{scen}.trace").read_bytes()
_, exp = BOOT.expected_records(scen)
print("corpus  :", LV.run_boot(lib, scen, data, len(exp))[4], flush=True)
for mode in [0] + [10 + 16 * int(n) for n in sys.argv[2:]]:
    if mode:
        lib.negctl(LV.NEGCTL, mode)
    try:
        lv = LV.run_live(lib, scen, LV.KIND_TRACE, data, LV.frame_ra_of(data), len(exp))
    finally:
        lib.negctl(LV.NEGCTL, 0)
    print(f"mode {mode}:", LV.run_boot(lib, scen, lv.trace, lv.records)[4], flush=True)
