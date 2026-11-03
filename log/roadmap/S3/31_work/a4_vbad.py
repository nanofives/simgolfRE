import csv, sys, collections
import numpy as np
rows = []
for p in sys.argv[1:]:
    rows += list(csv.DictReader(open(p), delimiter="\t"))
def f(r, k): return float(r[k])
bad = [r for r in rows if r["status"] in ("V_bad", "ND_bad")]
print(len(rows), "rows", len(bad), "bad")
lowc = sum(1 for r in bad if f(r, "c31_model") >= f(r, "hi")); print("real lower than model:", lowc, "higher:", len(bad) - lowc)
# implied cap factor: real c / (side * 248/255) ; compare to model cap
k = 31 * 248 / 255
for r in bad[:0]: pass
H = collections.Counter()
for r in bad:
    tl = f(r, "t_minus_L"); side = f(r, "side")
    H[(round(tl * 4) / 4, "low" if f(r, "c31_model") >= f(r, "hi") else "high")] += 1
print(sorted(H.items()))
# for pixels with side==1 (interior): implied cap = c/k ; implied t-L = 0.5 - cap
res = []
for r in rows:
    side = f(r, "side")
    if side < 0.999: continue
    lo, hi = f(r, "lo"), f(r, "hi")
    if hi <= 0.05: continue
    tl = f(r, "t_minus_L")
    capm = f(r, "cap")
    if capm >= 0.999 or capm <= 0: continue
    caplo, caphi = lo / k, hi / k
    res.append((tl, capm, caplo, caphi, f(r, "s"), f(r, "L"), f(r, "u"), f(r, "v"), int(r["w"])))
A = np.array(res)
print(len(A), "interior cap pixels")
# implied delta = model cap - real cap interval
dlo = A[:, 1] - A[:, 3]; dhi = A[:, 1] - A[:, 2]
mid = (dlo + dhi) / 2
print("delta (model cap - real cap) mid: mean %.4f sd %.4f min %.4f max %.4f" % (mid.mean(), mid.std(), mid.min(), mid.max()))
for q in np.argsort(-mid)[:25]:
    print("tl %+.3f capm %.3f real [%.3f,%.3f) s %.3f L %.2f u %+.2f v %+.2f w %d" % tuple(A[q, :9]))
