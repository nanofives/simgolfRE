"""S3.31: the not-drawn past-cap pixels: model value cov*31*a + dither vs the threshold 1 (white on black)."""
import importlib.util, sys
import numpy as np
spec = importlib.util.spec_from_file_location("p24", "log/roadmap/S3/24_probe.py"); P = importlib.util.module_from_spec(spec); spec.loader.exec_module(P)
rule = "b8s1"
for p in sys.argv[1:]:
    recs = P.load(p)
    nd = dr = 0
    for i, r in enumerate(recs):
        cov, g = P.coverage(r, rule)
        if g is None: continue
        m = P.blend(r, cov); vis = m != r["pre"]; rd = r["real"] != r["pre"]
        Y, X = g["Y"].astype(int), g["X"].astype(int)
        d = (P.K[Y & 3, X & 3] + 0.5) / 16
        tl = g["t"] - g["L"]
        ca = min(r["alpha"] / 10, 1)
        val = cov * ca * 31 + d   # white on black only meaningful when pre==0
        bad = vis != rd
        for yy, xx in zip(*np.nonzero(bad)):
            black = r["pre"][yy, xx] == 0
            print(f"{p[-14:]} rec {i:3d} w {r['width']:2d} ({X[yy,xx]},{Y[yy,xx]}) model {'vis' if vis[yy,xx] else 'no '} real {'vis' if rd[yy,xx] else 'no '} "
                  f"t-L {tl[yy,xx]:+.3f} t {g['t'][yy,xx]:+.3f} n {g['n'][yy,xx]:+.3f} half {g['half']} cov {cov[yy,xx]:.4f} val {val[yy,xx]:.4f} black {black} real5 {r['real'][yy,xx]:04x}")
