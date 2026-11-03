import importlib.util, numpy as np
spec = importlib.util.spec_from_file_location('f', 'log/roadmap/S3/31_f32.py'); F = importlib.util.module_from_spec(spec); spec.loader.exec_module(F)
P = F.P
recs = P.read_dump("log/roadmap/S3/24_data/dump_cov_a_keys.bin")
r = recs[2397]
x0, y0 = r["box"][:2]; yy, xx = 396 - y0, 460 - x0
for vert in ("snap", "f32"):
    P.geometry = F.geometry_f32 if vert == "f32" else F.P_geometry
    cov0, g = P.coverage(r, "b8s1")
    for form, bk in F.VARIANTS:
        cov = cov0 if form == "s38" else np.where(cov0 > 0, F.cov_f32(r, g, form), 0).astype(np.float32)
        m = F.blend(r, cov, bk)
        print(vert, form, bk, "%04x" % m[yy, xx], "cov %.9f" % cov[yy, xx])
