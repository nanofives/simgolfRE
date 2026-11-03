"""real side coverage of many side pixels of line (42, 15) w 2 (alpha bisection on black, 31_alpha.focus); JSON rows"""
import importlib.util, sys, json, math
args = sys.argv[1:]; sys.argv = ["x"]
spec = importlib.util.spec_from_file_location("a", "log/roadmap/S3/31_alpha.py"); A = importlib.util.module_from_spec(spec); spec.loader.exec_module(A)
A.PRE = 0
dx, dy, w = int(args[0]), int(args[1]), 2
L = math.hypot(dx, dy); ux, uy = dx / L, dy / L
out = []
for x in range(3, dx - 2, max(1, dx // 12)):
    for y in range(-3, dy + 4):
        px, py = x + 0.5, y + 0.5
        n = -px * uy + py * ux; t = px * ux + py * uy
        if not (0.52 < abs(n) < 1.4 and 3 < t < L - 3): continue
        c = 1.5 - abs(n)
        level = max(2, int(30.149 * c * 0.92))
        try:
            br, ref = A.focus(dx, dy, w, (x, y), iters=7, level=level)
        except Exception as e:
            print("skip", (x, y), e, flush=True); continue
        pl, ph = br["pix"]; rl, rh = br["ref"]
        row = dict(x=x, y=y, n=n, c=c, c_lo=rl / ph, c_hi=rh / pl)
        out.append(row)
        print(json.dumps(row), flush=True)
json.dump(out, open("log/roadmap/S3/31_data/nline_%s_%s.json" % (args[0], args[1]), "w"), indent=1)
