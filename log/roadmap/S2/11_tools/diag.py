"""S2.11: where the judged pixels of a checkpoint's rectangles fail.

usage (mirror root): SG_PORTSIM_DLL=<dll> ~/bin/py -3.12 <abs>/log/roadmap/S2/11_tools/diag.py <scenario> <checkpoint> <out dir>
Renders the run without a probe and the three probe runs (port/test/test_gfx_dialogs2.py), writes <out>/<checkpoint>_fail.png
(the port frame, failing judged pixels red, held pixels blue-tinted) and the reference, and prints the failing pixels' boxes
per rectangle with a few port / reference values.
"""
import pathlib
import sys

import numpy as np
from PIL import Image

sys.path.insert(0, str(pathlib.Path.cwd()))
from port.test import gfx as GX  # noqa: E402
from port.test import lib as L  # noqa: E402
from port.test import test_gfx_dialogs2 as T  # noqa: E402


def main(argv):
    scen, chk, out = argv[0], argv[1], pathlib.Path(argv[2])
    out.mkdir(parents=True, exist_ok=True)
    lib = L.load()
    if not lib.exe_loaded():
        lib.exe_load()
    normal = GX.render(scen, [chk], lib)
    fam = T.PROBE_FAMILY.get(scen, T.DEFAULT_PROBE)
    probes = [GX.render(scen, [chk], lib, negctl=m, negctl_name=fam) for m in T.PROBES] if fam else []
    img, ref = normal.frames[chk], GX.reference(scen, chk)
    opaque = np.ones(img.shape[:2], bool)
    for p in probes:
        opaque &= (p.frames[chk] == img).all(2)
    differ = (img != ref).any(2)
    vis = img.copy()
    vis[~opaque] = (vis[~opaque] * 0.4 + np.array([0, 0, 150])).astype(np.uint8)
    for (x0, y0, x1, y1) in T.CASES[scen][chk]:
        m = np.zeros_like(opaque)
        m[y0:y1 + 1, x0:x1 + 1] = True
        f = m & opaque & differ
        vis[f] = (255, 0, 0)
        ys, xs = np.where(f)
        print(f"rect {(x0, y0, x1, y1)}: fail {len(xs)}", end="")
        if len(xs):
            print(f" box ({xs.min()}, {ys.min()}) .. ({xs.max()}, {ys.max()})")
            for y, x in list(zip(ys, xs))[:12]:
                print(f"   ({x}, {y}) port {tuple(int(v) for v in img[y, x])} ref {tuple(int(v) for v in ref[y, x])}")
        else:
            print()
    Image.fromarray(vis).save(out / f"{scen}_{chk}_fail.png")
    Image.fromarray(ref).save(out / f"{scen}_{chk}_ref.png")


if __name__ == "__main__":
    main(sys.argv[1:])
