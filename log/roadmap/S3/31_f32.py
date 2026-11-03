"""S3.31: the overlay line's VALUE arithmetic (U-1829 / U-1371): float32 formulations of the coverage and the blend, scored against
the real GL on records of S3.24's sets (24_data/*.npz: pre, port post, real post per record).

Only pixels the real GL and the model both draw are scored (the footprint is 31_cap.py's subject); a channel value counts as wrong
when the variant's 5-bit value differs from the real one.

    ~/bin/py -3.12 log/roadmap/S3/31_f32.py <npz>... [--variants a,b] [--show N]
"""
import argparse
import importlib.util
import pathlib

import numpy as np

HERE = pathlib.Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("p24", HERE / "24_probe.py")
P = importlib.util.module_from_spec(spec)
spec.loader.exec_module(P)
f32 = np.float32


def win_f32(v, s_, flip_axis_top):
    """S3.21's vertex rule for one axis of glVertex2i under glOrtho(0, s, 0, s) (or (s, 0) for a top-down axis), identity modelview,
    viewport 0..s: clip = m12 + v * m0 in float32, window = float32(ndc * float32(s / 2 - 2^-8)) + s / 2, rounded half to even to 2^-8"""
    if flip_axis_top:   # glOrtho bottom = s, top = 0
        m0, m12 = f32(2.0 / (0 - s_)), f32(-(0 + s_) / (0 - s_))
    else:
        m0, m12 = f32(2.0 / s_), f32(-1.0)
    clip = f32(m12 + f32(f32(v) * m0))
    px = f32(clip * f32(s_ / 2.0 - 1.0 / 256.0))
    w = float(px) + s_ / 2.0
    return np.round(256.0 * w) / 256.0    # numpy round = half to even


def clip_f32(v, s_, top):
    if top:
        m0, m12 = f32(2.0 / (0 - s_)), f32(-(0 + s_) / (0 - s_))
    else:
        m0, m12 = f32(2.0 / s_), f32(-1.0)
    return f32(m12 + f32(f32(v) * m0))


def win_of_clip(c, s_):
    px = f32(c * f32(s_ / 2.0 - 1.0 / 256.0))
    return np.round(256.0 * (float(px) + s_ / 2.0)) / 256.0


CLIP_MODE = "window"   # window: S3.24's window-space clip of the snapped ends; clip: float32 clip-space segment clip


def geometry_f32_clip(r):
    """S3.21's vertex rule with the segment clipped in clip space in float32: planes x >= -w, x <= w, y >= -w, y <= w in that
    order; t = d_in / (d_in - d_out), the new end = in + t (out - in) per coordinate, from the inside vertex (w = 1 throughout)"""
    fw, fh = r["fw"], (r.get("fh") or 600)
    a = [clip_f32(r["x1"], fw, False), clip_f32(r["y1"], fh, not r["flip"])]
    b = [clip_f32(r["x2"], fw, False), clip_f32(r["y2"], fh, not r["flip"])]
    one = f32(1.0)
    for k, sg in ((0, 1), (0, -1), (1, 1), (1, -1)):
        da = f32(one + a[k]) if sg == 1 else f32(one - a[k])
        db = f32(one + b[k]) if sg == 1 else f32(one - b[k])
        if da < 0 and db < 0:
            return None
        if da < 0 or db < 0:
            inn, out, din, dout = (b, a, db, da) if da < 0 else (a, b, da, db)
            t = f32(din / f32(din - dout))
            new = [f32(inn[j] + f32(t * f32(out[j] - inn[j]))) for j in range(2)]
            if da < 0:
                a = new
            else:
                b = new
    ax, ay = win_of_clip(a[0], fw), win_of_clip(a[1], fh)
    bx, by = win_of_clip(b[0], fw), win_of_clip(b[1], fh)
    return ax, ay, bx, by, min(float(r["lw"]), 10.0) / 2.0


def geometry_f32(r):
    if CLIP_MODE == "clip":
        fw, fh = r["fw"], (r.get("fh") or 600)
        if fw == r["sw"] and fh == r["sh"]:
            return geometry_f32_clip(r)
    fw, fh = r["fw"], (r.get("fh") or 600)
    if fw != r["sw"] or fh != r["sh"]:
        return P_geometry(r)
    ax, bx = win_f32(r["x1"], fw, False), win_f32(r["x2"], fw, False)
    ay, by = win_f32(r["y1"], fh, not r["flip"]), win_f32(r["y2"], fh, not r["flip"])
    if not r["flip"]:
        pass   # the window y of the top-down ortho is already the row coordinate the model uses (sh - y)
    c = P.clip(ax, ay, bx, by, fw, fh)
    if c is None:
        return None
    ax, ay, bx, by = c
    return ax, ay, bx, by, min(float(r["lw"]), 10.0) / 2.0


P_geometry = P.geometry


