"""S3.24: for records with END-class residuals under a rule, print the last / first major lines: per pixel o, t - L (or t), model
coverage and R(eal drawn) / M(odel drawn only) / . (neither)."""
import importlib.util, pathlib, sys
import numpy as np
HERE = pathlib.Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("p24", HERE / "24_probe.py"); P = importlib.util.module_from_spec(spec); spec.loader.exec_module(P)
rule, path, nshow = sys.argv[1], sys.argv[2], int(sys.argv[3])
shown = 0
for i, r in enumerate(P.load(path)):
    cov, g = P.coverage(r, rule)
    if g is None:
        continue
    m = P.blend(r, cov)
    d = P.diff5(m, r["real"])
    end = (g["t"] < 0.5) | (g["t"] > g["L"] - 0.5)
    if not (d[end] > 0).any():
        continue
    o, xmaj = P.minor_offset(g)
    dx, dy = g["bx"] - g["ax"], g["by"] - g["ay"]
    print(f"rec {i} w {r['width']} a=({g['ax']:.4f},{g['ay']:.4f}) b=({g['bx']:.4f},{g['by']:.4f}) L {g['L']:.3f} xmaj {xmaj}")
    rd = r["real"] != r["pre"]
    md = m != r["pre"]
    lines = range(cov.shape[1]) if xmaj else range(cov.shape[0])
    for k in lines:
        sl = (slice(None), k) if xmaj else (k, slice(None))
        cc, tt, oo, rr, mm, dd = cov[sl], g["t"][sl], o[sl], rd[sl], md[sl], d[sl]
        if not ((tt < 1.5) | (tt > g["L"] - 1.5))[cc > 0].any():
            continue
        idx = np.nonzero((cc > 0) | rr)[0]
        if not len(idx):
            continue
        maj = int((g["X"] if xmaj else g["Y"])[sl][0])
        cells = []
        for j in idx:
            mark = ("R" if rr[j] and mm[j] else ("r" if rr[j] else ("m" if mm[j] else "."))) + ("!" if dd[j] else "")
            cells.append(f"o{oo[j]:+.2f}/t{tt[j]:.2f}/c{cc[j]:.2f}{mark}")
        print(f"  {maj:4d} " + " ".join(cells))
    shown += 1
    if shown >= nshow:
        break
