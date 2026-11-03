"""dense samples: per bin of g = f / (1 - s), the q - qlim of the lowest drop and the highest kept (inside the f range)"""
import importlib.util, sys, numpy as np
argv = sys.argv[1:]; sys.argv = ["x"]
spec = importlib.util.spec_from_file_location("sh", "log/roadmap/S3/31_shape.py"); M = importlib.util.module_from_spec(spec); spec.loader.exec_module(M)
for fn in argv:
    A, s, w = M.samples(fn)
    u, v, vis, rd, n, tl, mv = A.T
    rs = (1 + s * s) ** .5
    N = np.floor(w * rs) + 2
    o = v - u * s; f = (o + N / 2) % 1; q = u + v * s; ql = 1 - 0.5 * rs
    sel = (vis == 1) & (mv >= 1) & (f > 0) & (f < 1 - s)
    D, K = sel & (rd == 0), sel & (rd == 1)
    g = f / (1 - s)
    print(fn.split("/")[-1], "s %.4f" % s)
    for lo in np.arange(0, 1, 0.1):
        b = (g >= lo) & (g < lo + 0.1)
        d = (q - ql)[b & D]; k = (q - ql)[b & K]
        kk = k[k > -0.05]
        print("  g [%.1f,%.1f) lowest drop %+.4f  highest kept %+.4f  (kept above the lowest drop: %d)" % (lo, lo + .1, d.min() if len(d) else 9, kk.max() if len(kk) else -9, int((k > (d.min() if len(d) else 9)).sum())))
