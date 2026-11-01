"""S3 wave 8 tree = `w7_assemble.py --plain` of the merge base (default 2291a342, I20 merged) + the wave 8 steps' edits, each
through its own assembler's --root form, in the lead's order (each anchor exactly once; each assembler refuses otherwise and this
script stops at the first failure):

    1. W8.6   22_assemble.py --only-22     (gfx_chareditor.cpp: the unused body showCharacterEditor.bio removed)
    2. S3.21  21_assemble.py --root        (gl_raster.cpp / .h, test_gl_raster.py: float32 vertex rule + clipping, default on)
    3. S3.21b 21b_assemble.py --root       (gl_raster.cpp, test_gl_raster.py: #tune 30 attribute stepping, default OFF)
    4. S3.24  24_assemble.py --only-24     (terrain_overlay.cpp / .h, test_terrain_overlay.py: line span rule 2)
    5. S3.23  23_assemble.py --only-23     (framegate.py --split, test_framegate_split.py)
    6. S3.26  26_assemble.py --only-26     (gfx bodies and hand-overs, framegate.py, gfx.py, tests; after 22 and 23)

No CLAUDE.md / UNCERTAINTIES.md edit goes into the tree (MERGE_w8.md holds them as text).

    ~/bin/py -3.12 log/roadmap/S3/w8_assemble.py [--dst S3_W8] [--commit 2291a342] [--no-21b]
"""
import argparse
import pathlib
import subprocess
import sys

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parents[2]
PY = sys.executable


def run(args):
    print("+", " ".join(str(a) for a in args), flush=True)
    r = subprocess.run([PY] + [str(a) for a in args], cwd=ROOT)
    if r.returncode:
        sys.exit(f"step failed (rc {r.returncode}): {args[0]}")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--dst", default="S3_W8")
    ap.add_argument("--commit", default="2291a342")
    ap.add_argument("--no-21b", action="store_true")
    a = ap.parse_args()
    tree = ROOT / "port" / "build" / a.dst
    run([HERE / "w7_assemble.py", "--dst", a.dst, "--commit", a.commit, "--plain"])
    run([HERE / "22_assemble.py", "--root", tree, "--only-22"])
    run([HERE / "21_assemble.py", "--root", tree])
    if not a.no_21b:
        run([HERE / "21b_assemble.py", "--root", tree])
    run([HERE / "24_assemble.py", "--root", tree, "--only-24"])
    run([HERE / "23_assemble.py", "--root", tree, "--only-23"])
    run([HERE / "26_assemble.py", "--root", tree, "--only-26"])
    print(f"wave 8 tree ready: {tree}")


if __name__ == "__main__":
    main()
