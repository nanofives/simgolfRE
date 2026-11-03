import importlib.util, sys, numpy as np
spec = importlib.util.spec_from_file_location('s', 'log/roadmap/S3/31_score.py'); S = importlib.util.module_from_spec(spec); spec.loader.exec_module(S)
P = S.P
recs = P.read_dump("log/roadmap/S3/24_data/dump_cov_a_build.bin")
idx = [int(v) for v in sys.argv[1:]]
sel = [recs[i] for i in idx]
real = P.run_real(sel, "s331_brush")
for i, r, rl in zip(idx, sel, real):
    x0, y0 = r["box"][:2]
    m = S.model(r, "f32c:span_dir3")
    d = P.diff5(m, rl)
    print(i, (r["x1"], r["y1"], r["x2"], r["y2"]), "w", r["width"], "differ", [(int(x + x0), int(y + y0), "%04x/%04x" % (m[y, x], rl[y, x])) for y, x in zip(*np.nonzero(d))])
