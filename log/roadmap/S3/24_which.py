"""S3.24: for a scenario's rows, the differing pixels of the port frame (frames_<tag>) against the reference and the dumped
drawLine records (dump_<scenario>.bin) whose box holds each pixel (the last such record = the last line drawn there)."""
import importlib.util, pathlib, sys
import numpy as np
ROOT = pathlib.Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "port/build/S3_24"))
from port.test import gfx as GX
spec = importlib.util.spec_from_file_location("p24", ROOT / "log/roadmap/S3/24_probe.py"); P = importlib.util.module_from_spec(spec); spec.loader.exec_module(P)
scen, tag = sys.argv[1], sys.argv[2]
cks = sys.argv[3:]
fr = np.load(ROOT / f"log/roadmap/S3/24_data/frames_{tag}/{scen}.npz")
recs = P.read_dump(ROOT / f"log/roadmap/S3/24_data/dump_{scen}.bin")
print(len(recs), "records in the dump")
for c in cks or fr.files:
    port = fr[c].astype(int); ref = GX.reference(scen, c).astype(int) >> 3
    dd = np.abs(port - ref).max(2)
    ys, xs = np.nonzero(dd)
    print(c, len(ys), "px")
    for y, x in zip(ys, xs):
        hits = [i for i, r in enumerate(recs) if r["box"][0] <= x <= r["box"][2] and r["box"][1] <= y <= r["box"][3]]
        last = recs[hits[-1]] if hits else None
        desc = "no line box" if not hits else "rec %d of %d hits: (%d,%d)-(%d,%d) w %d a %d c %x" % (hits[-1], len(hits), last["x1"], last["y1"], last["x2"], last["y2"], last["width"], last["alpha"], last["colour"])
        print("   (%d,%d) d %d port %s ref %s  %s" % (x, y, dd[y, x], port[y, x], ref[y, x], desc))
