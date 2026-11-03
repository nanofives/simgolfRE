import csv, sys
import numpy as np
rows = []
for p in sys.argv[1:]:
    rows += list(csv.DictReader(open(p), delimiter="\t"))
k = 31 * 248 / 255
A = []
for r in rows:
    f = lambda x: float(r[x])
    if f("c31_model") <= 0: continue
    if f("t_minus_L") <= 0: continue
    drawn = f("hi") > 0.04
    nd_sure = f("hi") <= 0.04
    # skip pixels whose model value would round to 0 anyway at some phases? keep all; mark expected-visible by model c31 > 1
    A.append((f("t_minus_L"), f("s"), f("u"), f("v"), int(r["w"]), drawn, f("c31_model"), f("side")))
A = np.array(A, dtype=float)
print(len(A), "past-b pixels with model cov > 0;", int((A[:, 5] == 0).sum()), "not drawn")
tl, s, u, v, w, dr, c31, side = A.T
rs = np.sqrt(1 + s * s)
# only pixels whose model value is clearly visible in at least the brightest phase: c31 >= 1.0
vis = c31 >= 1.0
print("model c31>=1:", int(vis.sum()), "of which not drawn", int((vis & (dr == 0)).sum()))
for name, q in (("t-L", tl), ("major dist (0.5-tl)*rs", (0.5 - tl) * rs), ("minor dist (0.5-tl)*rs/s", np.where(s > 0, (0.5 - tl) * rs / np.maximum(s, 1e-9), 99)),
                ("tl*rs", tl * rs), ("u", u), ("v", v)):
    nd = q[vis & (dr == 0)]; d = q[vis & (dr == 1)]
    print(f"{name:28s} notdrawn range [{nd.min():.4f},{nd.max():.4f}]  drawn range [{d.min():.4f},{d.max():.4f}]")
# show not-drawn vs drawn in (s, minor dist)
md = np.where(s > 0, (0.5 - tl) * rs / np.maximum(s, 1e-9), 99)
o = np.argsort(md)
print("smallest minor distances (md, status, s, tl, u, v, w, c31):")
for i in o[:60]:
    if vis[i]:
        print("  %.4f %s s %.4f tl %.4f u %+.1f v %+.1f w %d c31 %.2f side %.3f" % (md[i], "DRAWN" if dr[i] else "drop ", s[i], tl[i], u[i], v[i], w[i], c31[i], side[i]))
