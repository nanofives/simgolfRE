import importlib.util, sys, collections, numpy as np
argv = sys.argv[1:]; sys.argv = ["x"]
spec = importlib.util.spec_from_file_location("sh", "log/roadmap/S3/31_shape.py"); M = importlib.util.module_from_spec(spec); spec.loader.exec_module(M)
P, F, C = M.P, M.F, M.C
z = np.load(argv[0])
dx, dy, w = (int(x) for x in z["meta"])
rows = []
for ci in range(len(z["boxes"])):
    box, e = z["boxes"][ci], z["ends"][ci]
    r = dict(fx1=e[0], fy1=e[1], fx2=e[2], fy2=e[3], x1=0, y1=0, x2=0, y2=0, colour=0x7FFF, width=w, alpha=10, flip=1, lw=float(w), sw=800, sh=600, fw=800, box=tuple(int(v) for v in box))
    x0, y0, x1, y1 = r["box"]; r["pre"] = np.zeros((y1 - y0 + 1, x1 - x0 + 1), "<u2")
    P.geometry = M.geometry_float
    cov, g = P.coverage(r, "span_dir3"); P.geometry = F.P_geometry
    m = P.blend(r, cov); real = z[f"real{ci}"]
    u, v, ua, va, s, xmaj, sM, sm = C.canon(g)
    tl = g["t"] - g["L"]
    mv = (m.astype(int) >> 10) & 31; rv = (real.astype(int) >> 10) & 31
    X, Y = g["X"].astype(int), g["Y"].astype(int)
    bx, by, ax, ay = g["bx"], g["by"], g["ax"], g["ay"]
    for yy, xx in zip(*np.nonzero((tl > -1) & (mv >= 2))):
        rows.append((ci, X[yy, xx], Y[yy, xx], u[yy, xx], v[yy, xx], tl[yy, xx], int(rv[yy, xx] == 0), bx, by, ax, ay, g["n"][yy, xx]))
A = np.array(rows, dtype=float)
ci, X, Y, u, v, tl, dr, bx, by, ax, ay, n = A.T
print(len(A), "samples, drops", int(dr.sum()))
def tab(name, key):
    c = collections.defaultdict(lambda: [0, 0])
    for k, d in zip(key, dr):
        c[k][int(d)] += 1
    print(name, " ".join("%s:%d/%d" % (k, c[k][1], c[k][0] + c[k][1]) for k in sorted(c)))
tab("col - floor(bx+0.5)", (X - np.floor(bx + 0.5)).astype(int))
tab("col - floor(bx)", (X - np.floor(bx)).astype(int))
tab("col - round(bx)", (X - np.floor(bx + 0.5)).astype(int))
tab("row - floor(by)", (Y - np.floor(by)).astype(int))
tab("row - floor(by+0.5)", (Y - np.floor(by + 0.5)).astype(int))
# per copy: number of columns from the first column of a
tab("col - floor(ax+0.5)", (X - np.floor(ax + 0.5)).astype(int))
