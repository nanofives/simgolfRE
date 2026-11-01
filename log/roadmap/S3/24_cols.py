"""S3.24: per-column listing of one phase-set line: minor positions the model covers (with o) and which the real GL drew."""
import importlib.util, pathlib, sys
import numpy as np
HERE = pathlib.Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("p24", HERE / "24_probe.py"); P = importlib.util.module_from_spec(spec); spec.loader.exec_module(P)
npz, w, dx, dy = sys.argv[1], int(sys.argv[2]), int(sys.argv[3]), int(sys.argv[4])
ncopy = int(sys.argv[5]) if len(sys.argv) > 5 else 2
recs = [r for r in P.load(npz) if r["width"] == w and r["x2"] - r["x1"] == dx and r["y2"] - r["y1"] == dy]
s = P.chan(0x7FFF)[0]
for r in recs[:ncopy]:
    cov, g = P.coverage(r, "s38")
    print(f"copy start ({r['x1']},{r['y1']}) snapped a=({g['ax']:.5f},{g['ay']:.5f}) b=({g['bx']:.5f},{g['by']:.5f})")
    X, Y = g["X"] + 0.5, g["Y"] + 0.5
    xmaj = abs(dx) >= abs(dy)
    d = (P.K[g["Y"].astype(int) & 3, g["X"].astype(int) & 3] + 0.5) / 16.0
    vis = np.floor(31 * s * cov + d) >= 1
    if xmaj:
        o = Y - (g["ay"] + (X - g["ax"]) * (g["by"] - g["ay"]) / (g["bx"] - g["ax"]))
        lines = [(int(g["X"][0, c]), o[:, c], cov[:, c], r["real"][:, c], vis[:, c]) for c in range(cov.shape[1])]
    else:
        o = X - (g["ax"] + (Y - g["ay"]) * (g["bx"] - g["ax"]) / (g["by"] - g["ay"]))
        lines = [(int(g["Y"][rr, 0]), o[rr, :], cov[rr, :], r["real"][rr, :], vis[rr, :]) for rr in range(cov.shape[0])]
    for major, oo, cc, rr, vv in lines:
        idx = np.nonzero(cc > 0)[0]
        if not len(idx):
            continue
        cells = []
        for i in idx:
            mark = "D" if rr[i] else ("x" if vv[i] else ".")
            cells.append(f"{oo[i]:+.3f}{mark}")
        print(f"  {major:4d} " + " ".join(cells))
