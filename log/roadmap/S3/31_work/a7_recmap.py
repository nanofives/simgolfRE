"""canonical b-end map of single records (white on black): model 5-bit R / real 5-bit R, '<' = real lower."""
import importlib.util, sys
import numpy as np
spec = importlib.util.spec_from_file_location("c", "log/roadmap/S3/31_cap.py"); C = importlib.util.module_from_spec(spec); argv = sys.argv[1:]; sys.argv = sys.argv[:1]; spec.loader.exec_module(C)
P = C.P
recs = P.load(argv[0])
for i in (int(x) for x in argv[1:]):
    r = recs[i]
    cov, geo = P.coverage(r, "b8s1")
    m = P.blend(r, cov)
    u, v, ua, va, s, xmaj, sM, sm = C.canon(geo)
    N = np.floor(2 * geo["half"] * geo["L"] / max(abs(geo["bx"] - geo["ax"]), abs(geo["by"] - geo["ay"]))) + 2
    rs = np.sqrt(1 + s * s)
    print(f"rec {i} ({r['x1']},{r['y1']})-({r['x2']},{r['y2']}) w {r['width']} s {s:.4f} L {geo['L']:.3f} N {N:.0f} xmaj {xmaj} sM {sM:+.0f} sm {sm:+.0f}  a=({geo['ax']:.4f},{geo['ay']:.4f}) b=({geo['bx']:.4f},{geo['by']:.4f})")
    us = np.arange(-4.5, 3.6, 1.0); vs = np.arange(7.5, -8.6, -1.0)
    for vv in vs:
        line = "v%+5.1f " % vv
        for uu in us:
            mm = (np.abs(u - uu) < 0.02) & (np.abs(v - vv) < 0.02)
            if not mm.any():
                line += "  .    "; continue
            yy, xx = np.argwhere(mm)[0]
            a_, b_ = (int(m[yy, xx]) >> 10) & 31, (int(r["real"][yy, xx]) >> 10) & 31
            fl = " " if a_ == b_ else ("<" if b_ < a_ else ">")
            inwin = abs((vv - uu * s)) <= N / 2
            line += "%2d/%2d%s%s " % (a_, b_, fl, "|" if inwin else " ")
        print(line)
    print("u:    " + "".join("%+6.1f " % x for x in us))
