"""footprint = pixel centres inside the support rectangle whose corners are rounded to a grid G (b end only)"""
import importlib.util, sys, numpy as np
spec = importlib.util.spec_from_file_location('s', 'log/roadmap/S3/31_score.py'); S = importlib.util.module_from_spec(spec); spec.loader.exec_module(S)
P, F = S.P, S.F
data = []
for f in sys.argv[1:]:
    for r in P.load(f):
        if not (r["pre"] == 0).all() or r["colour"] != 0x7FFF: continue
        P.geometry = F.geometry_f32; F.CLIP_MODE = "clip"
        cov, g = P.coverage(r, "span_dir3")
        P.geometry = F.P_geometry
        if g is None: continue
        m = P.blend(r, cov)
        vis, rd = m != 0, r["real"] != 0
        tl = g["t"] - g["L"]
        sel = vis & (tl > -1.5)
        if not sel.any(): continue
        data.append((g, sel, rd))
print(len(data), "records")
def inside_cap(g, sel, G, ext, mode):
    """the b cap edge through corners rounded to grid G; keep iff the pixel centre is on the inner side (>= 0)"""
    ux, uy = g["ux"], g["uy"]
    nx, ny = -uy, ux
    hw = g["half"] + 0.5
    bx, by = g["bx"] + ext * ux, g["by"] + ext * uy
    c1 = np.array([bx + hw * nx, by + hw * ny]); c2 = np.array([bx - hw * nx, by - hw * ny])
    if G > 0:
        c1 = np.round(c1 / G) * G if mode == "round" else np.floor(c1 / G) * G
        c2 = np.round(c2 / G) * G if mode == "round" else np.floor(c2 / G) * G
    X, Y = g["X"] + 0.5, g["Y"] + 0.5
    # inner side = towards a: sign of cross((c2 - c1), (p - c1)) equal to that of a
    ex, ey = c2 - c1
    s = ex * (Y - c1[1]) - ey * (X - c1[0])
    sa = ex * (g["ay"] - c1[1]) - ey * (g["ax"] - c1[0])
    return (s * np.sign(sa)) >= 0
res = []
for G in (0, 1.0, 0.5, 0.25, 0.125, 1 / 16):
    for ext in (0.5,):
        for mode in ("round", "floor"):
            if G == 0 and mode == "floor": continue
            bad = drop_ok = 0
            for g, sel, rd in data:
                k = inside_cap(g, sel, G, ext, mode)
                pred = k & sel
                bad += int(((pred != rd) & sel).sum())
            res.append((bad, G, ext, mode))
for r_ in sorted(res): print("errors %4d  G %.4f ext %.2f %s" % r_)
nd = sum(int((sel & ~rd).sum()) for g, sel, rd in data); nv = sum(int(sel.sum()) for g, sel, rd in data)
print("visible-by-model b-end pixels", nv, "not drawn by real", nd)
