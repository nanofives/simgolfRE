"""S2.11: reports control 8 on cov_r_s2_screens hole_stats, plain and with the control, with the DLL in SG_PORTSIM_DLL.
usage (a mirror root): ~/bin/py -3.12 <abs>/log/roadmap/S2/11_tools/ctl8.py <out prefix>"""
import pathlib, sys
import numpy as np
from PIL import Image
sys.path.insert(0, str(pathlib.Path.cwd()))
from port.test import gfx as GX
from port.test import lib as L
lib = L.load()
if not lib.exe_loaded():
    lib.exe_load()
cps = ["rep_roster", "bulletin", "hole_stats"]
a = GX.render("cov_r_s2_screens", cps, lib)
b = GX.render("cov_r_s2_screens", cps, lib, negctl=8, negctl_name="gfx.family.reports")
for c in cps:
    if a.frames.get(c) is None:
        print(c, "no snapshot"); continue
    d = (a.frames[c] != b.frames[c]).any(2)
    ys, xs = np.where(d)
    print(c, "pixels changed by control 8:", int(d.sum()), (xs.min(), ys.min(), xs.max(), ys.max()) if len(xs) else "")
    Image.fromarray(a.frames[c]).save(sys.argv[1] + f"_{c}.png"); Image.fromarray(b.frames[c]).save(sys.argv[1] + f"_{c}_ctl8.png")
    np.save(sys.argv[1] + f"_{c}_ctl8diff.npy", d)
