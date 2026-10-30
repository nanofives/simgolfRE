"""S3.16: where the port's terrain (the root at the last updateTerrainRender's exit before a checkpoint) differs from the original's
recorded one (S3.2 --pass-frames recordings, log/p5/terrain/<label>/frames). Prints the x histogram (bins of 8) of the differing
pixels and their one-level / beyond counts in the bottom_panel band; writes <out>.png (port | orig | diff, the bottom-right quarter).

    cd port/build/S3_16 && SG_PORTSIM_DLL=... ~/bin/py -3.12 ../../../log/roadmap/S3/16_terrain_cmp.py <label> <scenario> <checkpoint> [out]
"""
import collections
import pathlib
import sys

import numpy as np

ROOT = pathlib.Path.cwd()
sys.path.insert(0, str(ROOT))
from port.test import lib as L  # noqa: E402
from port.test import terrainlive as TL  # noqa: E402


def ch(v):
    return np.stack([((v >> s) & 0x1f).astype(np.int16) for s in (10, 5, 0)], axis=2)


def main():
    label, scen, cp = sys.argv[1:4]
    out = sys.argv[4] if len(sys.argv) > 4 else None
    rec = TL.recorded_frames(label)[cp]
    k = rec["meta"]["frame"]
    lib = L.load()
    pf = TL.port_frames(lib, scen, [k], cp)
    for what in ("exit",):
        o = rec[what]
        p = pf[(k, what)]
        d = np.abs(ch(p) - ch(o)).max(2)
        ys, xs = np.nonzero(d)
        print(label, cp, "frame", k, what, "differ", len(xs), "beyond", int((d > 1).sum()))
        print(" x", sorted(collections.Counter(int(x) // 8 * 8 for x in xs).items()))
        bp = d[0x1F0:0x1F0 + 0x68, 0xD0:0xD0 + 0x250]
        print(" bottom_panel one", int((bp == 1).sum()), "beyond", int((bp > 1).sum()))
        if out:
            from PIL import Image
            rgb = lambda v: (ch(v)[..., ::1].astype(np.uint8) << 3)
            m = np.zeros((600, 800, 3), np.uint8)
            m[d > 0] = (255, 0, 255)
            a = np.concatenate([rgb(p)[300:, 400:], rgb(o)[300:, 400:], m[300:, 400:]], axis=1)
            Image.fromarray(a).save(out)


if __name__ == "__main__":
    main()
