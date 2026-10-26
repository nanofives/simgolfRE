"""I7: which change removed cov_a_keys' one `stuck` (inflated) live delivery? Run the live core over cov_a_keys' corpus trace with
S2.13's U-1254 control showEndOfYearScreen mode 6 (the end-of-year loop's per-pass cursor read left out) and without it.
Run from the mirror root: ~/bin/py -3.12 <this> [scenario]"""
import os
import sys

sys.path.insert(0, os.getcwd())
from port.test import lib as L, live as LV, boot as BOOT  # noqa: E402

scen = sys.argv[1] if len(sys.argv) > 1 else "cov_a_keys"
lib = L.load(os.environ.get("SG_PORTSIM_DLL"))
data = (BOOT.TRACES / f"{scen}.trace").read_bytes()
_, exp = BOOT.expected_records(scen)
for mode in (0, 6):
    lib.negctl("showEndOfYearScreen", mode)
    try:
        lv = LV.run_live(lib, scen, LV.KIND_TRACE, data, LV.frame_ra_of(data), len(exp))
    finally:
        lib.negctl("showEndOfYearScreen", 0)
    print(scen, "showEndOfYearScreen mode", mode, "records", lv.records, "of", len(exp), "inflated", lv.stats.get("inflated"),
          "violations", lv.stats.get("violations"), "code", lv.code)
