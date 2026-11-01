"""S3.24: per column, the count of drawn pixels against floor(2 * reach) and the side of each not-drawn pixel (o sign relative
to the direction of travel along the minor axis)."""
import importlib.util, pathlib, sys
import numpy as np
HERE = pathlib.Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("p24", HERE / "24_probe.py"); P = importlib.util.module_from_spec(spec); spec.loader.exec_module(P)
recs = []
for p in sys.argv[1:]:
    recs += P.load(p)
stat = {}
for r in recs:
    if r["alpha"] != 10 or r["colour"] != 0x7FFF or r["pre"].any():
        continue
    cov, g = P.coverage(r, "s38")
    if g is None:
        continue
    dx, dy = g["bx"] - g["ax"], g["by"] - g["ay"]
    xmaj = abs(dx) >= abs(dy)
    X, Y = g["X"] + 0.5, g["Y"] + 0.5
    if xmaj:
        o = Y - (g["ay"] + (X - g["ax"]) * dy / dx); sgn_minor = np.sign(dy); cols = g["X"][0]; axis = 1
    else:
        o = X - (g["ax"] + (Y - g["ay"]) * dx / dy); sgn_minor = np.sign(dx); axis = 0
    s = P.chan(0x7FFF)[0]
    d = (P.K[g["Y"].astype(int) & 3, g["X"].astype(int) & 3] + 0.5) / 16.0
    vis = np.floor(31 * s * cov + d) >= 1
    drawn = r["real"] != 0
    mid = (g["t"] >= 1.0) & (g["t"] <= g["L"] - 1.0)
    nd = vis & ~drawn & mid
    for yy, xx in zip(*np.nonzero(nd)):
        key = ("x" if xmaj else "y", int(np.sign(dx)), int(np.sign(dy)), "o+" if o[yy, xx] > 0 else "o-")
        stat[key] = stat.get(key, 0) + 1
for k, v in sorted(stat.items()):
    print(k, v)
