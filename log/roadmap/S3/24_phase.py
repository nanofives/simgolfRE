"""S3.24: per-pixel coverage of the real GL's antialiased line, read through the 16 dither phases.

Each line (integer ends, as glVertex2i gives them) is drawn white on black at alpha 1 in 16 copies shifted by (i, j), i, j in
0..3, so every pixel of the line's box is seen under all 16 dither offsets d = (K + 0.5) / 16. A copy's 5-bit value q satisfies
floor(31 * c + d) = q, so the 16 copies pin 31 * c to [max(q - d), min(q + 1 - d)); all 16 at 0 means c < 0.001 (not drawn).

    ~/bin/py -3.12 log/roadmap/S3/24_phase.py run <set> <out.npz>      (glreplay; take the heavy lock)
    ~/bin/py -3.12 log/roadmap/S3/24_phase.py cmp <out.npz> [--rule R] [--show N]
"""
import argparse
import importlib.util
import pathlib

import numpy as np

HERE = pathlib.Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("p24", HERE / "24_probe.py")
P = importlib.util.module_from_spec(spec)
spec.loader.exec_module(P)
K = P.K


def lines_of(name):
    rng = np.random.default_rng(7)
    out = []
    if name == "fan":       # every width at angles 0..90 in 3 degree steps, length 22
        for w in (1, 2, 3, 5, 10):
            for ang in range(0, 91, 3):
                a = np.radians(ang)
                out.append((0, 0, int(round(22 * np.cos(a))), int(round(22 * np.sin(a))), w))
    elif name == "rand":    # random integer offsets (any octant), widths of the game, lengths 1..30
        for _ in range(240):
            w = int(rng.choice([1, 2, 3, 5, 10]))
            dx, dy = int(rng.integers(-30, 31)), int(rng.integers(-30, 31))
            out.append((0, 0, dx, dy, w))
    elif name == "short":   # lengths 1..6 at every direction, the end caps
        for w in (1, 2, 3, 5, 10):
            for dx in range(-4, 5):
                for dy in range(-4, 5):
                    if (dx, dy) != (0, 0) and abs(dx) + abs(dy) <= 6:
                        out.append((0, 0, dx, dy, w))
    else:
        raise KeyError(name)
    return out


def build(name):
    """records: 16 copies per line; groups[g] = list of record indices with (i, j)"""
    recs, groups = [], []
    cx, cy, rowh = 30, 30, 0
    for (x1, y1, x2, y2, w) in lines_of(name):
        span_x = abs(x2 - x1) + 2 * (w + 6)
        span_y = abs(y2 - y1) + 2 * (w + 6)
        cell_w, cell_h = 4 * (span_x + 6), 4 * (span_y + 6)
        if cx + cell_w > 790:
            cx, cy, rowh = 10, cy + rowh, 0
        if cy + cell_h > 590:
            cx, cy, rowh = 10, 10, 0   # wraps: a new scene region; pack() separates overlapping boxes into scenes
        ox, oy = cx + w + 6 - min(x1, x2), cy + w + 6 - min(y1, y2)
        g = []
        for k in range(16):
            i, j = k % 4, k // 4
            bx, by = ox + i * (span_x + 6) + i, oy + j * (span_y + 6) + j
            r = dict(x1=x1 + bx, y1=y1 + by, x2=x2 + bx, y2=y2 + by, colour=0x7FFF, width=w, alpha=10, flip=1, lw=float(w),
                     sw=P.W, sh=P.H, fw=P.W)
            geo = P.geometry(r)
            ax, ay, bx2, by2, half = geo
            reach = half + 1.5 + 2
            r["box"] = (int(np.floor(min(ax, bx2) - reach)), int(np.floor(min(ay, by2) - reach)),
                        int(np.ceil(max(ax, bx2) + reach)), int(np.ceil(max(ay, by2) + reach)))
            x0, yy0, xe, ye = r["box"]
            r["pre"] = np.zeros((ye - yy0 + 1, xe - x0 + 1), "<u2")
            r["post"] = P.blend(r, P.coverage(r, "s38")[0])
            g.append(len(recs))
            recs.append(r)
        groups.append(g)
        cx += cell_w
        rowh = max(rowh, cell_h)
    return recs, groups


