"""S3.24: candidate arithmetic models for the span rule's exact ties, scored against the real GL.

A tie is a pixel the model would draw visibly whose minor offset o is exactly +-N/2 in exact arithmetic (snapped ends are
multiples of 1/256). Each model predicts drawn / not drawn; printed: hits per model.

    ~/bin/py -3.12 log/roadmap/S3/24_tiemodel.py <npz>...
"""
import importlib.util
import pathlib
import sys

import numpy as np

HERE = pathlib.Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("p24", HERE / "24_probe.py")
P = importlib.util.module_from_spec(spec)
spec.loader.exec_module(P)


def f32(v):
    return float(np.float32(v))


def models():
    """name -> fn(case) -> bool drawn. case: dict(ax, ay, bx, by, xc (major centre), yc_pix (minor centre), N, xmaj, side)"""
    out = {}

    def conv(name, sgfn):
        out[name] = lambda c, sgfn=sgfn: (c["o"] * sgfn(c)) < 0      # at an exact tie: low-inclusive in s = o * sign
    conv("y_low", lambda c: 1.0)
    conv("y_high", lambda c: -1.0)
    conv("minor_dir", lambda c: c["sm"])                              # sign of the minor delta (a -> b)
    conv("slope_sign", lambda c: c["sM"] * c["sm"])                   # sign of minor / major
    conv("major_dir", lambda c: c["sM"])

    # DDA from an origin vertex with a slope rounded to F fraction bits (trunc toward 0 / floor / nearest); the window is low-inclusive
    # in the DDA's own minor direction (the sign of its per-step minor change); the tie is decided by the rounding drift
    for origin in ("a", "low"):
        for F in (8, 12, 16, 20, 24, 31):
            for rnd in ("trunc", "floor", "near"):
                for conv_dir in ("step", "y"):
                    def fn(c, origin=origin, F=F, rnd=rnd, conv_dir=conv_dir):
                        if origin == "a" or c["sM"] > 0:
                            M0, m0, sM = c["aM"], c["am"], c["sM"]
                        else:
                            M0, m0, sM = c["bM"], c["bm"], -c["sM"]
                        slope = (c["bm"] - c["am"]) / (c["bM"] - c["aM"]) * sM * c["sM"]   # minor change per +1 step in sM
                        slope = (c["bm"] - c["am"]) / abs(c["bM"] - c["aM"]) * (1 if (origin == "a" or c["sM"] > 0) else -1)
                        sc = 2.0 ** F
                        v = slope * sc
                        q = np.trunc(v) if rnd == "trunc" else (np.floor(v) if rnd == "floor" else np.floor(v + 0.5))
                        sl = q / sc
                        k = (c["xc"] - M0) * sM
                        ycen = m0 + k * sl
                        o2 = c["ypix"] - ycen
                        step = np.sign(sl) or 1.0
                        sg = step if conv_dir == "step" else 1.0
                        s = o2 * sg
                        n2 = c["N"] / 2
                        return bool(-n2 <= s < n2) if abs(abs(o2) - n2) > 1e-12 else (s < 0)
                    out[f"dda_{origin}_F{F}_{rnd}_{conv_dir}"] = fn
    return out


def cases(paths):
    out = []
    for p in paths:
        for r in P.load(p):
            cov, g = P.coverage(r, "s38")
            if g is None:
                continue
            dx, dy = g["bx"] - g["ax"], g["by"] - g["ay"]
            major = max(abs(dx), abs(dy))
            nn = np.floor(2 * g["half"] * g["L"] / major) + 2
            o, xmaj = P.minor_offset(g)
            tie = (np.abs(np.abs(o) - nn / 2) < 1e-9) & (cov > 0)
            if not tie.any():
                continue
            m = P.blend(r, cov)
            vis = m != r["pre"]
            rd = r["real"] != r["pre"]
            for yy, xx in zip(*np.nonzero(tie & vis)):
                X, Y = g["X"][yy, xx] + 0.5, g["Y"][yy, xx] + 0.5
                aM, am, bM, bm = (g["ax"], g["ay"], g["bx"], g["by"]) if xmaj else (g["ay"], g["ax"], g["by"], g["bx"])
                out.append(dict(o=float(o[yy, xx]), N=float(nn), xmaj=xmaj, sM=float(np.sign(bM - aM) or 1), sm=float(np.sign(bm - am) or 1),
                                aM=aM, am=am, bM=bM, bm=bm, xc=X if xmaj else Y, ypix=Y if xmaj else X, drawn=bool(rd[yy, xx]),
                                w=r["width"]))
    return out


def main():
    cs = cases(sys.argv[1:])
    print(len(cs), "tie pixels;", sum(c["drawn"] for c in cs), "drawn")
    res = []
    for name, fn in models().items():
        hit = sum(fn(c) == c["drawn"] for c in cs)
        res.append((hit, name))
    for hit, name in sorted(res, reverse=True)[:25]:
        print(f"{hit:5d} / {len(cs)}  {name}")


if __name__ == "__main__":
    main()
