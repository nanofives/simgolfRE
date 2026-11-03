"""exact ties of the hole rule (f == 1 - s or f == 0, q > qlim, model visible): real drop / keep per octant"""
import importlib.util, sys, collections, numpy as np
argv = sys.argv[1:]; sys.argv = ["x"]
spec = importlib.util.spec_from_file_location("h", "log/roadmap/S3/31_holes.py"); H = importlib.util.module_from_spec(spec); spec.loader.exec_module(H)
P, F, C = H.P, H.F, H.C
cnt = collections.Counter()
for p in argv:
    for r in P.load(p):
        P.geometry = F.geometry_f32; F.CLIP_MODE = "clip"
        cov, g = P.coverage(r, "span_dir3"); P.geometry = F.P_geometry
        if g is None: continue
        m = P.blend(r, cov)
        e = H.unclipped_ends(r)
        gg = dict(g); gg["ax"], gg["ay"], gg["bx"], gg["by"] = e
        dx, dy = e[2] - e[0], e[3] - e[1]
        major = max(abs(dx), abs(dy))
        if major == 0: continue
        u, v, ua, va, s, xmaj, sM, sm = C.canon(gg)
        N = np.floor(2 * g["half"] * np.hypot(dx, dy) / major) + 2
        o = v - u * s; f = (o + N / 2) % 1
        bpx, bpy = e[0] + np.round(dx), e[1] + np.round(dy)
        X, Y = g["X"] + 0.5, g["Y"] + 0.5
        Mc, mc = (X, Y) if xmaj else (Y, X)
        bM, bm = (bpx, bpy) if xmaj else (bpy, bpx)
        q = (Mc - bM) * sM + (mc - bm) * sm * s
        ql = 1 - 0.5 * (1 + s * s) ** .5
        vis = (m != r["pre"]) & (cov > 0)
        for name, tie in (("f=1-s", np.abs(f - (1 - s)) < 1e-6), ("f=0", (f < 1e-6) | (f > 1 - 1e-6))):
            sel = tie & vis & (q > ql + 1e-6) & (q < 0.5 * (1 + s * s) ** .5)
            for yy, xx in zip(*np.nonzero(sel)):
                key = ("x" if abs(dx) >= abs(dy) else "y", "+" if dx >= 0 else "-", "+" if dy >= 0 else "-", name)
                cnt[(key, "drop" if r["real"][yy, xx] == r["pre"][yy, xx] else "kept")] += 1
for k in sorted(cnt): print(k, cnt[k])
