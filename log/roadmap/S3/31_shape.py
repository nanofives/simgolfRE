"""S3.31: the shape of the second-vertex gap (U-1826 / U-1977) in the canonical plane, sampled densely.

One direction (dx, dy) and width is drawn white on black in 256 copies, each shifted by (i / 16, j / 16) px (glVertex3f ends), so
the pixel centres around the second vertex b sample the canonical plane (u major, v minor, origin b) at 1/16 px. Each sample is
`D` (the model draws it visibly, the real GL does not), `K` (both draw it), `x` (the real GL draws it, the model does not) or
`.`; `d` = a drop whose model value is 1 (a dither margin), `D` = model value 2 or more. Model = line rule 8 (31_score.py f32c:span_dir3) with the float ends.

    ~/bin/py -3.12 log/roadmap/S3/31_shape.py run <dx> <dy> <w> <out.npz>      (glreplay)
    ~/bin/py -3.12 log/roadmap/S3/31_shape.py map <out.npz> [--umin -3 --umax 4 --vmin -5 --vmax 5 --step 0.125]
"""
import argparse
import importlib.util
import pathlib
import struct

import numpy as np

HERE = pathlib.Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("s331s", HERE / "31_score.py")
S = importlib.util.module_from_spec(spec)
spec.loader.exec_module(S)
specc = importlib.util.spec_from_file_location("s331c", HERE / "31_cap.py")
C = importlib.util.module_from_spec(specc)
import sys  # noqa: E402
_argv = sys.argv
sys.argv = sys.argv[:1]
specc.loader.exec_module(C)
sys.argv = _argv
P, F = S.P, S.F
f32 = np.float32


def fb(x):
    return "0x%x" % struct.unpack("<I", struct.pack("<f", float(x)))[0]


def win(v, s_, top=False):
    return F.win_of_clip(F.clip_f32(v, s_, top), s_)


def geometry_float(r):
    """the float32 vertex stage for float object coordinates (the ends of this probe stay inside the viewport)"""
    return win(r["fx1"], 800), win(r["fy1"], 600), win(r["fx2"], 800), win(r["fy2"], 600), min(float(r["lw"]), 10.0) / 2.0


