"""S3.24: canonical frame (major and minor increasing, origin b): per end column u, the topmost (largest v) pixel the model
draws visibly, whether the real GL drew it, with slope s, width, t - L, the window top vc + N/2 and the cap-line v at the centre."""
import importlib.util, sys
import numpy as np
spec = importlib.util.spec_from_file_location("p24", "log/roadmap/S3/24_probe.py"); P = importlib.util.module_from_spec(spec); spec.loader.exec_module(P)
ucol = float(sys.argv[1])
rows = []
for p in sys.argv[2:]:
    for r in P.load(p):
        cov, g = P.coverage(r, "b8s1")
        if g is None or r["width"] < 2: continue
        m = P.blend(r, cov); vis = m != r["pre"]; rd = r["real"] != r["pre"]
        dx, dy = g["bx"] - g["ax"], g["by"] - g["ay"]; xmaj = abs(dx) >= abs(dy)
        sM = np.sign(dx if xmaj else dy) or 1.0; sm = np.sign(dy if xmaj else dx) or 1.0
        X, Y = g["X"] + 0.5, g["Y"] + 0.5
        Mc, mc = (X, Y) if xmaj else (Y, X)
        bM, bm = (g["bx"], g["by"]) if xmaj else (g["by"], g["bx"])
        u = (Mc - bM) * sM; v = (mc - bm) * sm
        s = abs((dy / dx) if xmaj else (dx / dy))
        sel = vis & (np.abs(u - ucol) < 0.01)
        if not sel.any(): continue
        j = np.argmax(np.where(sel, v, -99))
        yy, xx = np.unravel_index(j, v.shape)
        major = max(abs(dx), abs(dy)); N = np.floor(2 * g["half"] * g["L"] / major) + 2
        vc = ucol * s
        rs = np.sqrt(1 + s * s)
        vcap = (0.5 * rs - ucol) / s if s > 0 else 99      # t - L = (u + v s) / rs = 0.5
        rows.append((round(s, 3), r["width"], round(float(v[yy, xx]), 3), int(rd[yy, xx]), round(float(g["t"][yy, xx] - g["L"]), 3),
                     round(vc + N / 2, 3), round(vcap, 3), round(float(cov[yy, xx]), 3)))
rows.sort()
print("slope w v_top real t-L window_top v_cap cov")
for r in rows:
    print(*r)
