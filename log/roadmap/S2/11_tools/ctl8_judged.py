"""S2.11: which judged pixels of cov_r_s2_screens hole_stats reports control 8 changes (test_gfx_reports.py's measure).
usage (a mirror root, SG_PORTSIM_DLL set): ~/bin/py -3.12 <abs>/log/roadmap/S2/11_tools/ctl8_judged.py"""
import pathlib, sys
import numpy as np
sys.path.insert(0, str(pathlib.Path.cwd()))
from port.test import gfx as GX
from port.test import lib as L
from port.test import test_gfx_reports as T
lib = L.load()
if not lib.exe_loaded():
    lib.exe_load()
c = "hole_stats"
held = T.held_masks([c], lib)[c]
a = GX.render(T.SCENARIO, [c], lib).frames[c]
b = GX.render(T.SCENARIO, [c], lib, negctl=8, negctl_name=T.FAMILY).frames[c]
ref = GX.reference(T.SCENARIO, c)
j = ~held
ch = j & (a != b).any(2)
ys, xs = np.where(ch)
print("judged", int(j.sum()), "judged changed by control 8", int(ch.sum()), (xs.min(), ys.min(), xs.max(), ys.max()) if len(xs) else "")
print("judged differing from the reference: plain", int((j & (a != ref).any(2)).sum()), "control", int((j & (b != ref).any(2)).sum()))
for y, x in list(zip(ys, xs))[:16]:
    print((int(x), int(y)), "plain", tuple(int(v) for v in a[y, x]), "control", tuple(int(v) for v in b[y, x]), "ref", tuple(int(v) for v in ref[y, x]))
