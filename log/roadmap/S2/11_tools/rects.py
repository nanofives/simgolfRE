"""S2.11: measure the dialog rectangles on the reference frames (log/c3/alt/refs/<scenario>/det_b/<checkpoint>.png).

usage: ~/bin/py -3.12 log/roadmap/S2/11_tools/rects.py [image.png ...]
For each checkpoint (or each image given): the bounding boxes (x0, y0, x1, y1, pixels) of the 4-connected regions of
  - the SimFoto border colours 0x7fff (white) and 0x7d08 (narrateShot's flag colour), 2,000 pixels or more, after a 5 x 5
    opening of the colour mask (white label text touching the border joined it: ui_tour menus measured 2 columns too wide
    without it): the border is a solid 16-pixel ring, which the opening keeps whole, so its box is the frame's outer
    rectangle;
  - the message ball frame's outline colour RGB (206, 206, 231) (drawFrameR3's border pieces, TransPopups cells 0..8),
    300 pixels or more.
"""
import pathlib
import sys
from collections import deque

import numpy as np
from PIL import Image

ROOT = pathlib.Path(__file__).resolve().parents[4]
REFS = ROOT / "log" / "c3" / "alt" / "refs"
CHECKPOINTS = [("championship", "tour_0180"), ("ui_tour", "menus"), ("cov_k1_animals", "bought"),
               ("cov_b_inject2", "debug_overlay"), ("cov_k2_simfoto", "bottom"), ("cov_k2_simfoto", "right"),
               ("cov_a_golfer", "button_2")]


def exp(v):   # the back buffer's RGB555 -> 8-bit expansion (v << 3) | (v >> 2)
    return (v << 3) | (v >> 2)


def col(c):
    return np.array([exp((c >> 10) & 31), exp((c >> 5) & 31), exp(c & 31)])


def comps(m, minsize):
    h, w = m.shape
    seen = np.zeros_like(m)
    out = []
    ys, xs = np.where(m)
    for y0, x0 in zip(ys, xs):
        if seen[y0, x0]:
            continue
        q = deque([(y0, x0)])
        seen[y0, x0] = 1
        pts = []
        while q:
            y, x = q.popleft()
            pts.append((y, x))
            for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                yy, xx = y + dy, x + dx
                if 0 <= yy < h and 0 <= xx < w and m[yy, xx] and not seen[yy, xx]:
                    seen[yy, xx] = 1
                    q.append((yy, xx))
        if len(pts) >= minsize:
            a = np.array(pts)
            out.append((int(a[:, 1].min()), int(a[:, 0].min()), int(a[:, 1].max()), int(a[:, 0].max()), len(pts)))
    return out


def opening(m, k=5):
    """binary erosion then dilation with a k x k square (pixels outside the image count as unset)"""
    def shift_all(a, op):
        h, w = a.shape
        r = k // 2
        p = np.pad(a, r, constant_values=False)
        out = np.ones_like(a) if op == "and" else np.zeros_like(a)
        for dy in range(k):
            for dx in range(k):
                v = p[dy:dy + h, dx:dx + w]
                out = (out & v) if op == "and" else (out | v)
        return out
    return shift_all(shift_all(m, "and"), "or")


def measure(path):
    r = np.asarray(Image.open(path).convert("RGB")).astype(int)
    return {"simfoto_white": comps(opening((r == col(0x7FFF)).all(2)), 2000),
            "simfoto_flag": comps(opening((r == col(0x7D08)).all(2)), 2000),
            "ball_outline": comps((r == np.array([206, 206, 231])).all(2), 300)}


def main(argv):
    paths = [pathlib.Path(a) for a in argv] or [REFS / s / "det_b" / f"{c}.png" for s, c in CHECKPOINTS]
    for p in paths:
        print(p.relative_to(ROOT) if p.is_relative_to(ROOT) else p)
        for k, v in measure(p).items():
            print(f"  {k}: {v}")


if __name__ == "__main__":
    main(sys.argv[1:])
