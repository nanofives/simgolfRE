r"""S3.27: the map-corner box 781..799 x 451..466 in the original's kept passes (27_gl_record.py) and in the port's frames.

    ~/bin/py -3.12 log/roadmap/S3/27_analyze.py passes [--label s327_s2]      # per kept pass: box px written, equal to course's exit?
    ~/bin/py -3.12 log/roadmap/S3/27_analyze.py box <step> [pre|post]          # the box as RGB555 hex rows
DIB memory row 0 is the picture's top row (S3.2 notes 1).
"""
import pathlib
import sys
import zlib

import numpy as np

ROOT = pathlib.Path(__file__).resolve().parents[3]
X0, X1, Y0, Y1 = 781, 799, 451, 466     # inclusive
W, H = 800, 600


def dib(p):
    return np.frombuffer(p.read_bytes(), "<u2").reshape(H, W)


def box(a):
    return a[Y0:Y1 + 1, X0:X1 + 1]


def course_exit():
    p = ROOT / "log/p5/terrain/s325_cov_r_s2_screens/frames/course/exit.z"
    return np.frombuffer(zlib.decompress(p.read_bytes()), "<u2").reshape(H, W)


def meta(d):
    out = {}
    for ln in (d / "meta.txt").read_text().splitlines():
        k, _, v = ln.partition(" ")
        out[k] = v
    return out


def cmd_passes(label):
    root = ROOT / "log/p5/gl/corpus" / label / "snaps"
    ce = box(course_exit())
    for d in sorted(root.iterdir()):
        m = meta(d)
        pre, post = box(dib(d / "dib_pre.bin")), box(dib(d / "dib_post.bin"))
        full_pre, full_post = dib(d / "dib_pre.bin"), dib(d / "dib_post.bin")
        print(f"{d.name:24s} kind {m.get('entry_kind'):>2} ord {m.get('entry_ord'):>3} a0 {int(m.get('entry_a0', 0)):#x} "
              f"lines {m.get('passLines'):>6}  box written {int((pre != post).sum()):3d}  post==course_exit {int((post != ce).sum()):3d} px differ"
              f"  frame written {int((full_pre != full_post).sum())}")


def cmd_box(label, step, which):
    d = ROOT / "log/p5/gl/corpus" / label / "snaps" / step
    a = box(dib(d / f"dib_{which}.bin"))
    for y, row in zip(range(Y0, Y1 + 1), a):
        print(y, " ".join(f"{v:04x}" for v in row))


if __name__ == "__main__":
    args = sys.argv[1:]
    label = "s327_s2"
    if "--label" in args:
        i = args.index("--label"); label = args[i + 1]; del args[i:i + 2]
    if args[0] == "passes":
        cmd_passes(label)
    elif args[0] == "box":
        cmd_box(label, args[1], args[2] if len(args) > 2 else "post")
