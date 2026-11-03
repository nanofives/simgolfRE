import importlib.util, sys, numpy as np
spec = importlib.util.spec_from_file_location('s', 'log/roadmap/S3/31_score.py'); S = importlib.util.module_from_spec(spec); spec.loader.exec_module(S)
P = S.P
for p in sys.argv[1:]:
    for i, r in enumerate(P.load(p)):
        b = P.diff5(S.model(r, "snap:b8s1"), r["real"]) > 0
        m32 = S.model(r, "f32:b8s1")
        d = P.diff5(m32, r["real"]) > 0
        if (d & ~b).any() or (b & ~d).any():
            ends_in = all(0 <= v <= 800 for v in (r["x1"], r["x2"])) and all(0 <= v <= 600 for v in (r["y1"], r["y2"]))
            print(p[-12:], i, (r["x1"], r["y1"], r["x2"], r["y2"]), "w", r["width"], "flip", r["flip"], "ends_in", ends_in, "new", int((d & ~b).sum()), "fixed", int((b & ~d).sum()))
