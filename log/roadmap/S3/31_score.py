"""S3.31: score candidate overlay-line models against the real GL on S3.24's record sets and S3.31's probe sets.

For each set: differing px (any channel) of the model against the real GL, one-level and beyond, and against a base model the
pixels the candidate gets newly wrong. Models are (vertex, rule):
    vertex snap = S3.24's double snap of the ends, f32 = S3.21's float32 vertex rule (31_f32.py win_f32), f32c = f32 with
    the segment clipped in clip space in float32 (31_f32.py geometry_f32_clip)
    rule = a 24_probe.py RULES key (b8s1 = the port's line rule 2) or a 31_cap.py cap rule (cap:<name>)

    ~/bin/py -3.12 log/roadmap/S3/31_score.py <npz>... --model snap:b8s1 --model f32:b8s1 [--base snap:b8s1]
"""
import argparse
import importlib.util
import pathlib

import numpy as np

HERE = pathlib.Path(__file__).resolve().parent


def _load(name, file):
    spec = importlib.util.spec_from_file_location(name, HERE / file)
    m = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(m)
    return m


F = _load("s331f", "31_f32.py")
P = F.P
EXTRA_RULES = {}


def model(r, spec):
    vert, rule = spec.split(":", 1)
    P.geometry = F.geometry_f32 if vert in ("f32", "f32c") else F.P_geometry
    F.CLIP_MODE = "clip" if vert == "f32c" else "window"
    try:
        if rule in EXTRA_RULES:
            cov, g = P.coverage(r, "b8s1")
            if g is not None:
                cov = EXTRA_RULES[rule](cov, g)
        else:
            cov, g = P.coverage(r, rule)
        return P.blend(r, cov)
    finally:
        P.geometry = F.P_geometry


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("npz", nargs="+")
    ap.add_argument("--model", action="append", required=True)
    ap.add_argument("--base", default="snap:b8s1")
    a = ap.parse_args()
    for p in a.npz:
        recs = P.load(p)
        tot = {m: [0, 0, 0, 0] for m in a.model}
        for r in recs:
            base = model(r, a.base)
            bw = P.diff5(base, r["real"]) > 0
            for m in a.model:
                d = P.diff5(model(r, m), r["real"])
                tot[m][0] += int((d > 0).sum())
                tot[m][1] += int((d > 1).sum())
                tot[m][2] += int(((d > 0) & ~bw).sum())
                tot[m][3] += int(((d == 0) & bw).sum())
        for m, (px, beyond, new, fixed) in tot.items():
            print(f"{pathlib.Path(p).name:16s} {m:22s} px {px:5d} beyond {beyond:4d} newly_wrong {new:4d} fixed {fixed:4d}  (base {a.base})")


if __name__ == "__main__":
    main()
