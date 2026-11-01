"""S3.24: for the differing pixels of rows (frames_<tag> against the reference) that a line touches (frames_off differs), the
dumped lines (distinct calls) whose S3.8 support covers the pixel, with the pixel's class per line: TIE (|o| within 0.002 of N/2),
END (t < 0.5 or t > L - 0.5), MID; and the summary per row."""
import collections, importlib.util, pathlib, sys
import numpy as np
ROOT = pathlib.Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "port/build/S3_24"))
from port.test import gfx as GX
spec = importlib.util.spec_from_file_location("p24", ROOT / "log/roadmap/S3/24_probe.py"); P = importlib.util.module_from_spec(spec); spec.loader.exec_module(P)
tag = sys.argv[1]
for scen in sys.argv[2:]:
    on = np.load(ROOT / f"log/roadmap/S3/24_data/frames_{tag}/{scen}.npz")
    offp = ROOT / f"log/roadmap/S3/24_data/frames_off/{scen}.npz"
    off = np.load(offp) if offp.exists() else None
    recs, seen = [], set()
    for r in P.read_dump(ROOT / f"log/roadmap/S3/24_data/dump_{scen}.bin"):
        k = (r["x1"], r["y1"], r["x2"], r["y2"], r["width"], r["lw"])
        if k not in seen:
            seen.add(k); recs.append(r)
    for c in on.files:
        ref = GX.reference(scen, c).astype(int) >> 3
        a = on[c].astype(int)
        d = np.abs(a - ref).max(2)
        cls = collections.Counter()
        for y, x in zip(*np.nonzero(d)):
            kinds = set()
            for r in recs:
                x0, y0, x1, y1 = r["box"]
                if not (x0 <= x <= x1 and y0 <= y <= y1):
                    continue
                g = P.geometry(r)
                if g is None:
                    continue
                ax, ay, bx, by, half = g
                dx, dy = bx - ax, by - ay
                L = np.hypot(dx, dy)
                if L == 0:
                    continue
                ux, uy = dx / L, dy / L
                px, py = x + 0.5 - ax, y + 0.5 - ay
                t, n = px * ux + py * uy, -px * uy + py * ux
                cov = max(0, min(1, half + 0.5 - abs(n))) * max(0, min(1, t + 0.5)) * max(0, min(1, L - t + 0.5))
                if cov <= 0:
                    continue
                major = max(abs(dx), abs(dy))
                N = np.floor(2 * half * L / major) + 2
                o = (y + 0.5) - (ay + (x + 0.5 - ax) * dy / dx) if abs(dx) >= abs(dy) else (x + 0.5) - (ax + (y + 0.5 - ay) * dx / dy)
                if abs(abs(o) - N / 2) < 2e-3:
                    kinds.add("TIE")
                elif abs(o) > N / 2:
                    kinds.add("CUT")
                elif t < 0.5 or t > L - 0.5:
                    kinds.add("END")
                else:
                    kinds.add("MID")
            cls["+".join(sorted(kinds)) or "NOLINE"] += 1
        print(scen, c, int((d > 0).sum()), dict(cls), flush=True)
