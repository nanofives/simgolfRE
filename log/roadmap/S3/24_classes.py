"""S3.24: residual pixels of a rule split into classes: TIE (|o| within 1e-3 of N/2), END (t < 0.5 or t > L - 0.5), VAL (both
drawn, values differ), OTHER."""
import importlib.util, pathlib, sys, collections
import numpy as np
HERE = pathlib.Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("p24", HERE / "24_probe.py"); P = importlib.util.module_from_spec(spec); spec.loader.exec_module(P)
rule = sys.argv[1]
for p in sys.argv[2:]:
    c = collections.Counter()
    for r in P.load(p):
        cov, g = P.coverage(r, rule)
        if g is None:
            continue
        m = P.blend(r, cov)
        d = P.diff5(m, r["real"])
        if not (d > 0).any():
            continue
        o, xmaj = P.minor_offset(g)
        dx, dy = g["bx"] - g["ax"], g["by"] - g["ay"]
        major = max(abs(dx), abs(dy))
        n_px = np.floor(2.0 * g["half"] * g["L"] / major) + 2.0
        for yy, xx in zip(*np.nonzero(d)):
            rd = r["real"][yy, xx] != r["pre"][yy, xx]
            md = m[yy, xx] != r["pre"][yy, xx]
            t = g["t"][yy, xx]
            if abs(abs(o[yy, xx]) - n_px / 2) < 1e-3:
                k = "TIE"
            elif t < 0.5 or t > g["L"] - 0.5:
                k = "END"
            elif rd and md:
                k = "VAL"
            else:
                k = "OTHER"
            c[(k, int(d[yy, xx] > 1))] += 1
    print(pathlib.Path(p).stem, rule, dict(sorted(c.items())))
