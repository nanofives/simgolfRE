"""per canonical b-end position (u, v): the slopes where the model draws it visibly and the real GL drops it vs keeps it"""
import importlib.util, sys, collections, numpy as np
spec = importlib.util.spec_from_file_location('s', 'log/roadmap/S3/31_score.py'); S = importlib.util.module_from_spec(spec); spec.loader.exec_module(S)
specc = importlib.util.spec_from_file_location('c', 'log/roadmap/S3/31_cap.py'); C = importlib.util.module_from_spec(specc); argv = sys.argv[1:]; sys.argv = sys.argv[:1]; specc.loader.exec_module(C)
P, F = S.P, S.F
pos = collections.defaultdict(list)
for r in P.load(argv[0]):
    P.geometry = F.geometry_f32; F.CLIP_MODE = "clip"
    cov, g = P.coverage(r, "span_dir3")
    P.geometry = F.P_geometry
    if g is None: continue
    m = P.blend(r, cov)
    vis, rd = m != 0, r["real"] != 0
    u, v, ua, va, s, xmaj, sM, sm = C.canon(g)
    tl = g["t"] - g["L"]
    for yy, xx in zip(*np.nonzero(vis & (tl > 0))):
        pos[(round(float(u[yy, xx]), 1), round(float(v[yy, xx]), 1), r["width"])].append((s, int(rd[yy, xx]), float(tl[yy, xx]), r["x2"] - r["x1"], r["y2"] - r["y1"]))
for key in sorted(pos, key=lambda k: (k[2], k[0], k[1])):
    L = sorted(pos[key])
    if all(x[1] for x in L): continue
    segs, cur = [], None
    for sv, d, tl, dx, dy in L:
        st = "K" if d else "D"
        if cur and cur[0] == st:
            cur[2] = sv; cur[3] += 1
        else:
            if cur: segs.append(cur)
            cur = [st, sv, sv, 1]
    segs.append(cur)
    print("w %2d (u %+.1f, v %+.1f): " % (key[2], key[0], key[1]) + " ".join("%s[%.4f..%.4f]x%d" % tuple(sg) for sg in segs))
