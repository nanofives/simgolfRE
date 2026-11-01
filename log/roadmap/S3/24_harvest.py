"""S3.24: labelled tie decisions from the census rows. For every row frame (frames_r6: every tie drawn) and every pixel covered by
exactly one distinct dumped line whose |o| equals N/2 (1e-9): label drawn = (port frame == reference there). Output npz-free
pickle log/roadmap/S3/24_data/ties_census.pkl: list of (record dict without pixels, X, Y, N, xmaj, drawn)."""
import collections, importlib.util, pathlib, pickle, sys
import numpy as np
ROOT = pathlib.Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "port/build/S3_24"))
from port.test import gfx as GX
spec = importlib.util.spec_from_file_location("p24", ROOT / "log/roadmap/S3/24_probe.py"); P = importlib.util.module_from_spec(spec); spec.loader.exec_module(P)
out = []
for scen in sys.argv[1:]:
    on = np.load(ROOT / f"log/roadmap/S3/24_data/frames_r6/{scen}.npz")
    recs, seen = [], set()
    for r in P.read_dump(ROOT / f"log/roadmap/S3/24_data/dump_{scen}.bin"):
        k = (r["x1"], r["y1"], r["x2"], r["y2"], r["width"], r["lw"])
        if k not in seen:
            seen.add(k); recs.append(r)
    # coverage maps per line (S3.8 support) and tie masks
    cover = collections.defaultdict(list)
    ties = {}
    for i, r in enumerate(recs):
        cov, g = P.coverage(r, "s38")
        if g is None:
            continue
        dx, dy = g["bx"] - g["ax"], g["by"] - g["ay"]
        major = max(abs(dx), abs(dy))
        if major == 0:
            continue
        N = np.floor(2 * g["half"] * g["L"] / major) + 2
        o, xmaj = P.minor_offset(g)
        x0, y0 = r["box"][:2]
        for yy, xx in zip(*np.nonzero(cov > 0.02)):
            cover[(xx + x0, yy + y0)].append(i)
            if abs(abs(o[yy, xx]) - N / 2) < 1e-9:
                ties[(i, xx + x0, yy + y0)] = (N, xmaj)
    for c in on.files:
        ref = GX.reference(scen, c).astype(int) >> 3
        a = on[c].astype(int)
        for (i, x, y), (N, xmaj) in ties.items():
            if len(cover[(x, y)]) != 1:
                continue
            rr = recs[i]
            bx0, by0 = rr["box"][:2]
            if rr["post"][y - by0, x - bx0] == rr["pre"][y - by0, x - bx0]:
                continue       # the port's line does not change this pixel visibly
            r = {k: v for k, v in rr.items() if k not in ("pre", "post")}
            out.append((scen, c, r, x + 0.5, y + 0.5, float(N), bool(xmaj), bool((a[y, x] == ref[y, x]).all())))
    print(scen, len(out), flush=True)
pickle.dump(out, open(ROOT / "log/roadmap/S3/24_data/ties_census.pkl", "wb"))
lab = collections.Counter(o[-1] for o in out)
print("labelled tie pixels", len(out), dict(lab))
