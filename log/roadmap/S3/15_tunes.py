"""S3.15 (U-1392): count GlRaster's differences from glreplay's real software GL on glreplay-exact snapshots, for a list of
`#tune` settings (each given as "i=v,i=v"; "" = the default pipeline).

    cd port/build/S3_15 && SG_PORTSIM_DLL=... S31B_SCRATCH=... PYTHONPATH=. ~/bin/py -3.12 ../../../log/roadmap/S3/15_tunes.py \
        --snap c7/fuzz1_z0__tex --snap g2/i001_zoom1_pathupd "" "28=1" "28=2"
--corpus: take snapshots from the corpus (their dib_post is the original's, recorded) instead of the scratch root.
"""
import argparse
import os
import pathlib

import numpy as np

from port.test import glcorpus as G, glraster as R, lib as L

SCR = pathlib.Path(os.environ.get("S31B_SCRATCH", "."))


def rgb(a):
    return np.stack([(a >> 10) & 31, (a >> 5) & 31, a & 31], axis=1).astype(int)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--snap", action="append", required=True)
    ap.add_argument("--corpus", action="store_true")
    ap.add_argument("tunes", nargs="*")
    a = ap.parse_args()
    lib = L.load()
    R.bind(lib.dll)
    for spec in a.tunes or [""]:
        lines = "".join(f"#tune {kv.split('=')[0]} {kv.split('=')[1]}\n" for kv in spec.split(",") if kv).encode()

        def patch(r, dll, lines=lines):
            if lines:
                dll.sg_raster_exec(r, lines, len(lines))

        tot1 = totb = drawn = 0
        per = []
        for s in a.snap:
            label, step = s.split("/")
            root = (G.CORPUS if a.corpus else SCR) / label
            post, got, st = R.run(lib, R.Setup(root), step, patch=patch)[:3]
            dd = np.abs(rgb(post) - rgb(got)).max(axis=1)
            one, big = int((dd == 1).sum()), int((dd > 1).sum())
            tot1 += one
            totb += big
            drawn += st[2]
            per.append(f"{s}:{one}/{big}")
        print(f"[{spec or 'default'}] one {tot1} big {totb} drawn {drawn}  ({100 * tot1 / max(drawn, 1):.4f} %)  " + " ".join(per),
              flush=True)


if __name__ == "__main__":
    main()
