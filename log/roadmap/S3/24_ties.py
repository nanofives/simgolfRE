"""S3.24: residual pixels of a rule per record, with the minor offset o, the octant and the position along the line."""
import importlib.util, pathlib, sys, collections
import numpy as np
HERE = pathlib.Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("p24", HERE / "24_probe.py"); P = importlib.util.module_from_spec(spec); spec.loader.exec_module(P)
rule = sys.argv[1]
stat = collections.Counter()
rows = []
for p in sys.argv[2:]:
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
        for yy, xx in zip(*np.nonzero(d)):
            t, L = g["t"][yy, xx], g["L"]
            where = "start" if t < 1 else ("end" if t > L - 1 else "mid")
            real_drawn = r["real"][yy, xx] != r["pre"][yy, xx]
            model_drawn = m[yy, xx] != r["pre"][yy, xx]
            kind = "ND" if model_drawn and not real_drawn else ("EX" if real_drawn and not model_drawn else "VAL")
            key = (kind, where, "x" if xmaj else "y", int(np.sign(dx)), int(np.sign(dy)), "o+" if o[yy, xx] > 0 else "o-")
            stat[key] += 1
            rows.append((kind, where, r["width"], round(float(np.degrees(np.arctan2(dy, dx))), 1), round(float(o[yy, xx]), 3),
                         round(float(t), 3), round(float(L), 3), round(float(cov[yy, xx]), 3)))
for k, v in sorted(stat.items(), key=lambda kv: -kv[1]):
    print(k, v)
for r in rows[: int(__import__("os").environ.get("SHOW", "0"))]:
    print(*r)
