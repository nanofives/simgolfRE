"""S5.3: one corpus job drawn by the FreeType provider next to the GDI reference raster (port/corpus/labels/rasters/
gdi_x64.npz). Prints the differing pixel count, the ink boxes, and both surfaces' 5-bit green channel as digits over the
ink box. Usage: SG_PORTSIM_DLL=... 3_job_cmp.py [job index ...] [--font 0x0051b360 --n 3] [--negctl N]"""
import argparse
import pathlib
import sys

import numpy as np

ROOT = pathlib.Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT))
from port.test import labels as LB  # noqa: E402


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("jobs", nargs="*", type=int)
    ap.add_argument("--font")
    ap.add_argument("--n", type=int, default=2)
    ap.add_argument("--negctl", type=int, default=0)
    ap.add_argument("--quiet", action="store_true")
    a = ap.parse_args()
    corpus = LB.load_corpus()
    ents = corpus["entries"]
    jobs = LB.corpus_jobs(corpus)
    idx = list(a.jobs)
    if a.font:
        fr = int(a.font, 16)
        idx += [i for i, j in enumerate(jobs) if j[0] == fr][:a.n]
    ref = LB.load_rasters(LB.OUT / "rasters" / "gdi_x64.npz")
    lib = LB.load_lib("x64")
    prev = lib.negctl("fttext.fontWidth", a.negctl) if a.negctl else None
    lab = LB.Lab(lib, "freetype", LB.start_image(corpus["image"]))
    try:
        got = lab.render([jobs[i] for i in idx])
    finally:
        lab.close()
        if a.negctl:
            lib.negctl("fttext.fontWidth", prev or 0)
    for i, g in zip(idx, got):
        r = ref[i]
        f, c, bg, x, y, w, h, t = jobs[i][:8]
        d = g != r
        print(f"job {i} cfg {corpus['jobs'][i][0]} font 0x{f:08x} text {t!r} colour 0x{c:08x} bg 0x{bg:04x} origin ({x},{y})"
              f" {w}x{h}: {int(d.sum())} px differ")
        if a.quiet:
            continue
        ink = (g != bg) | (r != bg)
        ys, xs = np.nonzero(ink)
        if not len(ys):
            continue
        y0, y1, x0, x1 = ys.min(), ys.max() + 1, xs.min(), min(xs.max() + 1, xs.min() + 60)
        for yy in range(y0, y1):
            gg = "".join("%x" % (((v >> 5) & 31) >> 1) for v in r[yy, x0:x1])
            ff = "".join("%x" % (((v >> 5) & 31) >> 1) for v in g[yy, x0:x1])
            print(f"  {yy - y:3d} {gg}   {ff}")


if __name__ == "__main__":
    main()
