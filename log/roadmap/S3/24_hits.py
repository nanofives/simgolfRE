"""S3.24: replay through the real GL the dumped drawLine record that last covered each differing pixel of the given rows, and
print per pixel: port post, real GL post (same pre, same call), reference frame.

    ~/bin/py -3.12 log/roadmap/S3/24_hits.py <tag> <scenario>:<checkpoint>[,<checkpoint>] ...   (glreplay: heavy lock)
"""
import importlib.util, pathlib, sys
import numpy as np
ROOT = pathlib.Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "port/build/S3_24"))
from port.test import gfx as GX
spec = importlib.util.spec_from_file_location("p24", ROOT / "log/roadmap/S3/24_probe.py"); P = importlib.util.module_from_spec(spec); spec.loader.exec_module(P)
tag = sys.argv[1]
jobs = []
for arg in sys.argv[2:]:
    scen, cks = arg.split(":")
    fr = np.load(ROOT / f"log/roadmap/S3/24_data/frames_{tag}/{scen}.npz")
    recs = P.read_dump(ROOT / f"log/roadmap/S3/24_data/dump_{scen}.bin")
    for c in cks.split(","):
        port = fr[c].astype(int); ref = GX.reference(scen, c).astype(int) >> 3
        dd = np.abs(port - ref).max(2)
        for y, x in zip(*np.nonzero(dd)):
            hits = [i for i, r in enumerate(recs) if r["box"][0] <= x <= r["box"][2] and r["box"][1] <= y <= r["box"][3]]
            if hits:
                jobs.append((scen, c, int(x), int(y), recs[hits[-1]], port[y, x], ref[y, x]))
uniq = {}
for j in jobs:
    uniq.setdefault(id(j[4]), j[4])
lst = list(uniq.values())
real = P.run_real(lst, "s324hits")
idx = {id(r): k for k, r in enumerate(lst)}
same_ref = 0
for scen, c, x, y, r, pv, rv in jobs:
    k = idx[id(r)]
    x0, y0 = r["box"][:2]
    rp = int(real[k][y - y0, x - x0]); pp = int(r["post"][y - y0, x - x0]); pre = int(r["pre"][y - y0, x - x0])
    f = lambda v: [(v >> 10) & 31, (v >> 5) & 31, v & 31]
    print(scen, c, (x, y), "pre", f(pre), "port_post", f(pp), "real_post", f(rp), "frame_port", list(pv), "ref", list(rv),
          "REAL==REF" if f(rp) == list(rv) else "")
    same_ref += f(rp) == list(rv)
print(len(jobs), "pixels;", same_ref, "where the real GL on the port's call gives the reference value")
