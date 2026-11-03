"""kept iff floor(A + coord) < floor(B + coord_cap) family, per column (minor) and per row (major), on b-end visible pixels"""
import importlib.util, sys, collections, numpy as np
spec = importlib.util.spec_from_file_location('s', 'log/roadmap/S3/31_score.py'); S = importlib.util.module_from_spec(spec); spec.loader.exec_module(S)
specc = importlib.util.spec_from_file_location('c', 'log/roadmap/S3/31_cap.py'); C = importlib.util.module_from_spec(specc); argv = sys.argv[1:]; sys.argv = sys.argv[:1]; specc.loader.exec_module(C)
P, F = S.P, S.F
rows = []
for f in argv:
    for r in P.load(f):
        if not (r["pre"] == 0).all(): continue
        P.geometry = F.geometry_f32; F.CLIP_MODE = "clip"
        cov, g = P.coverage(r, "span_dir3")
        P.geometry = F.P_geometry
        if g is None: continue
        m = P.blend(r, cov)
        vis, rd = m != 0, r["real"] != 0
        u, v, ua, va, s, xmaj, sM, sm = C.canon(g)
        major = max(abs(g["bx"] - g["ax"]), abs(g["by"] - g["ay"]))
        N = np.floor(2 * g["half"] * g["L"] / major) + 2
        tl = g["t"] - g["L"]
        for yy, xx in zip(*np.nonzero(vis & (tl > -1.0))):
            rows.append((u[yy, xx], v[yy, xx], s, N, int(rd[yy, xx]), g["L"]))
A = np.array(rows); u, v, s, N, dr, L = A.T
rs = np.sqrt(1 + s * s)
print(len(A), "pixels", int((dr == 0).sum()), "dropped")
vcap = (0.5 * rs - u) / np.maximum(s, 1e-9)
ucap = 0.5 * rs - v * s
o = v - u * s
ocap = vcap - u * s
res = []
grid = np.arange(-1.0, 1.01, 0.125)
for name, X, Xc in (("col o", o, ocap), ("col v", v, vcap), ("row u", u, ucap), ("col o+N/2", o + N / 2, ocap + N / 2)):
    for Aa in grid:
        for Bb in grid:
            keep = np.floor(Aa + X) < np.floor(Bb + Xc)
            if name.startswith("col"):
                keep = keep | (s == 0)
            e = int((keep != (dr == 1)).sum())
            res.append((e, name, Aa, Bb))
for r_ in sorted(res)[:10]: print("errors %4d  %-10s A %+.3f B %+.3f" % r_)
