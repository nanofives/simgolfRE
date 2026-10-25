"""S3.2: the port's course view on the Tropical study scenario (trace from the alias run) against the original's recorded frames."""
import pathlib, sys
sys.path.insert(0, str(pathlib.Path.cwd()))
from port.test import lib as L, terrainlive as TL
lab = "s32t_cov_b_s34_hawaii"
rec = TL.recorded_frames(lab)
tr = TL.ROOT / "log/p5/terrain" / lab / "trace.trace"
got = TL.port_frames(L.load(), "cov_b_s34_hawaii", sorted({e["meta"]["frame"] for e in rec.values()}), "", trace=tr)
print("run", got["boot"].status, got["boot"].detail, "passes", got["passes"], "frames", got["frames"])
for c, e in rec.items():
    for w in sorted(k for k in e if k not in ("meta", "entry")):
        p = got.get((e["meta"]["frame"], w.replace("pass", "p")))
        print(c, e["meta"]["frame"], w, None if p is None else TL.g2(e[w], p, e["entry"]))
