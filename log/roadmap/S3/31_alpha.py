"""S3.31: a pixel's real coverage to about 1e-7, by alpha bisection (U-1829, the line value arithmetic).

One line geometry (integer ends, so translations by multiples of 4 px keep the geometry relative to the pixels and the dither phase;
the float32 vertex rule leaves integer ends away from x 200 / 599 unchanged) is drawn white on black in K copies, copy k with
glColor alpha a_k. A pixel's 5-bit red value is floor(31 * f(a_k * c) + d) with f the blend's mapping and d its dither; a reference
pixel of the same dither phase with coverage exactly 1 (|n| <= half - 0.5, far from both caps) switches at a_ref, the pixel under
test at a_pix, so c = a_ref / a_pix wherever both switch from the same value q to q + 1 (same d, same q).

    ~/bin/py -3.12 log/roadmap/S3/31_alpha.py run <dx> <dy> <w> <out.npz> [--iters 4]     (glreplay)
    ~/bin/py -3.12 log/roadmap/S3/31_alpha.py show <out.npz>

Each iteration places --per copies at alphas spread over the bracket every tracked (pixel, q) still has, then narrows the brackets.
"""
import argparse
import importlib.util
import pathlib
import struct

import numpy as np

HERE = pathlib.Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("s331f", HERE / "31_f32.py")
F = importlib.util.module_from_spec(spec)
spec.loader.exec_module(F)
P = F.P
f32 = np.float32


def fbits(x):
    return "0x%x" % struct.unpack("<I", struct.pack("<f", float(x)))[0]


PRE = 0


