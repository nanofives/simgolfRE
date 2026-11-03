import importlib.util, sys, numpy as np
spec = importlib.util.spec_from_file_location('s', 'log/roadmap/S3/31_score.py'); S = importlib.util.module_from_spec(spec); spec.loader.exec_module(S)
specc = importlib.util.spec_from_file_location('c', 'log/roadmap/S3/31_cap.py'); C = importlib.util.module_from_spec(specc); argv = sys.argv[1:]; sys.argv = sys.argv[:1]; specc.loader.exec_module(C)
P, F = S.P, S.F
rows = []
for f in argv:
    for r in P.load(f):
        if not (r["pre"] == 0).all(): continue
        P.geometry = F.geometry_f32; F.CLIP_MODE = "clip"
        cov, g = P.coverage(r, "span_dir3")
        P.geometry = F.P_geometry
        if g is None: continue
        m = P.blend(r, cov)
        vis, rd = m != 0, r["real"] != 0
        u, v, ua, va, s, xmaj, sM, sm = C.canon(g)
        # exact b in canonical coords is the origin; snapped ends make u, v carry the 1/256 offsets already
        q = u + v * s
        rs = np.sqrt(1 + s * s)
        dist = (q - 0.5) / rs
        for yy, xx in zip(*np.nonzero(vis & (np.abs(dist) < 0.1))):
            rows.append((dist[yy, xx], int(rd[yy, xx]), u[yy, xx], v[yy, xx], s, r["width"], g["t"][yy, xx] - g["L"]))
A = np.array(rows)
d, dr = A[:, 0], A[:, 1]
for lo in np.arange(-0.1, 0.1, 0.01):
    sel = (d >= lo) & (d < lo + 0.01)
    print("dist [%+.2f,%+.2f): visible %4d dropped %4d" % (lo, lo + 0.01, int(sel.sum()), int((sel & (dr == 0)).sum())))
u, v, s, w = A[:, 2], A[:, 3], A[:, 4], A[:, 5]
o = v - u * s
import collections
c = collections.Counter()
for i in range(len(A)):
    if abs(d[i]) >= 0.08: continue
    c[("o>0" if o[i] > 0 else "o<0", "in" if d[i] < 0 else "out", "drop" if dr[i] == 0 else "kept")] += 1
for k in sorted(c): print(k, c[k])
print("o>0 inside / o<0 outside, |dist| bins of 0.005: drop/kept")
for side, cond in (("o>0 in", (o > 0) & (d < 0)), ("o<0 out", (o < 0) & (d >= 0))):
    line = side + ": "
    for lo in np.arange(0, 0.1, 0.005):
        sel = cond & (np.abs(d) >= lo) & (np.abs(d) < lo + 0.005)
        line += "%d/%d " % (int((sel & (dr == 0)).sum()), int((sel & (dr == 1)).sum()))
    print(line)
print("samples o>0 in |d|<0.075")
tlv = A[:, 6]
sel = np.nonzero((o > 0) & (d < 0) & (np.abs(d) < 0.075))[0]
sel = sel[np.lexsort((s[sel], v[sel], u[sel], w[sel]))]
for i in sel[:120]:
    print("%s w %2d (u %+.1f v %+.1f) s %.4f d %+.4f o %.3f tl %.3f" % ("DROP" if dr[i] == 0 else "kept", w[i], u[i], v[i], s[i], d[i], o[i], tlv[i]))
