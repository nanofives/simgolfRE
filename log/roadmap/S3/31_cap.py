"""S3.31: the second-vertex end cap of wide antialiased lines (U-1826) and the line value (U-1829), against Windows' generic GL.

Reuses S3.24's 16-dither-phase method (24_phase.py: each line drawn white on black at alpha 1 in 16 copies shifted by (i, j),
so a pixel's 31 * c is pinned to an interval) with line sets aimed at the cap:

    ~/bin/py -3.12 log/roadmap/S3/31_cap.py run <set> <out.npz>       (glreplay; under w8_lock)
    ~/bin/py -3.12 log/roadmap/S3/31_cap.py rows <out.npz> [--rule R]  one row per cap-zone pixel (t > L - 1.5) as TSV

Sets:
    len    slopes 3/5, 5/8, 7/11, 2/3, 9/14 and others in the 0.55 .. 0.75 band, every length multiple, widths 3 / 5 / 10, +x +y
    oct    the len set's lines in all 8 octants (does the drop follow the second vertex in every direction?)
    rev    the len set with the vertices swapped
    fine   widths 2..10 (all integers), slopes p/q with q <= 16, major 8..40
    sweep  single copies (`run1`): every slope p/q in [0, 1] with q <= 48 (major 24..48), widths 3 / 5 / 10, +x +y
    sweep8 single copies (`run1`): the sweep's slopes with q <= 24 in all 8 octants, width 5
    tie8   single copies (`run1`): small-denominator slopes in all 8 octants, widths 3 / 5 / 10 (exact ties of the hole rule)
"""
import argparse
import importlib.util
import pathlib
import sys

import numpy as np

HERE = pathlib.Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("p24", HERE / "24_probe.py")
P = importlib.util.module_from_spec(spec)
spec.loader.exec_module(P)
spec2 = importlib.util.spec_from_file_location("p24ph", HERE / "24_phase.py")
PH = importlib.util.module_from_spec(spec2)
spec2.loader.exec_module(PH)

SLOPES = [(3, 5), (5, 8), (7, 11), (2, 3), (9, 14), (4, 7), (5, 7), (8, 13), (11, 17), (12, 19), (13, 20), (1, 2), (3, 4)]


