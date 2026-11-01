"""S3.24: ASCII picture of one record's box under a rule: # both draw, M model only, r real only, '.' neither; optional window."""
import importlib.util, sys
import numpy as np
spec = importlib.util.spec_from_file_location("p24", "log/roadmap/S3/24_probe.py"); P = importlib.util.module_from_spec(spec); spec.loader.exec_module(P)
path, i, rule = sys.argv[1], int(sys.argv[2]), sys.argv[3]
r = P.load(path)[i]
cov, g = P.coverage(r, rule)
m = P.blend(r, cov)
rd, md = r["real"] != r["pre"], m != r["pre"]
x0, y0 = r["box"][:2]
print(f"a=({g['ax']},{g['ay']}) b=({g['bx']},{g['by']}) w={r['width']} box {r['box']}")
print("     " + "".join(str((x0 + c) % 10) for c in range(cov.shape[1])))
for yy in range(cov.shape[0]):
    row = "".join("#" if a and b else ("M" if b else ("r" if a else ("," if cov[yy, c] > 0 else "."))) for c, (a, b) in enumerate(zip(rd[yy], md[yy])))
    print(f"{y0 + yy:4d} {row}")
