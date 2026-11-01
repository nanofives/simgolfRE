"""S3.24: blend arithmetic variants for the drawn pixels (both the model's span rule and the real GL draw them): which one gives the
real GL's 5-bit values.

    ~/bin/py -3.12 log/roadmap/S3/24_arith.py <npz>...
"""
import importlib.util
import pathlib
import sys

import numpy as np

HERE = pathlib.Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("p24", HERE / "24_probe.py")
P = importlib.util.module_from_spec(spec)
spec.loader.exec_module(P)
f32 = np.float32


def variants():
    V = {}

    def base(s, al, dc, d):
        v = s * 255.0 * al + dc * 255.0 / 31.0 * (1.0 - al)
        return np.floor(v * 31.0 / 255.0 + d)
    V["s38"] = base

    def f32all(s, al, dc, d):
        s, al, dc, d = f32(s), al.astype(f32), dc.astype(f32), d.astype(f32)
        v = s * f32(255.0) * al + dc * (f32(255.0) / f32(31.0)) * (f32(1.0) - al)
        return np.floor(v * (f32(31.0) / f32(255.0)) + d)
    V["f32"] = f32all

    def f32b(s, al, dc, d):      # colours in [0, 1] floats, dst = dc / 31
        s, al, dc, d = f32(s), al.astype(f32), dc.astype(f32), d.astype(f32)
        v = s * al + (dc / f32(31.0)) * (f32(1.0) - al)
        return np.floor(v * f32(31.0) + d)
    V["f32_unit"] = f32b

    def f32c(s, al, dc, d):      # dst expanded to 8 bits as (dc << 3) | (dc >> 2)
        s, al, d = f32(s), al.astype(f32), d.astype(f32)
        dst = ((dc.astype(int) << 3) | (dc.astype(int) >> 2)).astype(f32)
        v = s * f32(255.0) * al + dst * (f32(1.0) - al)
        return np.floor(v * (f32(31.0) / f32(255.0)) + d)
    V["f32_dst8"] = f32c

    for bits in (8, 12, 16):
        def q(s, al, dc, d, bits=bits):
            alq = np.floor(al * (1 << bits)) / (1 << bits)
            return base(s, alq, dc, d)
        V[f"alpha_floor{bits}"] = q

        def qr(s, al, dc, d, bits=bits):
            alq = np.floor(al * (1 << bits) + 0.5) / (1 << bits)
            return base(s, alq, dc, d)
        V[f"alpha_round{bits}"] = qr
    return V


def main():
    V = variants()
    score = {k: [0, 0] for k in V}
    for p in sys.argv[1:]:
        for r in P.load(p):
            cov, g = P.coverage(r, "span_dir")
            if g is None:
                continue
            m = P.blend(r, cov)
            both = (m != r["pre"]) & (r["real"] != r["pre"]) & (cov > 0)
            if not both.any():
                continue
            x0, y0 = r["box"][:2]
            Y, X = np.mgrid[y0:y0 + cov.shape[0], x0:x0 + cov.shape[1]]
            d = (P.K[Y & 3, X & 3] + 0.5) / 16.0
            ca = min(max(float(f32(r["alpha"]) / f32(10.0)), 0.0), 1.0)
            al = (ca * cov)[both]
            dd = d[both]
            ss = P.chan(r["colour"])
            for k, fn in V.items():
                for sh, s in zip((10, 5, 0), ss):
                    dc = ((r["pre"][both].astype(int) >> sh) & 31).astype(float)
                    q = np.clip(fn(s, al, dc, dd), 0, 31).astype(int)
                    real = (r["real"][both].astype(int) >> sh) & 31
                    score[k][0] += int((q != real).sum())
                    score[k][1] += int(q.size)
    for k, (bad, n) in sorted(score.items(), key=lambda kv: kv[1][0]):
        print(f"{bad:6d} / {n}  {k}")


if __name__ == "__main__":
    main()
