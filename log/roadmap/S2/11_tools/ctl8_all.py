"""S2.11: reports control 8 on every checkpoint test_gfx_reports.py lists for it (judged pixels changed / differing).
usage (a mirror root, SG_PORTSIM_DLL set): ~/bin/py -3.12 <abs>/log/roadmap/S2/11_tools/ctl8_all.py"""
import pathlib, sys
sys.path.insert(0, str(pathlib.Path.cwd()))
from port.test import lib as L
from port.test import test_gfx_reports as T
lib = L.load()
if not lib.exe_loaded():
    lib.exe_load()
held = T.held_masks(T.JUDGED_CHECKPOINTS, lib)
base = T.judge(T.JUDGED_CHECKPOINTS, lib, held=held)
ctl = T.judge(T.JUDGED_CHECKPOINTS, lib, negctl=(T.FAMILY, 8), held=held)
for c in T.FRAME_CONTROLS[8]:
    print(c, "plain", base[c]["judged"], base[c]["diff"], "control", ctl[c]["judged"], ctl[c]["diff"])
