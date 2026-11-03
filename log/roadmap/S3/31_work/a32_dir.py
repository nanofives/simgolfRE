"""side coverage with a float32 unit direction against the measured pixels (nline_42_15.json + the three focus pixels)"""
import json, numpy as np
f32 = np.float32
rows = [r for r in json.load(open("log/roadmap/S3/31_data/nline_42_15.json")) if r["c"] > 0.385]
rows += [dict(x=36, y=12, c_lo=0.995496213, c_hi=0.995496350), dict(x=33, y=12, c_lo=0.995494387, c_hi=0.995494521),
         dict(x=30, y=11, c_lo=0.928227254, c_hi=0.928227374)]
dx, dy = 42.0, 15.0
L = np.sqrt(dx * dx + dy * dy)
V = {
    "exact": (dx / L, dy / L),
    "f32(dx/L)": (float(f32(dx / L)), float(f32(dy / L))),
    "f32 div f32 L": (float(f32(f32(dx) / f32(np.sqrt(f32(dx * dx + dy * dy))))), float(f32(f32(dy) / f32(np.sqrt(f32(dx * dx + dy * dy)))))),
    "f32 * f32 1/L": (float(f32(f32(dx) * f32(f32(1) / f32(np.sqrt(f32(1989.0)))))), float(f32(f32(dy) * f32(f32(1) / f32(np.sqrt(f32(1989.0))))))),
}
for name, (ux, uy) in V.items():
    ok = 0
    errs = []
    for r in rows:
        px, py = r["x"] + 0.5, r["y"] + 0.5
        n = -px * uy + py * ux
        c = 1.5 - abs(n) if abs(n) > 0.5 else 1.0
        inside = r["c_lo"] - 2e-8 <= c <= r["c_hi"] + 2e-8
        ok += inside
        mid = (r["c_lo"] + r["c_hi"]) / 2
        errs.append(c - mid)
    print("%-16s within %2d / %d   max |c - real| %.2e" % (name, ok, len(rows), max(abs(e) for e in errs)))
