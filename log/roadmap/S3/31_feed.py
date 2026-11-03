"""S3.31 (U-1981): the window coordinates the real GL gives drawLine's glVertex2i ends, read in GL_FEEDBACK mode
(log/roadmap/S3/21_glfeed/glfeed.exe with GLFEED_POINTS=1: one point token per vertex), against line rule 8's float32 vertex stage
(31_f32.py win_f32) and S3.24's double snap.

    ~/bin/py -3.12 log/roadmap/S3/31_feed.py [--flip 1|0]
"""
import argparse
import importlib.util
import os
import pathlib
import subprocess
import sys

import numpy as np

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parents[2]
spec = importlib.util.spec_from_file_location("s331f", HERE / "31_f32.py")
F = importlib.util.module_from_spec(spec)
spec.loader.exec_module(F)
P = F.P
FEED = HERE / "21_glfeed" / "glfeed.exe"


def run(points, flip):
    sc = P.PR.Scene() if hasattr(P, "PR") else None
    from port.test import glprobe as G
    sc = G.Scene()
    sc.state += P.INIT
    for k in range(0, len(points), 2):
        (x1, y1), (x2, y2) = points[k], points[k + 1] if k + 1 < len(points) else points[k]
        r = dict(x1=x1, y1=y1, x2=x2, y2=y2, colour=0x7FFF, width=1, alpha=10, lw=1.0, flip=flip, sw=800, sh=600)
        sc.pass_ += P.calls(r)
    d = sc.write("s331_feed_%d" % flip)
    out = d / "feed.f32"
    env = dict(os.environ, GLFEED_POINTS="1")
    r = subprocess.run([str(FEED), str(d), "s1", str(out)], capture_output=True, text=True, env=env, timeout=300)
    a = np.fromfile(out, dtype="<f4")
    n = int(a[0])
    vals = a[1:1 + n]
    pts, i = [], 0
    while i < len(vals):
        tok = int(vals[i])
        if tok == 0x0701:            # GL_POINT_TOKEN, then x y z r g b a s t r q
            pts.append((float(vals[i + 1]), float(vals[i + 2])))
            i += 12
        else:
            raise RuntimeError("token 0x%x at %d (%s)" % (tok, i, r.stdout[-200:]))
    return pts


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--flip", type=int, default=1)
    a = ap.parse_args()
    sys.path.insert(0, str(ROOT))
    xs = list(range(-20, 821))
    ys = list(range(-20, 621))
    pts = [(x, 300) for x in xs] + [(400, y) for y in ys]
    if len(pts) % 2:
        pts.append((400, 300))
    got = run(pts, a.flip)
    print(len(pts), "vertices,", len(got), "feedback points (points outside the view volume are clipped away)")
    # map back: feedback order follows the vertex order of the visible points
    vis = [(x, y) for (x, y) in pts if 0 <= x <= 800 and 0 <= y <= 600]
    bad_f32 = bad_snap = 0
    rows = []
    for (x, y), (fx, fy) in zip(vis, got):
        ex, ey = F.win_f32(x, 800, False), F.win_f32(y, 600, not a.flip)
        sx = P.snap(float(x), 400.0, 400.0)
        sy = P.snap(float(y) if a.flip else 600.0 - y, 300.0, 300.0)
        if (fx, fy) != (ex, ey):
            bad_f32 += 1
            rows.append(("f32", x, y, fx, fy, ex, ey))
        if (fx, fy) != (sx, sy):
            bad_snap += 1
    print("flip", a.flip, "float32 rule differs at", bad_f32, "| double snap differs at", bad_snap)
    for r in rows[:30]:
        print("  %s (%d,%d): real (%.6f, %.6f) model (%.6f, %.6f)" % r)


if __name__ == "__main__":
    main()
