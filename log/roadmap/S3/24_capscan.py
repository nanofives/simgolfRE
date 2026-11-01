"""S3.24: does a cap test at a shifted sample point (u + du, v + dv) in the canonical frame separate the real GL's drawn and
not-drawn end pixels? For each shift, the best threshold tau on t_shift = t - L + (du + dv * s) / sqrt(1 + s^2) and its errors."""
import importlib.util, sys, pathlib
import numpy as np
spec = importlib.util.spec_from_file_location("p24", "log/roadmap/S3/24_probe.py"); P = importlib.util.module_from_spec(spec); spec.loader.exec_module(P)
rows = []
for p in sys.argv[1:]:
    for r in P.load(p):
        cov, g = P.coverage(r, "span_dir")
        if g is None or r["width"] < 2: continue
        m = P.blend(r, cov); vis = m != r["pre"]; rd = r["real"] != r["pre"]
        dx, dy = g["bx"] - g["ax"], g["by"] - g["ay"]; xmaj = abs(dx) >= abs(dy)
        sM = np.sign(dx if xmaj else dy) or 1.0; sm = np.sign(dy if xmaj else dx) or 1.0
        X, Y = g["X"] + 0.5, g["Y"] + 0.5
        Mc, mc = (X, Y) if xmaj else (Y, X)
        bM, bm = (g["bx"], g["by"]) if xmaj else (g["by"], g["bx"])
        u = (Mc - bM) * sM; v = (mc - bm) * sm
        s = abs((dy / dx) if xmaj else (dx / dy))
        tl = g["t"] - g["L"]
        sel = vis & (tl > 0)
        for yy, xx in zip(*np.nonzero(sel)):
            rows.append((float(u[yy, xx]), float(v[yy, xx]), s, float(tl[yy, xx]), int(rd[yy, xx])))
A = np.array(rows)
print(len(A), "past-end visible pixels,", int((A[:, 4] == 0).sum()), "not drawn")
res = []
for du in np.arange(-1.0, 1.01, 0.25):
    for dv in np.arange(-1.0, 1.01, 0.25):
        ts = A[:, 3] + (du + dv * A[:, 2]) / np.sqrt(1 + A[:, 2] ** 2)
        best = None
        for tau in np.unique(np.round(ts, 4)):
            err = int(((ts < tau) & (A[:, 4] == 0)).sum() + ((ts >= tau) & (A[:, 4] == 1)).sum())
            if best is None or err < best[0]:
                best = (err, tau)
        res.append((best[0], du, dv, best[1]))
for e, du, dv, tau in sorted(res)[:10]:
    print(f"errors {e:4d}  du {du:+.2f} dv {dv:+.2f}  tau {tau:.4f}")
