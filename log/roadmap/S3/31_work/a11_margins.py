"""residual value errors (both draw) under f32c:span_dir3; margin = how far the model's alpha must move to give the real value"""
import importlib.util, sys, numpy as np
spec = importlib.util.spec_from_file_location('s', 'log/roadmap/S3/31_score.py'); S = importlib.util.module_from_spec(spec); spec.loader.exec_module(S)
P, F = S.P, S.F
out = []
for p in sys.argv[1:]:
    for r in P.load(p):
        P.geometry = F.geometry_f32; F.CLIP_MODE = "clip"
        cov, g = P.coverage(r, "span_dir3")
        P.geometry = F.P_geometry
        if g is None: continue
        m = P.blend(r, cov)
        both = (m != r["pre"]) & (r["real"] != r["pre"]) & (cov > 0)
        bad = both & (m != r["real"])
        if not bad.any(): continue
        x0, y0 = r["box"][:2]
        ca = min(r["alpha"] / 10, 1)
        for yy, xx in zip(*np.nonzero(bad)):
            d = (P.K[(yy + y0) & 3, (xx + x0) & 3] + 0.5) / 16
            worst = 0
            for sh, s in zip((10, 5, 0), P.chan(r["colour"])):
                dc = (int(r["pre"][yy, xx]) >> sh) & 31
                q_real = (int(r["real"][yy, xx]) >> sh) & 31
                q_mod = (int(m[yy, xx]) >> sh) & 31
                if q_real == q_mod: continue
                # alpha needed: s*255*al + dc*255/31*(1-al) = (q_real + (0 if q_real>q_mod else 1) - d) * 255/31
                tgt = ((q_real if q_real > q_mod else q_real + 1) - d) * 255 / 31
                al_need = (tgt - dc * 255 / 31) / (s * 255 - dc * 255 / 31) if s * 255 != dc * 255 / 31 else np.nan
                worst = max(worst, abs(al_need - ca * cov[yy, xx]))
            out.append((worst, p[-14:], r["x1"], r["y1"], r["x2"], r["y2"], r["width"], r["alpha"], xx + x0, yy + y0, float(cov[yy, xx]),
                        float(g["t"][yy, xx] - g["L"]), float(g["t"][yy, xx]), float(g["n"][yy, xx]), float(g["half"])))
out.sort()
for o in out:
    print("margin %.2e %s (%d,%d)-(%d,%d) w %d a %d px (%d,%d) cov %.6f t-L %.3f t %.3f n %.4f half %.1f" % o)
