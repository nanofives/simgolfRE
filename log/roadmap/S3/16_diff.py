"""S3.16: where a framegate port image differs from the reference: connected clusters of differing pixels (8-neighbour), each with
its box, pixel count, one-level / beyond count and a sample of (x, y, port 5-bit, ref 5-bit).

    ~/bin/py -3.12 log/roadmap/S3/16_diff.py <img dir> <scenario> <checkpoint> [--region x,y,w,h] [--beyond] [--crop <png>]
"""
import argparse
import pathlib
import sys

import numpy as np
from PIL import Image

ROOT = pathlib.Path(__file__).resolve().parents[3]
REFS = ROOT / "log" / "c3" / "alt" / "refs"


def load(d, s, c):
    port = np.asarray(Image.open(pathlib.Path(d) / f"{s}.{c}.port.png").convert("RGB")).astype(int)
    ref = np.asarray(Image.open(REFS / s / "det_b" / f"{c}.png").convert("RGB")).astype(int)
    return port, ref


def clusters(mask):
    H, W = mask.shape
    seen = np.zeros_like(mask, bool)
    out = []
    ys, xs = np.nonzero(mask)
    for y0, x0 in zip(ys, xs):
        if seen[y0, x0]:
            continue
        st = [(y0, x0)]
        seen[y0, x0] = True
        pts = []
        while st:
            y, x = st.pop()
            pts.append((y, x))
            for dy in (-1, 0, 1):
                for dx in (-1, 0, 1):
                    yy, xx = y + dy, x + dx
                    if 0 <= yy < H and 0 <= xx < W and mask[yy, xx] and not seen[yy, xx]:
                        seen[yy, xx] = True
                        st.append((yy, xx))
        out.append(pts)
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("dir")
    ap.add_argument("scenario")
    ap.add_argument("checkpoint")
    ap.add_argument("--region")
    ap.add_argument("--beyond", action="store_true", help="only pixels beyond one level")
    ap.add_argument("--crop", help="write port|ref|diff crop (x4) of the largest cluster box to this png")
    ap.add_argument("--max", type=int, default=25)
    a = ap.parse_args()
    port, ref = load(a.dir, a.scenario, a.checkpoint)
    p5, r5 = port >> 3, ref >> 3
    d = np.abs(p5 - r5).max(axis=2)
    mask = d > (1 if a.beyond else 0)
    if a.region:
        x, y, w, h = [int(v, 0) for v in a.region.split(",")]
        m2 = np.zeros_like(mask)
        m2[y:y + h, x:x + w] = True
        mask &= m2
    cl = sorted(clusters(mask), key=lambda p: -len(p))
    print(f"{a.scenario}/{a.checkpoint}: {int(mask.sum())} px in {len(cl)} clusters")
    for pts in cl[:a.max]:
        ys = [p[0] for p in pts]
        xs = [p[1] for p in pts]
        one = sum(1 for y, x in pts if d[y, x] == 1)
        smp = []
        for y, x in pts[:4]:
            smp.append(f"({x},{y}) p{tuple(p5[y, x])} r{tuple(r5[y, x])}")
        print(f"  box x{min(xs)}..{max(xs)} y{min(ys)}..{max(ys)} n={len(pts)} one={one} beyond={len(pts) - one}  " + " ".join(smp))
    if a.crop and cl:
        pts = cl[0]
        ys = [p[0] for p in pts]
        xs = [p[1] for p in pts]
        x0, x1 = max(0, min(xs) - 8), min(800, max(xs) + 9)
        y0, y1 = max(0, min(ys) - 8), min(600, max(ys) + 9)
        P = port[y0:y1, x0:x1].astype(np.uint8)
        R = ref[y0:y1, x0:x1].astype(np.uint8)
        D = (mask[y0:y1, x0:x1, None] * np.array([255, 0, 255])).astype(np.uint8)
        img = np.concatenate([P, R, D], axis=1)
        Image.fromarray(img).resize((img.shape[1] * 4, img.shape[0] * 4), Image.NEAREST).save(a.crop)
        print("crop", a.crop, (x0, y0, x1, y1))


if __name__ == "__main__":
    sys.exit(main())