def build(dx, dy, w):
    recs = []
    span_x, span_y = abs(dx) + 2 * (w + 8), abs(dy) + 2 * (w + 8)
    cols = max(1, 760 // span_x)
    k = 0
    for j in range(16):
        for i in range(16):
            rows_ = max(1, 560 // span_y)
            cx, cy = 20 + (k % cols) * span_x, 20 + ((k // cols) % rows_) * span_y
            k += 1
            ox, oy = cx + w + 8 - min(0, dx) + i / 16.0, cy + w + 8 - min(0, dy) + j / 16.0
            r = dict(fx1=ox, fy1=oy, fx2=ox + dx, fy2=oy + dy, x1=int(ox), y1=int(oy), x2=int(ox + dx), y2=int(oy + dy), colour=0x7FFF,
                     width=w, alpha=10, flip=1, lw=float(w), sw=800, sh=600, fw=800)
            ax, ay, bx, by, half = geometry_float(r)
            reach = half + 3.5
            r["box"] = (int(np.floor(min(ax, bx) - reach)), int(np.floor(min(ay, by) - reach)),
                        int(np.ceil(max(ax, bx) + reach)), int(np.ceil(max(ay, by) + reach)))
            if r["box"][3] > 599:
                raise RuntimeError("layout overflow")
            x0, y0, x1, y1 = r["box"]
            r["pre"] = np.zeros((y1 - y0 + 1, x1 - x0 + 1), "<u2")
            r["post"] = r["pre"].copy()
            recs.append(r)
    return recs


def cmd_run(a):
    recs = build(a.dx, a.dy, a.w)
    orig = P.calls

    def calls(r):
        out = orig(r)
        res = []
        for c in out:
            if c.startswith("glVertex2i"):
                continue
            res.append(c)
            if c == "glBegin 0x1":
                res += ["glVertex3f %s,%s,%s" % (fb(r["fx1"]), fb(r["fy1"]), fb(0.0)),
                        "glVertex3f %s,%s,%s" % (fb(r["fx2"]), fb(r["fy2"]), fb(0.0))]
        return res
    P.calls = calls
    try:
        real = P.run_real(recs, f"s331_shape_{a.dx}_{a.dy}_{a.w}")
    finally:
        P.calls = orig
    np.savez_compressed(a.out, meta=np.array([a.dx, a.dy, a.w]), boxes=np.array([r["box"] for r in recs]),
                        ends=np.array([[r["fx1"], r["fy1"], r["fx2"], r["fy2"]] for r in recs]),
                        **{f"real{i}": real[i] for i in range(len(recs))})


def samples(npz):
    z = np.load(npz)
    dx, dy, w = (int(v) for v in z["meta"])
    out = []
    for i, (box, e) in enumerate(zip(z["boxes"], z["ends"])):
        r = dict(fx1=e[0], fy1=e[1], fx2=e[2], fy2=e[3], x1=int(e[0]), y1=int(e[1]), x2=int(e[2]), y2=int(e[3]), colour=0x7FFF,
                 width=w, alpha=10, flip=1, lw=float(w), sw=800, sh=600, fw=800, box=tuple(int(v) for v in box))
        x0, y0, x1, y1 = r["box"]
        r["pre"] = np.zeros((y1 - y0 + 1, x1 - x0 + 1), "<u2")
        P.geometry = geometry_float
        try:
            cov, g = P.coverage(r, "span_dir3")
        finally:
            P.geometry = F.P_geometry
        m = P.blend(r, cov)
        real = z[f"real{i}"]
        u, v, ua, va, s, xmaj, sM, sm = C.canon(g)
        tl = g["t"] - g["L"]
        vis, rd = m != 0, real != 0
        mv = (m.astype(int) >> 10) & 31
        sel = (tl > -3) & (vis | rd)
        out.append(np.stack([u[sel], v[sel], vis[sel], rd[sel], g["n"][sel], tl[sel], mv[sel]], 1))
    return np.concatenate(out), s, w


def cmd_map(a):
    A, s, w = samples(a.npz)
    print(f"slope {s:.4f} w {w}: {len(A)} samples, D {int(((A[:, 2] == 1) & (A[:, 3] == 0)).sum())}, x {int(((A[:, 2] == 0) & (A[:, 3] == 1)).sum())}")
    print("rows v (top = largest), columns u; '|' marks u + v s = 0.5 (q), '#' the cap q = 0.5 sqrt(1 + s^2)")
    rs = (1 + s * s) ** 0.5
    us = np.arange(a.umin, a.umax + 1e-9, a.step)
    for vv in np.arange(a.vmax, a.vmin - 1e-9, -a.step):
        line = "%+6.2f " % vv
        for uu in us:
            sel = (np.abs(A[:, 0] - uu) < a.step / 2) & (np.abs(A[:, 1] - vv) < a.step / 2)
            q = uu + vv * s
            if not sel.any():
                ch = "|" if abs(q - 0.5) < a.step * 0.6 else ("#" if abs(q - 0.5 * rs) < a.step * 0.6 else " ")
            else:
                vis, rd = A[sel, 2], A[sel, 3]
                mv = A[sel, 6]
                if ((vis == 1) & (rd == 0) & (mv >= 2)).any():
                    ch = "D"
                elif ((vis == 1) & (rd == 0)).any():
                    ch = "d"
                elif ((vis == 0) & (rd == 1)).any():
                    ch = "x"
                elif (rd == 1).any():
                    ch = "K"
                else:
                    ch = "."
            line += ch
        print(line)


def main():
    ap = argparse.ArgumentParser()
    sp = ap.add_subparsers(dest="cmd", required=True)
    p = sp.add_parser("run")
    p.add_argument("dx", type=int)
    p.add_argument("dy", type=int)
    p.add_argument("w", type=int)
    p.add_argument("out")
    p = sp.add_parser("map")
    p.add_argument("npz")
    p.add_argument("--umin", type=float, default=-3)
    p.add_argument("--umax", type=float, default=4)
    p.add_argument("--vmin", type=float, default=-5)
    p.add_argument("--vmax", type=float, default=5)
    p.add_argument("--step", type=float, default=0.125)
    a = ap.parse_args()
    {"run": cmd_run, "map": cmd_map}[a.cmd](a)


if __name__ == "__main__":
    main()
