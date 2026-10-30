"""S3.16: which of the port's own renderSpriteQueue results (ordinals ord-2 .. ord+2 of the checkpoint's scene-corpus ord) the
framegate snapshot of a checkpoint equals, in a box (beyond-one-level px counts), and the same for the original's queue capture.

    cd port/build/S3_16 && SG_PORTSIM_DLL=... ~/bin/py -3.12 ../../../log/roadmap/S3/16_qcmp.py <scenario> <checkpoint> x0 y0 x1 y1
"""
import importlib.util
import json
import pathlib
import sys
import zlib

import numpy as np
from PIL import Image

ROOT = pathlib.Path.cwd()
sys.path.insert(0, str(ROOT))
sys.path.insert(0, str(ROOT / "re" / "tools"))
LOG = ROOT / ".." / ".." / ".." / "log" / "roadmap" / "S3"
spec = importlib.util.spec_from_file_location("sg5", LOG / "5_sprite_gate.py")
SG = importlib.util.module_from_spec(spec)
spec.loader.exec_module(SG)
from port.test import gfx as GX  # noqa: E402
import trace_format as TF  # noqa: E402
from port.test import boot as B  # noqa: E402


def c5(v):
    return np.stack([((v >> s) & 0x1f).astype(int) for s in (10, 5, 0)], axis=2)


def main():
    s, c = sys.argv[1:3]
    x0, y0, x1, y1 = map(int, sys.argv[3:7])
    d = SG.SCENE_DIR / s
    idx = json.loads((d / "index.json").read_text())
    o = idx["checkpoints"][c]["ord"]
    orig = c5(np.frombuffer(zlib.decompress((d / f"{c}.z").read_bytes()), "<u2").reshape(600, 800))
    ref = np.asarray(Image.open(GX.REFS / s / "det_b" / f"{c}.png").convert("RGB")).astype(int) >> 3
    snap = np.asarray(Image.open(LOG / "16_out" / "img5" / f"{s}.{c}.port.png").convert("RGB")).astype(int) >> 3
    tr = TF.read(B.TRACES / f"{s}.trace")
    frames = sum(1 for r in tr.records if r.type == TF.FRAME)
    ords = [o + k for k in range(-2, 3)]
    q, res = SG.boot_queue(s, ords, 0, frames + 2, True)
    f = lambda a, b: int((np.abs(a - b).max(2)[y0:y1, x0:x1] > 1).sum())
    print(s, c, "ord", o, "boot", res.status, "ref~origQ", f(ref, orig))
    for k in ords:
        if q.get(k) is None:
            print(" ", k, "none")
            continue
        p = c5(q[k])
        print(" ", k, "portQ~origQ", f(p, orig), "portQ~snapshot", f(p, snap), "portQ~ref", f(p, ref))


if __name__ == "__main__":
    main()
