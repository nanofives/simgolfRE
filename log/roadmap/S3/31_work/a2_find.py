import importlib.util, sys
import numpy as np
spec = importlib.util.spec_from_file_location("p24", "log/roadmap/S3/24_probe.py"); P = importlib.util.module_from_spec(spec); spec.loader.exec_module(P)
path, px, py = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
recs = P.read_dump(path)
print(len(recs), "records")
for i, r in enumerate(recs):
    x0, y0, x1, y1 = r["box"]
    if x0 <= px <= x1 and y0 <= py <= y1:
        a = r["pre"][py - y0, px - x0]; b = r["post"][py - y0, px - x0]
        print(i, (r["x1"], r["y1"], r["x2"], r["y2"]), hex(r["colour"]), "w", r["width"], "a", r["alpha"], "lw", r["lw"], "flip", r["flip"],
              "pre %04x post %04x" % (a, b), "changed" if a != b else "")
