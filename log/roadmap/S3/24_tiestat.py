"""S3.24: exact ties of the span rule (pixel centre exactly at +-N/2 from the line centre) and whether the real GL drew them.
Prints rows: set rec w dx dy xmaj k (major steps from the first vertex) side(+/-) real_drawn; and a summary per octant."""
import importlib.util, sys, collections
import numpy as np
spec = importlib.util.spec_from_file_location("p24", "log/roadmap/S3/24_probe.py"); P = importlib.util.module_from_spec(spec); spec.loader.exec_module(P)
s = P.chan(0x7FFF)[0]
out = []
for path in sys.argv[1:]:
    for i, r in enumerate(P.load(path)):
        cov, g = P.coverage(r, "s38")
        if g is None: continue
        dx, dy = g["bx"] - g["ax"], g["by"] - g["ay"]
        major = max(abs(dx), abs(dy))
        nn = np.floor(2 * g["half"] * g["L"] / major) + 2
        o, xmaj = P.minor_offset(g)
        tie = (np.abs(np.abs(o) - nn / 2) < 1e-6) & (cov > 0)
        if not tie.any(): continue
        d = (P.K[g["Y"].astype(int) & 3, g["X"].astype(int) & 3] + 0.5) / 16.0
        # visible test against black pre only; against a random pre use real != pre vs model
        m = P.blend(r, cov)
        vis = m != r["pre"]
        rd = r["real"] != r["pre"]
        for yy, xx in zip(*np.nonzero(tie & vis)):
            Mc = (g["X"] if xmaj else g["Y"])[yy, xx] + 0.5
            a_m = g["ax"] if xmaj else g["ay"]
            k = (Mc - a_m) * (np.sign(dx if xmaj else dy) or 1)
            out.append((path.split("/")[-1][:8], i, r["width"], round(dx, 4), round(dy, 4), int(xmaj), round(float(k), 2),
                        "+" if o[yy, xx] > 0 else "-", int(rd[yy, xx])))
c = collections.Counter()
for row in out:
    c[(row[5], int(np.sign(row[3])), int(np.sign(row[4])), row[7], row[8])] += 1
for k, v in sorted(c.items()):
    print(k, v)
import os
for row in out[: int(os.environ.get("SHOW", "0"))]:
    print(*row)
