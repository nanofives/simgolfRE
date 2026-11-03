"""strand hypotheses for the b-end drop, single-record sets (white on black)."""
import importlib.util, sys
import numpy as np
spec = importlib.util.spec_from_file_location("c", "log/roadmap/S3/31_cap.py"); C = importlib.util.module_from_spec(spec); argv = sys.argv[1:]; sys.argv = sys.argv[:1]; spec.loader.exec_module(C)
P = C.P
rows = []
for f in argv:
    for r in P.load(f):
        if not (r["pre"] == 0).all() or r["colour"] != 0x7FFF or r["alpha"] != 10:
            continue
        cov, geo = P.coverage(r, "b8s1")
        if geo is None: continue
        m = P.blend(r, cov)
        vis = (m != 0) & (cov > 0)
        u, v, ua, va, s, xmaj, sM, sm = C.canon(geo)
        major = max(abs(geo["bx"] - geo["ax"]), abs(geo["by"] - geo["ay"]))
        N = np.floor(2 * geo["half"] * geo["L"] / major) + 2
        tl = geo["t"] - geo["L"]
        sel = vis & (tl > 0)
        for yy, xx in zip(*np.nonzero(sel)):
            rows.append((u[yy, xx], v[yy, xx], s, N, geo["L"], tl[yy, xx], int(r["real"][yy, xx] != 0), r["width"], ua[yy, xx], va[yy, xx]))
A = np.array(rows)
u, v, s, N, L, tl, dr, w, ua, va = A.T
rs = np.sqrt(1 + s * s)
print(len(A), "past-b visible pixels;", int((dr == 0).sum()), "dropped")
o = v - u * s
def sep(name, q):
    """best threshold: keep iff q <= tau; errors"""
    best = None
    for tau in np.unique(np.round(q, 5)):
        e = int(((q <= tau) & (dr == 0)).sum() + ((q > tau) & (dr == 1)).sum())
        if best is None or e < best[0]: best = (e, tau)
    print(f"{name:40s} errors {best[0]:4d} tau {best[1]:.4f}")
for e in (0.0, 0.25, 0.5, 0.75, 1.0):
    j = np.floor(o + N / 2)
    c = j - N / 2 + e
    T = (u + (u * s + c) * s) / rs        # t - L of the strand centre point at this column (origin b)
    sep(f"strand centre c=j-N/2+{e}", T)
    # strand end column from the strand line's crossing of the cap, test at column right edge
    T2 = ((u + 0.5) + ((u + 0.5) * s + c) * s) / rs
    sep(f"strand right-edge c=j-N/2+{e}", T2)
    T3 = ((u - 0.5) + ((u - 0.5) * s + c) * s) / rs
    sep(f"strand left-edge c=j-N/2+{e}", T3)
sep("t-L (pixel)", tl)
# strand defined in the pixel's own row of the window counting from the window start computed at the a end (DDA from a)
print("--- grid")
res = []
for f in np.arange(0, 1.0, 0.125):
    for e in np.arange(0.0, 2.01, 0.125):
        j = np.floor(o + N / 2 + f)
        c = j - N / 2 + e
        T = (u + (u * s + c) * s) / rs
        best = None
        for tau in np.unique(np.round(T, 4)):
            er = int(((T <= tau) & (dr == 0)).sum() + ((T > tau) & (dr == 1)).sum())
            if best is None or er < best[0]: best = (er, tau)
        res.append((best[0], f, e, best[1]))
for r_ in sorted(res)[:12]: print("errors %3d f %.3f e %.3f tau %.4f" % r_)
# exact offset o + e
for e in np.arange(-1, 2.01, 0.25):
    T = (u + (u * s + o + e) * s) / rs
    sep(f"exact o+{e}", T)
print("--- strand start + count")
Lmaj = L / rs   # major length
res = []
for e in np.arange(0.0, 1.01, 0.25):
    for f in (0.0,):
        j = np.floor(o + N / 2 + f)
        c = j - N / 2 + e
        ua_start = (-0.5 * rs - c * s) / rs ** 2
        q = np.floor(ua - ua_start + 1e-9)          # column index from the strand's first column
        span = Lmaj + 1.0 / rs                        # strand length in columns (exact)
        for d in np.arange(-1.0, 1.01, 0.125):
            for kind in ("floor", "round", "ceil"):
                K = {"floor": np.floor(span + d), "round": np.floor(span + d + 0.5), "ceil": np.ceil(span + d)}[kind]
                keep = q < K
                er = int((keep & (dr == 0)).sum() + (~keep & (dr == 1)).sum())
                res.append((er, e, d, kind))
for r_ in sorted(res)[:15]: print("errors %3d e %.2f d %+.3f %s" % r_)
print("--- minor cap test families")
j = np.floor(o + N / 2)
o_cap = (0.5 * rs - u) / np.maximum(s, 1e-9) - u * s     # minor offset of the cap line at the column centre (from the line centre)
res = []
for cname, cx in (("o", o), ("o+0.5", o + 0.5), ("celltop", j - N / 2 + 1), ("cellmid", j - N / 2 + 0.5), ("cellbot", j - N / 2)):
    for gname, g in (("1", np.ones_like(s)), ("s/rs", s / rs), ("s", s), ("1/rs", 1 / rs), ("rs", rs), ("s*rs", s * rs)):
        q = (cx - o_cap) * g
        best = None
        for tau in np.unique(np.round(q, 4)):
            er = int(((q <= tau) & (dr == 0)).sum() + ((q > tau) & (dr == 1)).sum())
            if best is None or er < best[0]: best = (er, tau)
        res.append((best[0], cname, gname, best[1]))
for r_ in sorted(res)[:12]: print("errors %3d  c=%-8s g=%-5s tau %.4f" % r_)
print("--- errors of celltop T <= 0.7223")
c = j - N / 2 + 1
T = (u + (u * s + c) * s) / rs
keep = T <= 0.7223
bad = np.nonzero(keep != (dr == 1))[0]
for i in bad:
    print("  %s T %.4f u %+.2f v %+.2f o %+.3f j %d N %d s %.4f w %d tl %.3f L %.2f" % ("DROPPED" if dr[i] == 0 else "kept   ", T[i], u[i], v[i], o[i], j[i], N[i], s[i], w[i], tl[i], L[i]))
# near-threshold kept/dropped
o2 = np.argsort(np.abs(T - 0.7223))[:30]
print("nearest the threshold:")
for i in o2:
    print("  %s T %.4f u %+.2f v %+.2f o %+.3f j %d N %d s %.4f w %d tl %.3f" % ("DROPPED" if dr[i] == 0 else "kept   ", T[i], u[i], v[i], o[i], j[i], N[i], s[i], w[i], tl[i]))