def intervals(recs, g):
    """(lo, hi, base): 31*c interval per pixel of the group's first copy's box (relative to its line start)"""
    r0 = recs[g[0]]
    shp = r0["real"].shape
    lo = np.zeros(shp)
    hi = np.full(shp, 31.0)
    for idx in g:
        r = recs[idx]
        if r["real"].shape != shp:
            return None
        x0, y0 = r["box"][:2]
        Y, X = np.mgrid[y0:y0 + shp[0], x0:x0 + shp[1]]
        d = (K[Y & 3, X & 3] + 0.5) / 16.0
        q = (r["real"].astype(int) >> 10) & 31
        lo = np.maximum(lo, np.where(q > 0, q - d, 0.0))
        hi = np.minimum(hi, np.where(q < 31, q + 1 - d, 31.0))
    return lo, hi


def cmd_run(a):
    recs, groups = build(a.set)
    print(len(groups), "lines,", len(recs), "records", flush=True)
    real = P.run_real(recs, "s324ph_" + a.set)
    P.save(a.out, recs, real)
    np.save(a.out.replace(".npz", "_groups.npy"), np.array(groups))


def cmd_cmp(a):
    recs = P.load(a.npz)
    groups = np.load(a.npz.replace(".npz", "_groups.npy"))
    tot = dict(lines=0, px_real=0, miss_notdrawn=0, miss_extra=0, miss_value=0, lines_bad=0)
    shown = 0
    for g in groups:
        iv = intervals(recs, g)
        if iv is None:
            continue
        lo, hi = iv
        r0 = recs[g[0]]
        cov, geo = P.coverage(r0, a.rule)
        c31 = 31.0 * P.chan(r0["colour"])[0] * cov   # the white of drawLine is 248 / 255
        notdrawn = hi <= 1.0 - 15.5 / 16 + 1e-9     # all 16 copies at 0
        drawn = lo > 0
        tol = 0.0
        a_nd = (cov > 0) & notdrawn & (c31 >= hi + tol)
        a_ex = (cov == 0) & drawn
        a_val = (cov > 0) & ~notdrawn & ((c31 < lo - tol) | (c31 >= hi + tol))
        tot["lines"] += 1
        tot["px_real"] += int(drawn.sum())
        tot["miss_notdrawn"] += int(a_nd.sum())
        tot["miss_extra"] += int(a_ex.sum())
        tot["miss_value"] += int(a_val.sum())
        bad = a_nd | a_ex | a_val
        tot["lines_bad"] += int(bad.any())
        if bad.any() and shown < a.show:
            shown += 1
            x0, y0 = r0["box"][:2]
            ang = np.degrees(np.arctan2(geo["by"] - geo["ay"], geo["bx"] - geo["ax"]))
            print(f"line ({r0['x1']},{r0['y1']})-({r0['x2']},{r0['y2']}) w {r0['width']} ang {ang:.1f} L {geo['L']:.2f}")
            for yy, xx in zip(*np.nonzero(bad)):
                kind = "ND" if a_nd[yy, xx] else ("EX" if a_ex[yy, xx] else "VAL")
                print(f"   {kind} px ({xx + x0 - r0['x1']},{yy + y0 - r0['y1']}) t {geo['t'][yy, xx]:.3f} n {geo['n'][yy, xx]:.3f} "
                      f"31c model {c31[yy, xx]:.3f} real [{lo[yy, xx]:.3f},{hi[yy, xx]:.3f})")
    print(a.rule, tot)


def main():
    ap = argparse.ArgumentParser()
    sp = ap.add_subparsers(dest="cmd", required=True)
    p = sp.add_parser("run")
    p.add_argument("set")
    p.add_argument("out")
    p = sp.add_parser("cmp")
    p.add_argument("npz")
    p.add_argument("--rule", default="s38")
    p.add_argument("--show", type=int, default=10)
    a = ap.parse_args()
    {"run": cmd_run, "cmp": cmd_cmp}[a.cmd](a)


if __name__ == "__main__":
    main()
