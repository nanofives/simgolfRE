"""samples relative to the nearest cluster centre C_k (on q = 0.5, o = o(P0) + k): drop / kept map in (dq, do)"""
import importlib.util, sys, collections, numpy as np
argv = sys.argv[1:]; sys.argv = ["x"]
spec = importlib.util.spec_from_file_location("sh", "log/roadmap/S3/31_shape.py"); M = importlib.util.module_from_spec(spec); spec.loader.exec_module(M)
A, s, w = M.samples(argv[0])
u, v, vis, rd, n, tl, mv = A.T
rs = (1 + s * s) ** .5
q = u + v * s
o = v - u * s
oP = -0.5 * s
k = np.round(o - oP)
do = o - oP - k
dq = q - 0.5
sel = (vis == 1) & (mv >= 2) & (tl > -1.5)
D = sel & (rd == 0)
K = sel & (rd == 1)
print("slope %.4f w %d: drops %d" % (s, w, D.sum()))
print("drops: dq range [%.3f,%.3f], do range [%.3f,%.3f]" % (dq[D].min(), dq[D].max(), do[D].min(), do[D].max()))
step = float(argv[1]) if len(argv) > 1 else 0.025
print("map rows do (top +), cols dq; D drop, K kept, ' ' none")
for dov in np.arange(0.3, -0.3 - 1e-9, -step):
    line = "%+.3f " % dov
    for dqv in np.arange(-0.2, 0.2 + 1e-9, step):
        m = (np.abs(do - dov) < step / 2) & (np.abs(dq - dqv) < step / 2)
        d, kk = (m & D).sum(), (m & K).sum()
        line += "D" if d and not kk else ("*" if d and kk else ("K" if kk else " "))
    print(line)