def scene(dx, dy, w, alphas, name):
    """copies of the line at alphas; returns (recs, real boxes); alpha is passed as float bits through a record field"""
    recs = []
    span_x, span_y = abs(dx) + 2 * (w + 6), abs(dy) + 2 * (w + 6)
    cw, ch = 4 * ((span_x + 3) // 4 + 1), 4 * ((span_y + 3) // 4 + 1)
    # the ends stay where the window rule leaves integers unchanged: 200 < x < 600, 150 < y < 450 (|x - c| < h / 2)
    cols = max(1, (596 - 204) // cw)
    for k, a in enumerate(alphas):
        cx, cy = 204 + (k % cols) * cw, 156 + (k // cols) * ch
        ox, oy = cx + w + 6 - min(0, dx), cy + w + 6 - min(0, dy)
        r = dict(x1=ox, y1=oy, x2=ox + dx, y2=oy + dy, colour=0x7FFF, width=w, alpha=10, flip=1, lw=float(w), sw=P.W, sh=P.H, fw=P.W,
                 alpha_f=float(f32(a)))
        g = P.geometry(r)
        ax, ay, bx, by, half = g
        reach = half + 3.5
        r["box"] = (int(np.floor(min(ax, bx) - reach)), int(np.floor(min(ay, by) - reach)),
                    int(np.ceil(max(ax, bx) + reach)), int(np.ceil(max(ay, by) + reach)))
        x0, y0, x1, y1 = r["box"]
        if not (200 < min(r["x1"], r["x2"]) and max(r["x1"], r["x2"]) < 600 and 150 < min(r["y1"], r["y2"]) and max(r["y1"], r["y2"]) < 450):
            raise RuntimeError("copy %d outside the region where the ends are unchanged" % k)
        r["pre"] = np.full((y1 - y0 + 1, x1 - x0 + 1), PRE, "<u2")
        r["post"] = r["pre"].copy()
        recs.append(r)
    calls_orig = P.calls

    def calls(r):
        out = calls_orig(r)
        return [("glColor4f " + ",".join(fbits(v) for v in P.chan(r["colour"]) + [r["alpha_f"]])) if c.startswith("glColor4f") else c
                for c in out]
    P.calls = calls
    try:
        real = P.run_real(recs, name)
    finally:
        P.calls = calls_orig
    return recs, real


def red(a):
    return (a.astype(int) >> 10) & 31


def cmd_run(a):
    dx, dy, w = a.dx, a.dy, a.w
    lo, hi = 0.02, 1.0
    alphas = np.linspace(lo, hi, a.per)
    hist = []          # (alpha, red box) per copy
    for it in range(a.iters):
        recs, real = scene(dx, dy, w, alphas, f"s331_alpha_{it}")
        for r, rl in zip(recs, real):
            hist.append((r["alpha_f"], red(rl)))
        # brackets: for every pixel and value q, the largest alpha with value <= q and the smallest with value > q
        al = np.array([h[0] for h in hist])
        order = np.argsort(al)
        R = np.stack([hist[i][1] for i in order])
        al = al[order]
        nxt = []
        for k in range(len(al) - 1):
            if (R[k] != R[k + 1]).any():
                nxt += list(np.linspace(al[k], al[k + 1], 6)[1:-1])
        rng = np.random.default_rng(it)
        if len(nxt) > a.per:
            nxt = list(rng.choice(nxt, a.per, replace=False))
        alphas = np.array(sorted(set(float(f32(x)) for x in nxt)))
        if len(alphas) == 0:
            break
        print(f"iteration {it}: {len(hist)} copies, next {len(alphas)}", flush=True)
    al = np.array([h[0] for h in hist])
    R = np.stack([h[1] for h in hist])
    r0 = recs[0]
    np.savez_compressed(a.out, alpha=al, red=R, meta=np.array([dx, dy, w]), box=np.array(r0["box"]), x1=r0["x1"], y1=r0["y1"])


def thresholds(al, R):
    """per pixel: dict q -> (largest alpha with value <= q, smallest alpha with value > q)"""
    order = np.argsort(al)
    al, R = al[order], R[order]
    H, W = R.shape[1:]
    out = {}
    for y in range(H):
        for x in range(W):
            v = R[:, y, x]
            if v.max() == 0:
                continue
            d = {}
            for q in range(0, 31):
                below = al[v <= q]
                above = al[v > q]
                if len(below) and len(above) and below.max() < above.min():
                    d[q] = (below.max(), above.min())
            out[(x, y)] = d
    return out


def cmd_show(a):
    z = np.load(a.npz)
    dx, dy, w = (int(v) for v in z["meta"])
    th = thresholds(z["alpha"], z["red"])
    x0, y0 = int(z["box"][0]), int(z["box"][1])
    r = dict(x1=int(z["x1"]), y1=int(z["y1"]), x2=int(z["x1"]) + dx, y2=int(z["y1"]) + dy, colour=0x7FFF, width=w, alpha=10, flip=1,
             lw=float(w), sw=P.W, sh=P.H, fw=P.W, box=tuple(int(v) for v in z["box"]))
    cov, g = P.coverage(r, "span_dir3")
    # reference pixels: model coverage exactly 1, per dither phase
    refs = {}
    for (x, y), d in th.items():
        if cov[y, x] == 1.0 and abs(g["t"][y, x]) > 3 and abs(g["L"] - g["t"][y, x]) > 3:
            ph = ((y + y0) & 3, (x + x0) & 3)
            for q, br in d.items():
                refs.setdefault((ph, q), []).append(br)
    print("line", (dx, dy), "w", w, "pixels with thresholds", len(th), "reference phases", len({k[0] for k in refs}))
    rows = []
    for (x, y), d in th.items():
        if cov[y, x] >= 1.0 or cov[y, x] <= 0:
            continue
        ph = ((y + y0) & 3, (x + x0) & 3)
        est = []
        for q, (blo, bhi) in d.items():
            if (ph, q) not in refs:
                continue
            rlo = max(b[0] for b in refs[(ph, q)])
            rhi = min(b[1] for b in refs[(ph, q)])
            # c in (rlo / bhi, rhi / blo)
            est.append((rlo / bhi, rhi / blo))
        if not est:
            continue
        clo = max(e[0] for e in est)
        chi = min(e[1] for e in est)
        rows.append((x + x0, y + y0, cov[y, x], clo, chi, g["n"][y, x], g["t"][y, x] - g["L"], g["t"][y, x]))
    for x, y, c, clo, chi, n, tl, t in sorted(rows, key=lambda r_: -abs(r_[2] - (r_[3] + r_[4]) / 2)):
        flag = "" if clo <= c <= chi else "  <-- model outside"
        print(f"({x},{y}) model {c:.7f} real [{clo:.7f},{chi:.7f}] n {n:+.5f} t-L {tl:+.3f} t {t:.3f}{flag}")


def main():
    ap = argparse.ArgumentParser()
    sp = ap.add_subparsers(dest="cmd", required=True)
    p = sp.add_parser("run")
    p.add_argument("dx", type=int)
    p.add_argument("dy", type=int)
    p.add_argument("w", type=int)
    p.add_argument("out")
    p.add_argument("--iters", type=int, default=4)
    p.add_argument("--per", type=int, default=40, help="copies per scene")
    p = sp.add_parser("show")
    p.add_argument("npz")
    a = ap.parse_args()
    {"run": cmd_run, "show": cmd_show}[a.cmd](a)


if __name__ == "__main__":
    main()


def focus(dx, dy, w, rel, iters=8, per=40, level=30):
    """bisect the alpha at which pixel `rel` (relative to the first end) and a coverage-1 reference pixel of the same dither phase
    reach red value `level` (white on black, PRE = 0: the value is floor(31 * f * a * c + d), linear in a * c, so the coverage is
    c = a_ref / a_pix); returns (a_pix, a_ref) brackets and the reference position. Choose `level` so that both switch below
    alpha 1 (U-1980: the first version bisected 30 -> 31 over red 10, which a pixel under coverage 0.994 never reaches)."""
    L = (dx * dx + dy * dy) ** 0.5
    ux, uy = dx / L, dy / L
    ref = None
    for x in range(min(0, dx), max(0, dx) + 1):
        for y in range(min(0, dy) - 3, max(0, dy) + 4):
            px, py = x + 0.5, y + 0.5
            n = -px * uy + py * ux
            t = px * ux + py * uy
            if (x - rel[0]) % 4 == 0 and (y - rel[1]) % 4 == 0 and abs(n) < w / 2 - 0.6 and 3 < t < L - 3:
                ref = (x, y)
                break
        if ref:
            break
    assert ref, "no reference pixel"
    floor = max(0.01, 0.9 * (level - 1.0) / 31.0)   # below every switch to `level` (U-1980: a fixed 0.30 cut low levels off)
    br = {"pix": [floor, 1.0], "ref": [floor, 1.0]}
    for it in range(iters):
        alphas = []
        for key in ("pix", "ref"):
            lo, hi = br[key]
            alphas += list(np.linspace(lo, hi, per // 2))
        recs, real = scene(dx, dy, w, alphas, f"s331_focus_{it}")
        obs = {"pix": [], "ref": []}
        for r, rl in zip(recs, real):
            x0, y0 = r["box"][:2]
            for key, (rx, ry) in (("pix", rel), ("ref", ref)):
                q = int(red(rl[r["y1"] + ry - y0, r["x1"] + rx - x0]))
                obs[key].append((r["alpha_f"], q))
        for key in ("pix", "ref"):
            below = [a_ for a_, q in obs[key] if q < level]
            above = [a_ for a_, q in obs[key] if q >= level]
            if not above:
                raise RuntimeError("%s never reaches level %d below alpha 1" % (key, level))
            br[key] = [max([br[key][0]] + below), min([br[key][1]] + above)]
        print(it, {k: ("%.9f" % v[0], "%.9f" % v[1]) for k, v in br.items()}, flush=True)
    return br, ref
