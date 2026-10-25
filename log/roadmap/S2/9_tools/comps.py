"""S2.9: measure the dialog rectangles on the reference frames (log/c3/alt/refs/<scenario>/det_b/<checkpoint>.png).

usage: ~/bin/py -3.12 log/roadmap/S2/9_tools/comps.py
For each checkpoint: the bounding boxes (x0, y0, x1, y1, pixels) of the 4-connected regions of 2,000 pixels or more in the
panel fill colour 0x4e79 (and, for k_dlg_J, the grading page's panel colour RGB (148, 148, 198)), and the rectangle the test
uses: the box grown by 5 pixels on every side.
"""
import pathlib
from collections import deque

import numpy as np
from PIL import Image

ROOT = pathlib.Path(__file__).resolve().parents[4]
REFS = ROOT / "log" / "c3" / "alt" / "refs"
CHECKPOINTS = [("cov_r_quit", "exit_prompt"), ("cov_r_quit", "exit_save"), ("cov_a_keys", "k_C"), ("cov_a_keys", "k_dlg_S"),
               ("cov_a_keys", "k_dlg_J"), ("cov_a_keys2", "C_prompt"), ("cov_a_champ", "n_prompt"), ("cov_a_play", "n_prompt"),
               ("cov_a_build2", "remove_prompt1"), ("cov_k3_career", "prompt"), ("cov_k3_career", "prompt2")]


def exp(v):   # the back buffer's RGB555 -> 8-bit expansion (v << 3) | (v >> 2)
    return (v << 3) | (v >> 2)


def col(c):
    return np.array([exp((c >> 10) & 31), exp((c >> 5) & 31), exp(c & 31)])


def comps(m, minsize=2000):
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


def main():
    for s, c in CHECKPOINTS:
        r = np.asarray(Image.open(REFS / s / "det_b" / f"{c}.png").convert("RGB")).astype(int)
        boxes = comps((r == col(0x4E79)).all(2))
        if c == "k_dlg_J":
            boxes = comps((r == np.array([148, 148, 198])).all(2), 5000)
        rects = [(x0 - 5, y0 - 5, x1 + 5, y1 + 5) for x0, y0, x1, y1, _n in boxes]
        print(s, c, "regions", boxes, "rects", rects)


if __name__ == "__main__":
    main()
