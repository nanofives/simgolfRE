"""k_dlg_R pixel (460, 396): replay dump record(s) through the real GL over their own pre box; print values around the pixel."""
import importlib.util, sys
import numpy as np
spec = importlib.util.spec_from_file_location("p24", "log/roadmap/S3/24_probe.py"); P = importlib.util.module_from_spec(spec); spec.loader.exec_module(P)
recs = P.read_dump("log/roadmap/S3/24_data/dump_cov_a_keys.bin")
idx = [int(v) for v in sys.argv[1:]] or [2397]
sel = [recs[i] for i in idx]
real = P.run_real(sel, "s331_kdlg")
for r, rl in zip(sel, real):
    x0, y0 = r["box"][:2]
    cov, g = P.coverage(r, "b8s1")
    m = P.blend(r, cov)
    print("line", (r["x1"], r["y1"], r["x2"], r["y2"]), "w", r["width"], "a", r["alpha"], "geo", [round(v, 5) for v in (g["ax"], g["ay"], g["bx"], g["by"])])
    yy, xx = 396 - y0, 460 - x0
    for name, a in (("pre", r["pre"]), ("port", r["post"]), ("model", m), ("real", rl)):
        v = int(a[yy, xx]); print(f"  {name:5s} {v:04x} ({(v>>10)&31},{(v>>5)&31},{v&31})")
    print("  cov", cov[yy, xx], "n", g["n"][yy, xx], "t", g["t"][yy, xx])
    d = P.diff5(m, rl)
    print("  model vs real differing px in box:", int((d > 0).sum()), "port vs real:", int((P.diff5(r["post"], rl) > 0).sum()))
    for y2, x2 in zip(*np.nonzero(d)):
        print("    ", x2 + x0, y2 + y0, "model %04x real %04x" % (m[y2, x2], rl[y2, x2]), "cov %.5f n %.5f t %.4f" % (cov[y2, x2], g["n"][y2, x2], g["t"][y2, x2]))
