"""S3.16: the port's back buffer at a list of clock ordinals (gfx.snapshotAt) in one framegate-style boot (gfx + LiveTerrain),
each compared in a box with the reference frame of a checkpoint (beyond-one-level px).

    cd port/build/S3_16 && SG_PORTSIM_DLL=... ~/bin/py -3.12 ../../../log/roadmap/S3/16_snapsweep.py <scenario> <checkpoint> x0 y0 x1 y1 <ord>...
"""
import pathlib
import sys

import numpy as np

ROOT = pathlib.Path.cwd()
sys.path.insert(0, str(ROOT))
sys.path.insert(0, str(ROOT / "re" / "tools"))
from port.test import boot as B  # noqa: E402
from port.test import framegate as FG  # noqa: E402
from port.test import gfx as GX  # noqa: E402
from port.test import lib as L  # noqa: E402
from port.test import terrainlive as TLm  # noqa: E402


def main():
    s, c = sys.argv[1:3]
    x0, y0, x1, y1 = map(int, sys.argv[3:7])
    ords = sorted(int(v) for v in sys.argv[7:])
    lib = L.load()
    if getattr(lib, "dll", None) is not None:
        FG._bind_live(lib.dll)
    va = FG.terrain_address(s)
    out = {}

    def before(st):
        st.call("gfx.attach", [])
        for o in ords:
            st.call("gfx.snapshotAt", [o])
        assert TLm.live_attach(lib, st, va) == 0

    def after(st, _res):
        for k, o in enumerate(ords):
            raw = st.files_read(f"$gfx/snapshot_{k}.555")
            out[o] = None if raw is None else np.frombuffer(raw, "<u2").reshape(600, 800).copy()
        st.call("gfx.detach", [])
        TLm.live_detach(lib, st)

    mr, te = GX.record_budget(s, ords[-1])
    res = B.run(s, lib, max_records=mr, before=before, after=after, to_end=te)
    ref = GX.reference(s, c).astype(int) >> 3
    for o in ords:
        if out.get(o) is None:
            print(o, "none")
            continue
        v = out[o]
        p = np.stack([((v >> sh) & 0x1f).astype(int) for sh in (10, 5, 0)], axis=2)
        d = np.abs(p - ref).max(2)
        print(o, "box beyond", int((d[y0:y1, x0:x1] > 1).sum()), "whole beyond", int((d > 1).sum()))
    print("boot", res.status)


if __name__ == "__main__":
    main()
