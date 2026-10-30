"""S3.16: split the differing pixels of a framegate checkpoint by the layer that gave them their final value.

Inputs: the plain framegate image (<dir>/<s>.<c>.port.png) and the image of a framegate run with a family's TEST-ONLY mask mode
(<maskdir>/<s>.<c>.port.png, e.g. --negctl gfx.family.scene=7: every pixel a scene body writes is stored inverted). A pixel that
differs between the two got its final value from that family. Prints, per framegate region: differing px inside the mask
(one-level / beyond) and outside it (one-level / beyond), plus the clusters of the outside-the-mask beyond pixels.

    ~/bin/py -3.12 log/roadmap/S3/16_classify.py <dir> <maskdir> <scenario> <checkpoint>
"""
import pathlib
import sys

import numpy as np
from PIL import Image

ROOT = pathlib.Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "log" / "roadmap" / "S3"))
REGIONS = [("hud_title", (0, 0, 0xE8, 0x52)), ("hud_meters", (0x280, 0, 0xA0, 0x8C)), ("hud_message", (0xFA, 0, 0x12C, 0x1A)),
           ("toolbar", (0, 0x1B3, 0xD0, 0xA5)), ("bottom_panel", (0xD0, 0x1F0, 0x250, 0x68))]


def img(p):
    return np.asarray(Image.open(p).convert("RGB")).astype(int) >> 3


def main():
    d, md, s, c = sys.argv[1:5]
    p = img(pathlib.Path(d) / f"{s}.{c}.port.png")
    q = img(pathlib.Path(md) / f"{s}.{c}.port.png")
    r = img(ROOT / "log" / "c3" / "alt" / "refs" / s / "det_b" / f"{c}.png")
    lab = np.full((600, 800), len(REGIONS), np.int16)
    for i in range(len(REGIONS) - 1, -1, -1):
        x, y, w, h = REGIONS[i][1]
        lab[y:y + h, x:x + w] = i
    names = [n for n, _ in REGIONS] + ["view"]
    mask = (p != q).any(2)
    dd = np.abs(p - r).max(2)
    print(f"{s}/{c}: mask px {int(mask.sum())}, differing {int((dd > 0).sum())}")
    for i, n in enumerate(names):
        reg = lab == i
        a = lambda m: int((m & reg).sum())
        print(f"  {n:12s} in-mask one {a(mask & (dd == 1))} beyond {a(mask & (dd > 1))} | outside one {a(~mask & (dd == 1))} "
              f"beyond {a(~mask & (dd > 1))}")
    import importlib.util
    spec = importlib.util.spec_from_file_location("d16", pathlib.Path(__file__).with_name("16_diff.py"))
    D = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(D)
    for title, m in (("outside-mask beyond", ~mask & (dd > 1)), ("in-mask any", mask & (dd > 0))):
        cl = sorted(D.clusters(m), key=lambda t: -len(t))
        print(f" {title}: {len(cl)} clusters")
        for pts in cl[:12]:
            ys = [t[0] for t in pts]
            xs = [t[1] for t in pts]
            y0, x0 = pts[0]
            print(f"   x{min(xs)}..{max(xs)} y{min(ys)}..{max(ys)} n={len(pts)}  ({x0},{y0}) p{tuple(int(v) for v in p[y0, x0])} "
                  f"r{tuple(int(v) for v in r[y0, x0])}")


if __name__ == "__main__":
    main()
