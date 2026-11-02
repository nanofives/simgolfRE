"""S3 wave 9 tree = the wave 8 stack (w8_assemble.py on the base) + the wave 8 fix layers for I21 (w8_fix_i21.py, w8_fix_i21b.py)
+ S3.28 (lowerEdgeCorner's black-light pass) + S3.29 (textured-interpolation rule, #tune 32, default off), each through its own
assembler's --root form. Every anchor exactly once; each assembler refuses otherwise and this script stops there.

    ~/bin/py -3.12 log/roadmap/S3/w9_assemble.py [--dst S3_W9] [--commit 2291a342]
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
    ap.add_argument("--dst", default="S3_W9")
    ap.add_argument("--commit", default="2291a342")
    a = ap.parse_args()
    tree = ROOT / "port" / "build" / a.dst
    run([HERE / "w8_assemble.py", "--dst", a.dst, "--commit", a.commit])
    run([HERE / "w8_fix_i21.py", "--root", tree])
    run([HERE / "w8_fix_i21b.py", "--root", tree])
    run([HERE / "28_assemble.py", "--root", tree, "--only-28"])
    run([HERE / "29_assemble.py", "--root", tree, "--only-29"])
    print(f"wave 9 tree ready: {tree}")


if __name__ == "__main__":
    main()
