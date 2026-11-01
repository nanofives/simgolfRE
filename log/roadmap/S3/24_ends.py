"""S3.24: the major-axis extent at the line ends. m = how far a pixel centre lies past the end (or before the start) along the
major axis, in the direction of travel. Per (width, angle), the largest m drawn and the smallest m the model draws visibly but the
real GL does not."""
import importlib.util, pathlib, sys, collections
import numpy as np
HERE = pathlib.Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("p24", HERE / "24_probe.py"); P = importlib.util.module_from_spec(spec); spec.loader.exec_module(P)
rule = sys.argv[1]
agg = {}
s = P.chan(0x7FFF)[0]
for p in sys.argv[2:]:
    for r in P.load(p):
        if r["alpha"] != 10 or r["colour"] != 0x7FFF or r["pre"].any():
            continue
        cov, g = P.coverage(r, rule)
        if g is None:
            continue
        dx, dy = g["bx"] - g["ax"], g["by"] - g["ay"]
        xmaj = abs(dx) >= abs(dy)
        Mc = (g["X"] if xmaj else g["Y"]) + 0.5
        a_m, b_m, sg = (g["ax"], g["bx"], np.sign(dx)) if xmaj else (g["ay"], g["by"], np.sign(dy))
        sg = sg or 1.0
        m_end = (Mc - b_m) * sg
        m_start = (a_m - Mc) * sg
        d = (P.K[g["Y"].astype(int) & 3, g["X"].astype(int) & 3] + 0.5) / 16.0
        vis = np.floor(31 * s * cov + d) >= 1
        drawn = r["real"] != 0
        major = max(abs(dx), abs(dy))
        ang = round(float(np.degrees(np.arctan2(min(abs(dx), abs(dy)), major))))
        key = (r["width"], ang)
        a = agg.setdefault(key, [-9, 99, -9, 99, 0])
        for mm, idx in ((m_end, 0), (m_start, 2)):
            sel = mm > -0.6
            if (drawn & sel).any():
                a[idx] = max(a[idx], float(mm[drawn & sel].max()))
            nd = vis & ~drawn & sel
            if nd.any():
                a[idx + 1] = min(a[idx + 1], float(mm[nd].min()))
                a[4] += int(nd.sum())
print("w deg | end: max drawn, min nd | start: max drawn, min nd | nd   (half+0.5)*tan + 0.5/cos")
for (w, deg), v in sorted(agg.items()):
    if v[4] or "ALL" in sys.argv[0:1]:
        print(w, deg, "| %.3f %.3f | %.3f %.3f | %d" % tuple(v), " %.3f" % ((w / 2 + 0.5) * np.tan(np.radians(deg)) + 0.5 / np.cos(np.radians(deg))))
