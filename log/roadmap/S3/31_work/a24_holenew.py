import importlib.util, sys, collections, numpy as np
argv = sys.argv[1:]; sys.argv = ["x"]
spec = importlib.util.spec_from_file_location("h", "log/roadmap/S3/31_holes.py"); H = importlib.util.module_from_spec(spec); spec.loader.exec_module(H)
P, F, C, S = H.P, H.F, H.C, H.S
VAR = "lt_gt_1e-9"
cnt = collections.Counter()
shown = 0
for p in argv:
    for r in P.load(p):
        b = P.diff5(S.model(r, "f32c:span_dir3"), r["real"])
        hm = H.model(r, VAR)
        h = P.diff5(hm, r["real"])
        new = (h > 0) & (b == 0)
        if not new.any(): continue
        P.geometry = F.geometry_f32; F.CLIP_MODE = "clip"
        cov, g = P.coverage(r, "span_dir3"); P.geometry = F.P_geometry
        u, v, ua, va, s, xmaj, sM, sm = C.canon(g)
        major = max(abs(g["bx"] - g["ax"]), abs(g["by"] - g["ay"]))
        N = np.floor(2 * g["half"] * g["L"] / major) + 2
        o = v - u * s; f = (o + N / 2) % 1; q = u + v * s; ql = 1 - 0.5 * (1 + s * s) ** .5
        tl = g["t"] - g["L"]
        for yy, xx in zip(*np.nonzero(new)):
            end = "b" if tl[yy, xx] > -1.5 else ("a" if g["t"][yy, xx] < 1.5 else "mid")
            cnt[(end, round(s, 4))] += 1
            if shown < 25:
                shown += 1
                print("%s (%d,%d)-(%d,%d) w %d s %.4f end %s u %+.4f v %+.4f f %.5f 1-s %.5f q %.5f qlim %.5f t-L %.3f real %04x model %04x" % (
                    p[-10:], r["x1"], r["y1"], r["x2"], r["y2"], r["width"], s, end, u[yy, xx], v[yy, xx], f[yy, xx], 1 - s, q[yy, xx], ql, tl[yy, xx], r["real"][yy, xx], hm[yy, xx]))
print(sorted(cnt.items()))