def cov_f32(r, g, form):
    """coverage with the distances evaluated in float32; form selects the operation order"""
    ax, ay, bx, by = (f32(g[k]) for k in ("ax", "ay", "bx", "by"))
    half = f32(g["half"])
    X, Y = g["X"].astype(f32), g["Y"].astype(f32)
    px, py = (X + f32(0.5)) - ax, (Y + f32(0.5)) - ay
    dx, dy = bx - ax, by - ay
    L = np.sqrt(dx * dx + dy * dy, dtype=f32)
    if form == "unit":            # unit vector first, then dot products
        ux, uy = dx / L, dy / L
        t = px * ux + py * uy
        n = -px * uy + py * ux
    elif form == "recip":         # 1 / L once, unit vector by multiplication
        il = f32(1.0) / L
        ux, uy = dx * il, dy * il
        t = px * ux + py * uy
        n = -px * uy + py * ux
    elif form == "cross":         # cross / dot over L
        t = (px * dx + py * dy) / L
        n = (py * dx - px * dy) / L
    elif form == "crossr":
        il = f32(1.0) / L
        t = (px * dx + py * dy) * il
        n = (py * dx - px * dy) * il
    else:
        raise KeyError(form)
    one, h5 = f32(1.0), f32(0.5)
    side = np.clip(half + h5 - np.abs(n), 0, 1).astype(f32)
    c0 = np.clip(t + h5, 0, 1).astype(f32)
    c1 = np.clip(L - t + h5, 0, 1).astype(f32)
    return (side * c0 * c1).astype(f32)


def blend(r, cov, kind):
    x0, y0, x1, y1 = r["box"]
    cr, cg, cb = P.chan(r["colour"])
    ca = min(max(float(f32(r["alpha"]) / f32(10.0)), 0.0), 1.0)
    d = r["pre"].astype(np.int64)
    Yi, Xi = np.mgrid[y0:y1 + 1, x0:x1 + 1]
    dth = (P.K[Yi & 3, Xi & 3] + 0.5) / 16.0
    out = 0
    for sh, s in ((10, cr), (5, cg), (0, cb)):
        dc = (d >> sh) & 31
        if kind == "s38":
            al = ca * cov.astype(float)
            v = s * 255.0 * al + dc * 255.0 / 31.0 * (1.0 - al)
            q = np.floor(v * 31.0 / 255.0 + dth)
        elif kind == "unit32":    # [0, 1] floats in single precision, dst = dc / 31
            al = (f32(ca) * cov).astype(f32)
            v = f32(s) * al + (dc.astype(f32) / f32(31.0)) * (f32(1.0) - al)
            q = np.floor(v * f32(31.0) + dth.astype(f32))
        elif kind == "scaled32":  # colours scaled to the 5-bit range first (SGI-style redScale), single precision
            al = (f32(ca) * cov).astype(f32)
            v = (f32(s) * f32(31.0)) * al + dc.astype(f32) * (f32(1.0) - al)
            q = np.floor(v + dth.astype(f32))
        elif kind == "scaled32b": # src * a + dst - dst * a
            al = (f32(ca) * cov).astype(f32)
            sv = f32(s) * f32(31.0)
            dv = dc.astype(f32)
            v = sv * al + dv - dv * al
            q = np.floor(v + dth.astype(f32))
        elif kind == "scaled32c": # dst + (src - dst) * a
            al = (f32(ca) * cov).astype(f32)
            sv = f32(s) * f32(31.0)
            dv = dc.astype(f32)
            v = dv + (sv - dv) * al
            q = np.floor(v + dth.astype(f32))
        else:
            raise KeyError(kind)
        q = np.clip(q, 0, 31).astype(np.int64)
        out = out | (q << sh)
    return out


VARIANTS = [("s38", "s38")] + [(f, b) for f in ("unit", "recip", "cross", "crossr") for b in ("s38", "unit32", "scaled32", "scaled32b", "scaled32c")]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("npz", nargs="+")
    ap.add_argument("--show", type=int, default=0)
    ap.add_argument("--vertex", choices=("snap", "f32"), default="snap", help="the line ends: S3.24's double snap or S3.21's float32 rule")
    a = ap.parse_args()
    if a.vertex == "f32":
        P.geometry = geometry_f32
    score = {v: [0, 0] for v in VARIANTS}
    shown = 0
    for p in a.npz:
        for r in P.load(p):
            cov0, g = P.coverage(r, "b8s1")
            if g is None:
                continue
            m0 = P.blend(r, cov0)
            both = (m0 != r["pre"]) & (r["real"] != r["pre"]) & (cov0 > 0)
            if not both.any():
                continue
            for v in VARIANTS:
                form, bk = v
                cov = cov0 if form == "s38" else np.where(cov0 > 0, cov_f32(r, g, form), 0).astype(f32)
                m = blend(r, cov, bk)
                for sh in (10, 5, 0):
                    a_ = (m[both] >> sh) & 31
                    b_ = (r["real"][both].astype(np.int64) >> sh) & 31
                    score[v][0] += int((a_ != b_).sum())
                    score[v][1] += int(a_.size)
            if a.show and shown < a.show:
                d = (P.diff5(m0, r["real"]) > 0) & both
                for yy, xx in zip(*np.nonzero(d)):
                    shown += 1
                    print("  wrong", (r["x1"], r["y1"], r["x2"], r["y2"]), "w", r["width"], "a", r["alpha"], "px", xx + r["box"][0], yy + r["box"][1],
                          "model %04x real %04x pre %04x cov %.7f" % (m0[yy, xx], r["real"][yy, xx], r["pre"][yy, xx], cov0[yy, xx]))
    for v, (bad, n) in sorted(score.items(), key=lambda kv: kv[1][0]):
        print(f"{bad:6d} / {n}  {v[0]:7s} {v[1]}")


if __name__ == "__main__":
    main()
