"""per record (each 16-phase copy individually, own geometry): b-end drops (model visible, real 0) and extras, by slope and length"""
import importlib.util, sys, collections, numpy as np
spec = importlib.util.spec_from_file_location('s', 'log/roadmap/S3/31_score.py'); S = importlib.util.module_from_spec(spec); spec.loader.exec_module(S)
specc = importlib.util.spec_from_file_location('c', 'log/roadmap/S3/31_cap.py'); C = importlib.util.module_from_spec(specc); argv = sys.argv[1:]; sys.argv = sys.argv[:1]; specc.loader.exec_module(C)
P, F = S.P, S.F
model = argv[1] if len(argv) > 1 else "f32c:span_dir3"
tab = collections.defaultdict(lambda: [0, 0, set()])
for r in P.load(argv[0]):
    vert, rule = model.split(":")
    P.geometry = F.geometry_f32 if vert.startswith("f32") else F.P_geometry
    F.CLIP_MODE = "clip" if vert == "f32c" else "window"
    cov, g = P.coverage(r, rule)
    P.geometry = F.P_geometry
    if g is None: continue
    m = P.blend(r, cov)
    vis, rd = m != 0, r["real"] != 0
    u, v, ua, va, s, xmaj, sM, sm = C.canon(g)
    end = "b" if True else ""
    tl = g["t"] - g["L"]
    dx, dy = r["x2"] - r["x1"], r["y2"] - r["y1"]
    key = (abs(dy) if abs(dx) >= abs(dy) else abs(dx), max(abs(dx), abs(dy)), r["width"])
    k = tab[key]
    k[1] += 1
    drop = vis & ~rd
    extra = rd & ~vis
    for yy, xx in zip(*np.nonzero(drop | extra)):
        side = "b" if tl[yy, xx] > -1.5 else ("a" if g["t"][yy, xx] < 1.5 else "mid")
        k[0] += 1
        k[2].add(("D" if drop[yy, xx] else "X") + side + "(%+.1f,%+.1f)" % ((u if side == "b" else ua)[yy, xx], (v if side == "b" else va)[yy, xx]))
for key in sorted(tab, key=lambda k: (k[0] / k[1], k[1], k[2])):
    n, c, ev = tab[key]
    print("minor %2d major %2d s %.4f w %2d copies %2d bad %3d  %s" % (key[0], key[1], key[0] / key[1], key[2], c, n, " ".join(sorted(ev))[:200]))
