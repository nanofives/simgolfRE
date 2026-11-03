"""k_dlg_R record 2397 translated by (4i, 4j) over its own pre box: does the real value at the pixel depend on the position?"""
import importlib.util, sys, numpy as np
spec = importlib.util.spec_from_file_location('s', 'log/roadmap/S3/31_score.py'); S = importlib.util.module_from_spec(spec); spec.loader.exec_module(S)
P, F = S.P, S.F
base = P.read_dump("log/roadmap/S3/24_data/dump_cov_a_keys.bin")[2397]
recs = []
for i in range(-26, 26, 3):
    for j in range(-24, 24, 3):
        dx, dy = 4 * i, 4 * j
        r = dict(base)
        r.update(x1=base["x1"] + dx, x2=base["x2"] + dx, y1=base["y1"] + dy, y2=base["y2"] + dy)
        x0, y0, x1, y1 = base["box"]
        r["box"] = (x0 + dx, y0 + dy, x1 + dx, y1 + dy)
        if r["box"][0] < 0 or r["box"][1] < 0 or r["box"][2] > 799 or r["box"][3] > 599:
            continue
        r["shift"] = (dx, dy)
        recs.append(r)
real = P.run_real(recs, "s331_ktrans")
flip = 0
rows = []
for r, rl in zip(recs, real):
    m = S.model(r, "f32c:span_dir3")
    x0, y0 = r["box"][:2]
    yy, xx = 396 + r["shift"][1] - y0, 460 + r["shift"][0] - x0
    d = P.diff5(m, rl)
    rows.append((r["shift"], "%04x" % rl[yy, xx], "%04x" % m[yy, xx], int((d > 0).sum())))
import collections
print(collections.Counter(x[1] for x in rows), "model at pixel:", collections.Counter(x[2] for x in rows))
print("records with any model/real difference:", sum(1 for x in rows if x[3]), "of", len(rows))
for x in rows:
    if x[1] != "7fff" or x[3]:
        print(x)
