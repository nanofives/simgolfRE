"""S3.2 probe: the port's root surface around recorded frames of one scenario, against the original's (run from the S3_2 root)."""
import sys, pathlib, numpy as np
ROOT = pathlib.Path(__file__).resolve().parents[4] if "S3_2_root" not in str(pathlib.Path.cwd()) else pathlib.Path.cwd()
sys.path.insert(0, str(pathlib.Path.cwd()))
from port.test import lib as L, terrainlive as TL
label, scen = sys.argv[1], sys.argv[2]
neg = (sys.argv[3], int(sys.argv[4])) if len(sys.argv) > 4 else None
rec = TL.recorded_frames(label)
lib = L.load()
last = max(rec, key=lambda c: rec[c]["meta"]["frame"])
frames = sorted({e["meta"]["frame"] for e in rec.values()})
got = TL.port_frames(lib, scen, frames, last, negctl=neg)
print("boot", got["boot"].status, got["boot"].detail[:100], "passes", got["passes"], "frames", got["frames"])
for c, e in sorted(rec.items(), key=lambda kv: kv[1]["meta"]["frame"]):
    k = e["meta"]["frame"]
    for what in ["entry"] + [f"pass{j}_{w}" for j in range(8) for w in ("pre", "post")] + ["exit"]:
        if what not in e:
            continue
        pw = what.replace("pass", "p")
        p = got.get((k, pw))
        if p is None:
            print(c, k, what, "port: none"); continue
        r = TL.g2(e[what], p, e["entry"] if what != "entry" else None)
        print(c, k, what, r)
for ln in got["log"].splitlines():
    if ln.startswith(("handled\tterrain", "unsupported\tterrain", "first")) and "terrain" in ln:
        print(ln)
