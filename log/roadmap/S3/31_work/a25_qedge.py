import importlib.util, sys, numpy as np
argv = sys.argv[1:]; sys.argv = ["x"]
spec = importlib.util.spec_from_file_location("h", "log/roadmap/S3/31_holes.py"); H = importlib.util.module_from_spec(spec); spec.loader.exec_module(H)
P, F, C, S = H.P, H.F, H.C, H.S
rows = []
for p in argv:
    for r in P.load(p):
        if not (r["pre"] == 0).all(): continue
        P.geometry = F.geometry_f32; F.CLIP_MODE = "clip"
        cov, g = P.coverage(r, "span_dir3"); P.geometry = F.P_geometry
        if g is None: continue
        m = P.blend(r, cov)
        u, v, ua, va, s, xmaj, sM, sm = C.canon(g)
        major = max(abs(g["bx"] - g["ax"]), abs(g["by"] - g["ay"]))
        N = np.floor(2 * g["half"] * g["L"] / major) + 2
        o = v - u * s; f = (o + N / 2) % 1; q = u + v * s; ql = 1 - 0.5 * (1 + s * s) ** .5
        mv = (m.astype(int) >> 10) & 31
        sel = (np.abs(q - ql) < 0.02) & (f > 0.02) & (f < 1 - s - 0.02) & (mv >= 2) & (g["t"] - g["L"] > -1.5)
        for yy, xx in zip(*np.nonzero(sel)):
            rows.append((q[yy, xx] - ql, int(r["real"][yy, xx] == 0), s, u[yy, xx], v[yy, xx], g["bx"], g["by"], r["x2"], r["y2"], g["L"]))
rows.sort()
for x in rows:
    print("q-qlim %+.5f %s s %.4f u %+.4f v %+.4f b=(%.4f,%.4f) L %.3f" % (x[0], "DROP" if x[1] else "kept", x[2], x[3], x[4], x[5], x[6], x[9]))
