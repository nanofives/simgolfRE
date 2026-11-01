"""S3.25 group 2: the original's map corner in cov_r_s2_screens (box 781..799 x 451..466) from the terrain recording
log/p5/terrain/s325_cov_r_s2_screens (re/tools/terrain_calls.py --pass-frames). Prints the exit root of `course` in the box, per row the
last px that is not one of the dark dither values {0x0000, 0x0020, 0x0420, 0x0421} and the extent of the dither, and the comparisons
with the det_b frame and the scene corpus root.

    ~/bin/py -3.12 log/roadmap/S3/25_group2.py | tee log/roadmap/S3/25_group2.log
"""
import json
import os
import pathlib
import zlib

import numpy as np
from PIL import Image

ROOT = pathlib.Path(__file__).resolve().parents[3]
T = ROOT / "log/p5/terrain/s325_cov_r_s2_screens/frames"
X0, Y0, X1, Y1 = 781, 451, 799, 466
DARK = {0x0000, 0x0020, 0x0420, 0x0421}


def z(p):
    return np.frombuffer(zlib.decompress(pathlib.Path(p).read_bytes()), "<u2").reshape(600, 800)


def png555(p):
    a = np.asarray(Image.open(p).convert("RGB")).astype(np.uint16)
    return ((a[..., 0] >> 3) << 10) | ((a[..., 1] >> 3) << 5) | (a[..., 2] >> 3)


def box(a):
    return a[Y0:Y1 + 1, X0:X1 + 1]


for cp in ("course", "radial_hover", "rep_comments", "rep_histograph", "rep_financial", "rep_report1", "rep_shortcuts", "rep_roster"):
    ex = z(T / cp / "exit.z")
    sp = ROOT / f"port/corpus/scene/cov_r_s2_screens/{cp}.z"
    sc = z(sp) if sp.exists() else None
    ref = png555(ROOT / f"log/c3/alt/refs/cov_r_s2_screens/det_b/{cp}.png")
    m = json.loads((T / cp / "meta.json").read_text())
    print(f"{cp}: terrain frame {m['frame']} passes {[p['name'] for p in m['passes']]}; box px exit != scene root "
          f"{None if sc is None else int((box(ex) != box(sc)).sum())}, exit != det_b {int((box(ex) != box(ref)).sum())}, "
          f"exit != course exit {int((box(ex) != box(z(T / 'course' / 'exit.z'))).sum())}")
ex = z(T / "course" / "exit.z")
print("course exit, RGB555, x 781..799:")
for y in range(Y0, Y1 + 1):
    row = [int(v) for v in ex[y, X0:X1 + 1]]
    terr = [X0 + i for i, v in enumerate(row) if v not in DARK]
    dith = [X0 + i for i, v in enumerate(row) if v in DARK and v != 0]
    print(f"{y} {' '.join(f'{v:04x}' for v in row)}   last terrain x {max(terr) if terr else '-'}; "
          f"nonzero dark px x {min(dith) if dith else '-'}..{max(dith) if dith else '-'}")
