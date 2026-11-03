"""for each dropped b-end pixel: the real drawn status of its column (u) and row (v) neighbours near the cap"""
import importlib.util, sys, numpy as np
spec = importlib.util.spec_from_file_location('s', 'log/roadmap/S3/31_score.py'); S = importlib.util.module_from_spec(spec); spec.loader.exec_module(S)
specc = importlib.util.spec_from_file_location('c', 'log/roadmap/S3/31_cap.py'); C = importlib.util.module_from_spec(specc); argv = sys.argv[1:]; sys.argv = sys.argv[:1]; specc.loader.exec_module(C)
P, F = S.P, S.F
wsel = int(argv[1])
seen = set()
for r in P.load(argv[0]):
    if r["width"] != wsel: continue
    P.geometry = F.geometry_f32; F.CLIP_MODE = "clip"
    cov, g = P.coverage(r, "span_dir3")
    P.geometry = F.P_geometry
    m = P.blend(r, cov)
    vis, rd = m != 0, r["real"] != 0
    u, v, ua, va, s, xmaj, sM, sm = C.canon(g)
    tl = g["t"] - g["L"]
    drop = vis & ~rd & (tl > 0)
    if not drop.any(): continue
    rs = np.sqrt(1 + s * s)
    ds = [(round(float(u[y, x]), 1), round(float(v[y, x]), 1)) for y, x in zip(*np.nonzero(drop))]
    key = (tuple(ds), round(s, 2))
    if key in seen: continue
    seen.add(key)
    # the outermost kept (real drawn) pixel per column near b and per row
    cols = {}
    for y, x in zip(*np.nonzero(rd & (tl > -1.0))):
        uu, vv = round(float(u[y, x]), 1), round(float(v[y, x]), 1)
        cols[uu] = max(cols.get(uu, -99), vv)
    print("s %.4f q-drop %s  top drawn v per column u: %s" % (s, ["%s q=%.3f" % (d, d[0] + d[1] * s) for d in ds],
          " ".join("%+.1f:%+.1f" % (k, cols[k]) for k in sorted(cols) if -4 < k < 5)))
