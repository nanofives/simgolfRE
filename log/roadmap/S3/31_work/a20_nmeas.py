"""real side coverage of one near-|n|=0.5 pixel per line (alpha bisection, 31_alpha.focus) for several w=2 lines; implied n ratio"""
import importlib.util, sys, json, numpy as np
sys.argv = ["x"]
spec = importlib.util.spec_from_file_location("a", "log/roadmap/S3/31_alpha.py"); A = importlib.util.module_from_spec(spec); spec.loader.exec_module(A)
A.PRE = 0
lines = [(42, 15), (31, 19), (40, 7), (25, 24), (37, 29), (45, 11), (20, 13), (44, 3)]
out = []
for dx, dy in lines:
    L = (dx * dx + dy * dy) ** 0.5
    ux, uy = dx / L, dy / L
    best = None
    for x in range(2, dx - 2):
        for y in range(-3, dy + 4):
            px, py = x + 0.5, y + 0.5
            t = px * ux + py * uy
            n = -px * uy + py * ux
            if 3 < t < L - 3 and 0.5 < abs(n) < 0.53 and 200 < 212 + x < 600:
                if best is None or abs(n) < abs(best[2]):
                    best = (x, y, n)
    if best is None:
        print("no pixel for", dx, dy); continue
    x, y, n = best
    br, ref = A.focus(dx, dy, 2, (x, y), iters=7, level=int(30.149 * (1.5 - abs(n)) * 0.92))
    pl, ph = br["pix"]; rl, rh = br["ref"]
    clo, chi = rl / ph, rh / pl
    nlo, nhi = 1.5 - chi, 1.5 - clo
    out.append(dict(dx=dx, dy=dy, x=x, y=y, n=abs(n), c_model=1.5 - abs(n), c_lo=clo, c_hi=chi, ratio_lo=nlo / abs(n) - 1, ratio_hi=nhi / abs(n) - 1))
    print(out[-1], flush=True)
json.dump(out, open("log/roadmap/S3/31_data/nmeas2.json", "w"), indent=1)
