"""candidate lines (x-major, +x+y, integer ends) with an exact f == 0 or f == 1 - s tie at a visible hole-range pixel, by exact
rational arithmetic; probed through glreplay; prints the real outcome per (slope, width, N, cell index j)"""
import importlib.util, sys, math
from fractions import Fraction as Fr
import numpy as np
sys.argv = ["x"]
spec = importlib.util.spec_from_file_location("h", "log/roadmap/S3/31_holes.py"); H = importlib.util.module_from_spec(spec); spec.loader.exec_module(H)
P = H.P
cands = []
for q_ in range(2, 13):
    for p_ in range(1, q_):
        if math.gcd(p_, q_) != 1: continue
        s = Fr(p_, q_)
        rs = math.sqrt(1 + float(s) ** 2)
        for w in range(2, 11):
            N = math.floor(w * rs) + 2
            for m in range(-4, 5):
                for j in range(-6, 7):
                    u, v = Fr(2 * m + 1, 2), Fr(2 * j + 1, 2)
                    o = v - u * s
                    c = o + Fr(N, 2)
                    f = c - math.floor(c)
                    qv = u + v * s
                    if not (1 - 0.5 * rs < float(qv) < 0.5 * rs): continue
                    n = float(o) / rs
                    if abs(n) >= w / 2 + 0.5 - 0.05: continue
                    if f == 0 or f == 1 - s:
                        cands.append((p_, q_, w, N, float(u), float(v), "f=0" if f == 0 else "f=1-s", int(math.floor(c))))
print(len(cands), "candidates")
# lines: (q*k, p*k) with k making major >= 12; one record per (p, q, w)
seen, recs = set(), []
cx, cy = 204, 154
for (p_, q_, w, N, u, v, kind, j) in cands:
    if (p_, q_, w) in seen: continue
    seen.add((p_, q_, w))
    k = max(1, (14 + q_ - 1) // q_)
    dx, dy = q_ * k, p_ * k
    bw, bh = dx + 2 * (w + 8), dy + 2 * (w + 8)
    if cx + bw > 596: cx, cy = 204, cy + 40
    if cy + bh > 446: break
    r = dict(x1=cx + w + 8, y1=cy + w + 8, x2=cx + w + 8 + dx, y2=cy + w + 8 + dy, colour=0x7FFF, width=w, alpha=10, flip=1, lw=float(w), sw=800, sh=600, fw=800)
    cx += bw
    ax, ay, bx, by, half = P.geometry(r)
    r["box"] = (int(ax - half - 4), int(ay - half - 4), int(bx + half + 4), int(by + half + 4))
    x0, y0, x1, y1 = r["box"]
    r["pre"] = np.zeros((y1 - y0 + 1, x1 - x0 + 1), "<u2"); r["post"] = r["pre"].copy()
    recs.append(r)
print(len(recs), "lines")
real = P.run_real(recs, "s331_f0")
for r, rl in zip(recs, real):
    x0, y0 = r["box"][:2]
    p_ = r["y2"] - r["y1"]; q_ = r["x2"] - r["x1"]
    g = math.gcd(p_, q_); p_, q_ = p_ // g, q_ // g
    for c in cands:
        if (c[0], c[1], c[2]) != (p_, q_, r["width"]): continue
        u, v = c[4], c[5]
        X, Y = int(r["x2"] + u - 0.5), int(r["y2"] + v - 0.5)
        val = rl[Y - y0, X - x0]
        print("s %d/%d w %d N %d (u %+.1f, v %+.1f) %s cell %d real %s" % (c[0], c[1], c[2], c[3], u, v, c[6], c[7], "drop" if val == 0 else "kept %04x" % val))
