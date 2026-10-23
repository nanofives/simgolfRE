"""Fit experiments for the software rasteriser's arithmetic (phase 5 step 3): replays snapshots with `#tune` knobs
(portsim/sim/gl_raster.h, `tune[]`) and prints, per knob setting, the number of pixels that differ from the original's back
buffer and how many of those differ by more than one 5-bit level.

    ~/bin/py -3.12 -m port.test.glraster_fit "<setting>;<setting>..." step [step ...]
    setting = index=value[,index=value]      e.g.  "8=1;9=1,0=8"      ("-" = no knob, the baseline)

The knobs of the final pipeline are all 0. Experiments recorded in re/PHASE5_TERRAIN_PLAN.md section 10 (bilinear weight bits,
texture-coordinate bits, 8-bit colour intermediates, specular look-up tables, sub-pixel vertex snapping, float32 geometry and
lighting, sampling-position shifts, edge tie-break variants): none of them lowered the error of the exact double pipeline.
"""
from __future__ import annotations

import sys

import numpy as np

from port.test import glcorpus as G
from port.test import glraster as R
from port.test import lib as L


def errors(lib, setup, step, knobs):
    def patch(r, dll):
        txt = "".join(f"#tune {i} {v}\n" for i, v in knobs.items()).encode()
        if txt:
            dll.sg_raster_exec(r, txt, len(txt))
    post, got, st, unknown, pre = R.run(lib, setup, step, patch=patch)
    c = lambda a, sh: ((a.astype(int) >> sh) & 31)
    big = np.maximum.reduce([abs(c(post, sh) - c(got, sh)) for sh in (10, 5, 0)]) > 1
    return int((post != got).sum()), int(big.sum())


def parse(spec: str) -> dict[int, int]:
    if spec in ("", "-"):
        return {}
    return {int(a): int(b) for a, b in (kv.split("=") for kv in spec.split(","))}


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return
    lib = L.load()
    setup = R.Setup(G.CORPUS / "c7")
    steps = sys.argv[2:]
    print("baseline", {s: errors(lib, setup, s, {}) for s in steps})
    for spec in sys.argv[1].split(";"):
        print(spec, {s: errors(lib, setup, s, parse(spec)) for s in steps}, flush=True)


if __name__ == "__main__":
    main()
