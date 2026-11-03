"""hole map in (dq = q - 0.5, dw = (o + N/2) - (0.5 - 0.5 s) wrapped to [-0.5, 0.5)); per dq bin the dw extent of drops and of kept"""
import importlib.util, sys, numpy as np
argv = sys.argv[1:]; sys.argv = ["x"]
spec = importlib.util.spec_from_file_location("sh", "log/roadmap/S3/31_shape.py"); M = importlib.util.module_from_spec(spec); spec.loader.exec_module(M)
for f in argv:
    A, s, w = M.samples(f)
    u, v, vis, rd, n, tl, mv = A.T
    rs = (1 + s * s) ** .5
    N = np.floor(w * rs) + 2
    q = u + v * s; o = v - u * s
    dw = ((o + N / 2) - (0.5 - 0.5 * s) + 0.5) % 1.0 - 0.5
    dq = q - 0.5
    sel = (vis == 1) & (mv >= 2)
    D, K = sel & (rd == 0), sel & (rd == 1)
    print("%s s %.4f w %d N %d: drops %d; cap mirror %.4f" % (f.split('/')[-1], s, w, N, D.sum(), -(0.5 * rs - 0.5)))
    for lo in np.arange(-0.2, 0.12, 0.02):
        b = (dq >= lo) & (dq < lo + 0.02)
        dd, kk = dw[b & D], dw[b & K]
        kin = kk[np.abs(kk) < (np.abs(dd).max() if len(dd) else 0)] if len(dd) else []
        print("  dq [%+.2f,%+.2f) drops %3d |dw| max %.3f   kept %3d, kept with |dw| below that %d, min |dw| kept %.3f" % (
            lo, lo + 0.02, len(dd), np.abs(dd).max() if len(dd) else 0, len(kk), len(kin), np.abs(kk).min() if len(kk) else 9))