def lines_of(name):
    out = []
    if name in ("len", "oct", "rev"):
        for (p, q) in SLOPES:
            for k in range(1, 60):
                if q * k > 48:
                    break
                if q * k < 4:
                    continue
                for w in (3, 5, 10):
                    dx, dy = q * k, p * k
                    if name == "len":
                        out.append((0, 0, dx, dy, w))
                    elif name == "rev":
                        out.append((dx, dy, 0, 0, w))
                    else:
                        if k % 3:
                            continue
                        for sx in (1, -1):
                            for sy in (1, -1):
                                out.append((0, 0, sx * dx, sy * dy, w))
                                out.append((0, 0, sx * dy, sy * dx, w))
    elif name == "fine":
        rng = np.random.default_rng(31)
        for _ in range(400):
            q = int(rng.integers(2, 17))
            p = int(rng.integers(1, q + 1))
            k = max(1, int(rng.integers(8, 41)) // q)
            w = int(rng.integers(2, 11))
            out.append((0, 0, q * k, p * k, w))
    else:
        raise KeyError(name)
    return out


def lines1(name):
    out, seen = [], set()
    from math import gcd
    if name in ("tie8", "tie8b"):   # exact hole ties in all 8 octants (tie8: f == 1 - s; tie8b: slope 2/3 at width 6, f == 0 too)
        sl = ((2, 3), (1, 2), (1, 3), (2, 5), (3, 4), (3, 5), (1, 4), (4, 5)) if name == "tie8" else ((2, 3),)
        ws = (3, 5, 10) if name == "tie8" else (6,)
        for (p_, q_) in sl:
            for k in (3, 5, 8):
                dx, dy = q_ * k, p_ * k
                for w in ws:
                    for sx in (1, -1):
                        for sy in (1, -1):
                            out.append((0, 0, sx * dx, sy * dy, w))
                            out.append((0, 0, sx * dy, sy * dx, w))
        return out
    qmax = 48 if name == "sweep" else 24
    for q in range(2, qmax + 1):
        for p_ in range(0, q + 1):
            if gcd(p_, q) != 1:
                continue
            k = max(1, (24 + q - 1) // q)
            if p_ * 1.0 / q in seen:
                continue
            seen.add(p_ * 1.0 / q)
            dx, dy = q * k, p_ * k
            if name == "sweep":
                for w in (3, 5, 10):
                    out.append((0, 0, dx, dy, w))
            else:
                for sx in (1, -1):
                    for sy in (1, -1):
                        out.append((0, 0, sx * dx, sy * dy, 5))
                        out.append((0, 0, sx * dy, sy * dx, 5))
    return out


def cmd_run1(a):
    """single copies white on black, packed into scenes, all ends inside 200 < x < 600, 150 < y < 450 being not required"""
    recs = []
    for (x1, y1, x2, y2, w) in lines1(a.set):
        recs.append(dict(x1=x1, y1=y1, x2=x2, y2=y2, colour=0x7FFF, width=w, alpha=10, flip=1, lw=float(w), sw=P.W, sh=P.H, fw=P.W))
    # place: rows of boxes (tie8: only where the window rule leaves integer ends exact, 200 < x < 600, 150 < y < 450)
    X0, X1, Y0, Y1 = (204, 596, 154, 446) if a.set in ("tie8", "tie8b") else (10, 790, 10, 590)
    cx, cy, rowh, out = X0, Y0, 0, []
    for r in recs:
        bw = abs(r["x2"] - r["x1"]) + 2 * (r["width"] + 8)
        bh = abs(r["y2"] - r["y1"]) + 2 * (r["width"] + 8)
        if cx + bw > X1:
            cx, cy, rowh = X0, cy + rowh, 0
        if cy + bh > Y1:
            cx, cy, rowh = X0, Y0, 0
        ox, oy = cx + r["width"] + 8 - min(0, r["x2"]), cy + r["width"] + 8 - min(0, r["y2"])
        r.update(x1=r["x1"] + ox, y1=r["y1"] + oy, x2=r["x2"] + ox, y2=r["y2"] + oy)
        ax, ay, bx, by, half = P.geometry(r)
        reach = half + 3.5
        r["box"] = (max(0, int(np.floor(min(ax, bx) - reach))), max(0, int(np.floor(min(ay, by) - reach))),
                    min(799, int(np.ceil(max(ax, bx) + reach))), min(599, int(np.ceil(max(ay, by) + reach))))
        x0, y0, x1, y1 = r["box"]
        r["pre"] = np.zeros((y1 - y0 + 1, x1 - x0 + 1), "<u2")
        r["post"] = r["pre"].copy()
        out.append(r)
        cx += bw
        rowh = max(rowh, bh)
    print(len(out), "records", flush=True)
    real = P.run_real(out, "s331_" + a.set)
    P.save(a.out, out, real)


def cmd_run(a):
    PH.lines_of = lines_of
    recs, groups = PH.build(a.set)
    print(len(groups), "lines,", len(recs), "records", flush=True)
    real = P.run_real(recs, "s331_" + a.set)
    P.save(a.out, recs, real)
    np.save(a.out.replace(".npz", "_groups.npy"), np.array(groups, dtype=object), allow_pickle=True)


def canon(g):
    """canonical frame: major and minor coordinates increasing from a to b, origin b; u major, v minor"""
    dx, dy = g["bx"] - g["ax"], g["by"] - g["ay"]
    xmaj = abs(dx) >= abs(dy)
    sM = np.sign(dx if xmaj else dy) or 1.0
    sm = np.sign(dy if xmaj else dx) or 1.0
    X, Y = g["X"] + 0.5, g["Y"] + 0.5
    Mc, mc = (X, Y) if xmaj else (Y, X)
    bM, bm = (g["bx"], g["by"]) if xmaj else (g["by"], g["bx"])
    aM, am = (g["ax"], g["ay"]) if xmaj else (g["ay"], g["ax"])
    s = abs((dy / dx) if xmaj else (dx / dy)) if (dx if xmaj else dy) else 0.0
    return (Mc - bM) * sM, (mc - bm) * sm, (Mc - aM) * sM, (mc - am) * sm, s, xmaj, sM, sm


def cmd_rows(a):
    recs = P.load(a.npz)
    groups = np.load(a.npz.replace(".npz", "_groups.npy"), allow_pickle=True)
    print("line\tx1\ty1\tx2\ty2\tw\ts\tL\tu\tv\tua\tva\tt_minus_L\tt\tn\tside\tcap\tc31_model\tlo\thi\tstatus")
    for li, g in enumerate(groups):
        iv = PH.intervals(recs, list(g))
        if iv is None:
            continue
        lo, hi = iv
        r0 = recs[g[0]]
        cov, geo = P.coverage(r0, a.rule)
        if geo is None:
            continue
        u, v, ua, va, s, xmaj, sM, sm = canon(geo)
        c31 = 31.0 * P.chan(r0["colour"])[0] * cov
        notdrawn = hi <= 1.0 - 15.5 / 16 + 1e-9
        tl = geo["t"] - geo["L"]
        side = np.clip(geo["half"] + 0.5 - np.abs(geo["n"]), 0, 1)
        cap = np.clip(geo["L"] - geo["t"] + 0.5, 0, 1)
        sel = (tl > -1.5) & ((cov > 0) | ~notdrawn)
        for yy, xx in zip(*np.nonzero(sel)):
            ok_model = (cov[yy, xx] > 0)
            if notdrawn[yy, xx]:
                st = "ND_ok" if c31[yy, xx] < hi[yy, xx] else "ND_bad"
            else:
                st = "V_ok" if lo[yy, xx] <= c31[yy, xx] < hi[yy, xx] else ("V_extra" if not ok_model else "V_bad")
            print(f"{li}\t{r0['x1']}\t{r0['y1']}\t{r0['x2']}\t{r0['y2']}\t{r0['width']}\t{s:.5f}\t{geo['L']:.4f}\t{u[yy, xx]:.4f}\t"
                  f"{v[yy, xx]:.4f}\t{ua[yy, xx]:.4f}\t{va[yy, xx]:.4f}\t{tl[yy, xx]:.4f}\t{geo['t'][yy, xx]:.4f}\t{geo['n'][yy, xx]:.4f}\t"
                  f"{side[yy, xx]:.4f}\t{cap[yy, xx]:.4f}\t{c31[yy, xx]:.4f}\t{lo[yy, xx]:.4f}\t{hi[yy, xx]:.4f}\t{st}")


def main():
    ap = argparse.ArgumentParser()
    sp = ap.add_subparsers(dest="cmd", required=True)
    p = sp.add_parser("run")
    p.add_argument("set")
    p.add_argument("out")
    p = sp.add_parser("run1")
    p.add_argument("set")
    p.add_argument("out")
    p = sp.add_parser("rows")
    p.add_argument("npz")
    p.add_argument("--rule", default="b8s1")
    a = ap.parse_args()
    {"run": cmd_run, "run1": cmd_run1, "rows": cmd_rows}[a.cmd](a)


if __name__ == "__main__":
    sys.exit(main())
