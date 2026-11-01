"""S3.21: re-run GlRaster's experiment knobs (#tune lines) on c7 stream replays now that the vertex rule and clipping are in.

    cd <tree> && SG_PORTSIM_DLL=... ~/bin/py -3.12 ../../../log/roadmap/S3/21_tunes.py [config ...]   (config = "i=v,i=v"; none = default)
"""
import pathlib
import sys

import numpy as np

sys.path.insert(0, str(pathlib.Path.cwd()))
from port.test import glraster as R, lib as L  # noqa: E402

STEPS = ["sandbox", "zoom1", "z2_r1", "hole", "fuzz1_z0", "fuzz7_z1", "view3"]


def ch(v):
    return np.stack([((v >> s) & 0x1f).astype(np.int16) for s in (10, 5, 0)], axis=-1)


def run(lib, setup, cfg):
    tot1 = totb = 0
    per = []
    for s in STEPS:
        def patch(r, dll, cfg=cfg):
            for kv in cfg:
                line = f"#tune {kv[0]} {kv[1]}".encode()
                dll.sg_raster_exec(r, line, len(line))
        post, got, st, unk, pre = R.run(lib, setup, s, patch=patch if cfg else None)
        d = np.abs(ch(got) - ch(post)).max(-1)
        n1, nb = int((d == 1).sum()), int((d > 1).sum())
        tot1 += n1; totb += nb
        per.append(f"{s}:{n1}/{nb}")
    return tot1, totb, per


def main():
    lib = L.load()
    setup = R.Setup(pathlib.Path.cwd() / "port" / "corpus" / "gl" / "c7")
    cfgs = sys.argv[1:] or [""]
    for c in cfgs:
        cfg = [tuple(int(x) for x in kv.split("=")) for kv in c.split(",") if kv]
        t1, tb, per = run(lib, setup, cfg)
        print(f"{c or 'default':22s} one {t1:5d} beyond {tb:4d}  {' '.join(per)}", flush=True)


if __name__ == "__main__":
    main()
