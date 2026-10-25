"""S3.1 (U-1300): where GlRaster misses the G2 criterion on the g6 fuzz passes, replaying the ORIGINAL's recorded stream (not the
port's): one-level and bigger errors split by blended / opaque pixels (sg_raster_get_source's blended flag).

    cd port/build/S3_head && SG_PORTSIM_DLL=<dll> PYTHONPATH=. ~/bin/py -3.12 log/roadmap/S3/1_g6_diag.py g6/i012_raised_strip g5/i012_raised_strip
"""
import sys

import numpy as np

from port.test import glcorpus as G, glraster as R, lib as L

lib = L.load()
R.bind(lib.dll)
for label, step in [a.split("/") for a in sys.argv[1:]]:
    st = R.Setup(G.CORPUS / label)
    post, got, stt, unk, pre, fl, src = R.run(lib, st, step, want_float=True)
    rgb = lambda a: np.stack([(a >> 10) & 31, (a >> 5) & 31, a & 31], axis=1).astype(int)
    dd = np.abs(rgb(post) - rgb(got)).max(axis=1)
    blended = src.reshape(-1, 8)[:, 7] > 0
    for name, m in (("blended", blended), ("opaque", ~blended)):
        print(f"{label}/{step} {name}: px {int(m.sum())} one {int(((dd == 1) & m).sum())} big {int(((dd > 1) & m).sum())}")
