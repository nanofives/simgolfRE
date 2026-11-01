"""S3.24: list the pixels a rule gets wrong that S3.8's model got right (o from the rule's geometry)."""
import importlib.util, sys
import numpy as np
spec = importlib.util.spec_from_file_location("p24", "log/roadmap/S3/24_probe.py"); P = importlib.util.module_from_spec(spec); spec.loader.exec_module(P)
rule = sys.argv[1]
for p in sys.argv[2:]:
    for i, r in enumerate(P.load(p)):
        c0, g = P.coverage(r, "s38"); c1, _ = P.coverage(r, rule)
        b0 = P.diff5(P.blend(r, c0), r["real"]) > 0; b1 = P.diff5(P.blend(r, c1), r["real"]) > 0
        nb = b1 & ~b0
        if not nb.any(): continue
        o, xmaj = P.minor_offset(g)
        dx, dy = g["bx"] - g["ax"], g["by"] - g["ay"]; major = max(abs(dx), abs(dy))
        N = np.floor(2 * g["half"] * g["L"] / major) + 2
        rd = r["real"] != r["pre"]
        for yy, xx in zip(*np.nonzero(nb)):
            print(p.split("/")[-1][:8], i, "w", r["width"], "ends", (r["x1"], r["y1"], r["x2"], r["y2"]), "xmaj", int(xmaj),
                  "o %.6f N/2 %.1f wL/maj %.5f" % (o[yy, xx], N / 2, 2 * g["half"] * g["L"] / major), "cov %.3f" % c0[yy, xx],
                  "t %.2f L %.2f" % (g["t"][yy, xx], g["L"]), "real_drawn", int(rd[yy, xx]), "rule_cov %.3f" % c1[yy, xx])
