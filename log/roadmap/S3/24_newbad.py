"""S3.24: pixels a rule gets wrong that S3.8's model got right (the 'none new' condition), per npz."""
import importlib.util, sys
import numpy as np
spec = importlib.util.spec_from_file_location("p24", "log/roadmap/S3/24_probe.py"); P = importlib.util.module_from_spec(spec); spec.loader.exec_module(P)
rule = sys.argv[1]
for p in sys.argv[2:]:
    new = fixed = old_bad = new_bad = 0
    for r in P.load(p):
        c0, _ = P.coverage(r, "s38"); c1, _ = P.coverage(r, rule)
        b0 = P.diff5(P.blend(r, c0), r["real"]) > 0
        b1 = P.diff5(P.blend(r, c1), r["real"]) > 0
        new += int((b1 & ~b0).sum()); fixed += int((b0 & ~b1).sum()); old_bad += int(b0.sum()); new_bad += int(b1.sum())
    print(p.split("/")[-1], rule, "bad", old_bad, "->", new_bad, "fixed", fixed, "new", new)
