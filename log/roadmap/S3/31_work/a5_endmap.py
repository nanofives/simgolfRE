"""print the b-end neighbourhood of one line of a phase set: per pixel model 31c and the real interval (copy-0 geometry)."""
import importlib.util, sys
import numpy as np
spec = importlib.util.spec_from_file_location("c", "log/roadmap/S3/31_cap.py"); C = importlib.util.module_from_spec(spec); sys.argv, a = sys.argv[:1], sys.argv[1:]; spec.loader.exec_module(C)
P, PH = C.P, C.PH
npz, want = a[0], tuple(int(v) for v in a[1:6])   # dx dy w ... (x2-x1, y2-y1, w)
recs = P.load(npz)
groups = np.load(npz.replace(".npz", "_groups.npy"), allow_pickle=True)
rule = a[6] if len(a) > 6 else "b8s1"
for g in groups:
    r0 = recs[g[0]]
    if (r0["x2"] - r0["x1"], r0["y2"] - r0["y1"], r0["width"]) != want[:3]:
        continue
    lo, hi = PH.intervals(recs, list(g))
    cov, geo = P.coverage(r0, rule)
    u, v, ua, va, s, xmaj, sM, sm = C.canon(geo)
    c31 = 31 * 248 / 255 * cov
    print("line", want[:3], "s %.4f L %.4f" % (s, geo["L"]))
    # grid in canonical coords: rows v from high to low, cols u
    us = np.arange(-4.5, 3.6, 1.0); vs = np.arange(6.5, -7.6, -1.0)
    for vv in vs:
        line = "v%+5.1f " % vv
        for uu in us:
            m = (np.abs(u - uu) < 0.01) & (np.abs(v - vv) < 0.01)
            if not m.any():
                line += "   .     "; continue
            yy, xx = np.argwhere(m)[0]
            mc, l_, h_ = c31[yy, xx], lo[yy, xx], hi[yy, xx]
            flag = " " if (l_ <= mc < h_) or (h_ <= 0.04 and mc < h_) else ("<" if mc >= h_ else ">")
            line += "%4.1f/%4.1f%s" % (mc, (l_ + h_) / 2 if h_ > 0.04 else 0, flag)
        print(line)
    print("u:    " + "".join("%+9.1f " % x for x in us))
    break
