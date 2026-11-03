"""S3.31: the second-vertex hole rule (U-1826) as a Python model on top of rule 8 (31_score.py f32c:span_dir3), scored against
the real GL.

In the canonical frame of a segment (u along the major axis and v along the minor axis, both increasing from the first end a to
the second end b, origin b; s = |minor / major|), with o = v - u s the pixel centre's minor offset from the line centre, N the span
count of rule 2 and q = u + v s: the real GL generates no fragment where

    0 < frac(o + N/2) < 1 - s     and     q > 1 - 0.5 sqrt(1 + s^2)

(the pixel's window cell is the one its row held in the previous column, and it lies beyond the mirror, about the perpendicular
through b + (0.5, 0), of the end cap q = 0.5 sqrt(1 + s^2)); q is measured from b' = a + round(b - a) per axis (BPRIME), o from b. Variants (--variant <f>_<q>[_eps]): f lt / le, q gt / ge, eps a tolerance on the f test (lt: f < 1 - s - eps).

    ~/bin/py -3.12 log/roadmap/S3/31_holes.py <npz>... [--variant lt_gt|le_gt|lt_ge|le_ge]
"""
import argparse
import importlib.util
import pathlib
import sys

import numpy as np

HERE = pathlib.Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("s331s", HERE / "31_score.py")
S = importlib.util.module_from_spec(spec)
spec.loader.exec_module(S)
_argv = sys.argv
sys.argv = sys.argv[:1]
specc = importlib.util.spec_from_file_location("s331c", HERE / "31_cap.py")
C = importlib.util.module_from_spec(specc)
specc.loader.exec_module(C)
sys.argv = _argv
P, F = S.P, S.F


UNCLIPPED = True   # the holes are measured from the unclipped ends (a clipped end moves neither b nor b')
BPRIME = True   # q measured from b' = a + round(b - a) per axis (the first end plus the rounded delta), not from b


def unclipped_ends(r):
    """the ends through the float32 vertex stage without the clip (31_f32.py), as window coordinates"""
    fw, fh = r["fw"], (r.get("fh") or 600)
    top = not r["flip"]
    return (F.win_of_clip(F.clip_f32(r["x1"], fw, False), fw), F.win_of_clip(F.clip_f32(r["y1"], fh, top), fh),
            F.win_of_clip(F.clip_f32(r["x2"], fw, False), fw), F.win_of_clip(F.clip_f32(r["y2"], fh, top), fh))


def holes(g, variant="lt_gt", ends=None):
    """ends: the unclipped (a0x, a0y, b0x, b0y); the holes are measured from them (a clipped end moves neither b nor b')"""
    if ends is not None:
        g = dict(g)
        g["ax"], g["ay"], g["bx"], g["by"] = ends
    dx, dy = g["bx"] - g["ax"], g["by"] - g["ay"]
    major = max(abs(dx), abs(dy))
    if major == 0:
        return np.zeros(g["X"].shape, bool)
    u, v, ua, va, s, xmaj, sM, sm = C.canon(g)
    if BPRIME:
        bpx, bpy = g["ax"] + np.round(dx), g["ay"] + np.round(dy)
        X, Y = g["X"] + 0.5, g["Y"] + 0.5
        Mc, mc = (X, Y) if xmaj else (Y, X)
        bM, bm = (bpx, bpy) if xmaj else (bpy, bpx)
        uq, vq = (Mc - bM) * sM, (mc - bm) * sm
    else:
        uq, vq = u, v
    n_px = np.floor(2.0 * g["half"] * np.hypot(dx, dy) / major) + 2.0
    o = v - u * s
    f = (o + n_px / 2.0) % 1.0
    q = uq + vq * s
    qlim = 1.0 - 0.5 * np.sqrt(1.0 + s * s)
    fl, qg = variant.split("_")[:2]
    eps = float(variant.split("_")[2]) if variant.count("_") == 2 else 0.0
    fin = (f <= 1.0 - s + eps) if fl == "le" else (f < 1.0 - s - eps)
    fin = fin & (f > eps)   # f exactly 0 (an exact cell boundary) is no hole (31_notes.md)
    qin = (q >= qlim) if qg == "ge" else (q > qlim)
    return fin & qin


def model(r, variant):
    P.geometry = F.geometry_f32
    F.CLIP_MODE = "clip"
    try:
        cov, g = P.coverage(r, "span_dir3")
    finally:
        P.geometry = F.P_geometry
    if g is None:
        return P.blend(r, cov)
    e = unclipped_ends(r) if UNCLIPPED else None
    if e is not None and (g["ax"], g["ay"]) != (e[0], e[1]):
        return P.blend(r, cov)   # the clip moved the first end: no holes (U-1982)
    cov = np.where(holes(g, variant, e), 0.0, cov)
    return P.blend(r, cov)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("npz", nargs="+")
    ap.add_argument("--variant", default="lt_gt")
    a = ap.parse_args()
    for p in a.npz:
        tot = dict(rec=0, r8=0, holes=0, new=0, fixed=0, beyond_r8=0, beyond_h=0)
        for r in P.load(p):
            if r["sw"] != 800 or r["sh"] != 600 or r["fw"] != 800:
                continue
            b = P.diff5(S.model(r, "f32c:span_dir3"), r["real"])
            h = P.diff5(model(r, a.variant), r["real"])
            tot["rec"] += 1
            tot["r8"] += int((b > 0).sum())
            tot["holes"] += int((h > 0).sum())
            tot["new"] += int(((h > 0) & (b == 0)).sum())
            tot["fixed"] += int(((b > 0) & (h == 0)).sum())
            tot["beyond_r8"] += int((b > 1).sum())
            tot["beyond_h"] += int((h > 1).sum())
        print(pathlib.Path(p).name, a.variant, tot, flush=True)


if __name__ == "__main__":
    main()
