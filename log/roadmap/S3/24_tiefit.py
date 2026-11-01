"""S3.24: tie models scored on labelled ties: the census harvest (24_data/ties_census.pkl, 24_harvest.py) and probe sets (npz).

A case: snapped ends (ax, ay, bx, by), pixel centre (X, Y), N, xmaj, drawn. Models decide drawn for a pixel exactly at +-N/2.
Convention models use signs only; DDA models step the minor centre from a chosen origin vertex with the slope rounded to F bits.

    ~/bin/py -3.12 log/roadmap/S3/24_tiefit.py [--npz a.npz ...] [--top 30]
"""
import argparse
import importlib.util
import pathlib
import pickle

import numpy as np

HERE = pathlib.Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("p24", HERE / "24_probe.py")
P = importlib.util.module_from_spec(spec)
spec.loader.exec_module(P)


def case_from(r, X, Y, N, xmaj, drawn, src):
    ax, ay, bx, by, half = P.geometry(r)
    aM, am, bM, bm = (ax, ay, bx, by) if xmaj else (ay, ax, by, bx)
    M, m = (X, Y) if xmaj else (Y, X)
    o = m - (am + (M - aM) * (bm - am) / (bM - aM))
    return dict(aM=aM, am=am, bM=bM, bm=bm, M=M, m=m, N=N, xmaj=xmaj, o=o, drawn=drawn, src=src, w=r["width"])


def load_cases(npzs):
    cs = []
    pk = HERE / "24_data" / "ties_census.pkl"
    if pk.exists():
        for scen, c, r, X, Y, N, xmaj, drawn in pickle.load(open(pk, "rb")):
            cs.append(case_from(r, X, Y, N, xmaj, drawn, "census"))
    for p in npzs:
        for r in P.load(p):
            cov, g = P.coverage(r, "s38")
            if g is None:
                continue
            dx, dy = g["bx"] - g["ax"], g["by"] - g["ay"]
            major = max(abs(dx), abs(dy))
            if major == 0:
                continue
            N = float(np.floor(2 * g["half"] * g["L"] / major) + 2)
            o, xmaj = P.minor_offset(g)
            m = P.blend(r, cov)
            sel = (np.abs(np.abs(o) - N / 2) < 1e-9) & (cov > 0) & (m != r["pre"])
            rd = r["real"] != r["pre"]
            for yy, xx in zip(*np.nonzero(sel)):
                cs.append(case_from(r, float(g["X"][yy, xx] + 0.5), float(g["Y"][yy, xx] + 0.5), N, bool(xmaj), bool(rd[yy, xx]),
                                    pathlib.Path(p).stem))
    return cs


def sgn(v):
    return 1.0 if v >= 0 else -1.0


def models():
    out = {}
    # conventions: drawn iff o * sign < 0 (the low side of s = o * sign is inclusive)
    conv = {
        "minor": lambda c: sgn(c["bm"] - c["am"]),
        "major": lambda c: sgn(c["bM"] - c["aM"]),
        "slope": lambda c: sgn(c["bm"] - c["am"]) * sgn(c["bM"] - c["aM"]),
        "plus": lambda c: 1.0,
        "minus": lambda c: -1.0,
    }
    for k, f in conv.items():
        out[f"conv_{k}"] = (lambda c, f=f: c["o"] * f(c) < 0)
        out[f"conv_{k}_xy"] = (lambda c, f=f: (c["o"] * f(c) < 0) if c["xmaj"] else (c["o"] * f(c) > 0))

    # DDA: origin = a or the lower-major end; minor centre stepped per major unit with the slope rounded to F bits (trunc toward
    # zero, floor, nearest, away); the first centre computed exactly at the first pixel centre column; window [lo, hi) in the
    # DDA's minor step direction (or in +minor)
    for origin in ("a", "lowM", "b"):
        for F in (10, 12, 14, 16, 20, 24, 28, 31):
            for rnd in ("trunc", "floor", "near", "away", "ceil"):
                for side in ("step", "plus"):
                    def fn(c, origin=origin, F=F, rnd=rnd, side=side):
                        if origin == "a" or (origin == "lowM" and c["aM"] <= c["bM"]):
                            M0, m0, M1, m1 = c["aM"], c["am"], c["bM"], c["bm"]
                        else:
                            M0, m0, M1, m1 = c["bM"], c["bm"], c["aM"], c["am"]
                        step_M = sgn(M1 - M0)
                        slope = (m1 - m0) / abs(M1 - M0)          # minor change per major step in the DDA direction
                        v = slope * (1 << F)
                        q = {"trunc": np.trunc(v), "floor": np.floor(v), "near": np.floor(v + 0.5),
                             "away": np.sign(v) * np.ceil(abs(v)), "ceil": np.ceil(v)}[rnd]
                        sl = q / (1 << F)
                        # first pixel-centre column at or after M0 in the DDA direction
                        k0 = (np.floor(M0) + 0.5) if step_M > 0 else (np.ceil(M0) - 0.5)
                        if (k0 - M0) * step_M < 0:
                            k0 += step_M
                        c0 = m0 + abs(k0 - M0) * slope                 # exact at the first column
                        steps = abs(c["M"] - k0)
                        mc = c0 + steps * sl
                        o2 = c["m"] - mc
                        s_ = o2 * (sgn(slope) if side == "step" else 1.0)
                        h = c["N"] / 2
                        if abs(abs(o2) - h) < 1e-12:
                            return s_ < 0
                        return -h <= s_ < h
                    out[f"dda_{origin}_F{F}_{rnd}_{side}"] = fn
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--npz", nargs="*", default=[])
    ap.add_argument("--top", type=int, default=30)
    a = ap.parse_args()
    cs = load_cases(a.npz)
    srcs = sorted({c["src"] for c in cs})
    print(len(cs), "ties;", {s: (sum(1 for c in cs if c["src"] == s), sum(c["drawn"] for c in cs if c["src"] == s)) for s in srcs})
    res = []
    for name, fn in models().items():
        hit = {s: 0 for s in srcs}
        for c in cs:
            hit[c["src"]] += fn(c) == c["drawn"]
        res.append((sum(hit.values()), name, hit))
    for tot, name, hit in sorted(res, key=lambda r: -r[0])[:a.top]:
        print(f"{tot:5d} / {len(cs)}  {name}  {hit}")


if __name__ == "__main__":
    main()
