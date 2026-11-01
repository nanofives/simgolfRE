"""S3.21b: the attribute-stepping layer (U-1309; opt-in `#tune 30`, default OFF: frames unchanged) on top of part 1.

    ~/bin/py -3.12 log/roadmap/S3/21b_assemble.py --root <tree>     # a tree that already holds 21_edits.py (21_assemble.py)
    ~/bin/py -3.12 log/roadmap/S3/21b_assemble.py --dst S3_21       # plain f2dff003 + 21_assemble.py + 21b_edits.py

Every edit is anchored (old text exactly once)."""
import argparse
import importlib.util
import pathlib
import subprocess
import sys

HERE = pathlib.Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("s3_11_assemble", HERE / "11_assemble.py")
A = importlib.util.module_from_spec(spec)
spec.loader.exec_module(A)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--dst", default="S3_21")
    ap.add_argument("--root")
    a = ap.parse_args()
    if a.root:
        dst = pathlib.Path(a.root).resolve()
    else:
        subprocess.run([sys.executable, str(HERE / "21_assemble.py"), "--dst", a.dst], check=True)
        dst = A.ROOT / "port" / "build" / a.dst
    env = A.env_of(HERE / "21b_edits.py")
    R = A.Root(dst, False)
    A.edits(R, env["EDITS"], "s321b 21b_edits")
    R.flush()
    print("done", dst)


if __name__ == "__main__":
    main()
