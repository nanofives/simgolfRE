"""S3.31: I22's port tree (w10_base.py) + 31_edits.py (the overlay line's float32 vertex stage, line rule 8).

    ~/bin/py -3.12 log/roadmap/S3/31_assemble.py [--dst S3_31chk]           # a fresh w10_base tree, then the edits
    ~/bin/py -3.12 log/roadmap/S3/31_assemble.py --root <dir> --only-31 [--dry]

Every edit is anchored (old text exactly once; an edit whose new text is already there is refused)."""
import argparse
import importlib.util
import pathlib
import subprocess
import sys

HERE = pathlib.Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("s3_11_assemble", HERE / "11_assemble.py")
A = importlib.util.module_from_spec(spec)
spec.loader.exec_module(A)


def apply31(dst: pathlib.Path, dry: bool = False):
    env = A.env_of(HERE / "31_edits.py")
    R = A.Root(dst, dry)
    A.edits(R, env["EDITS"], "s331 31_edits")
    R.flush()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--dst", default="S3_31chk")
    ap.add_argument("--root")
    ap.add_argument("--only-31", action="store_true")
    ap.add_argument("--dry", action="store_true")
    a = ap.parse_args()
    if a.root and a.only_31:
        dst = pathlib.Path(a.root).resolve()
    elif a.root:
        sys.exit("--root needs --only-31")
    else:
        subprocess.run([sys.executable, str(HERE / "w10_base.py"), "--dst", a.dst], check=True)
        dst = A.ROOT / "port" / "build" / a.dst
    apply31(dst, a.dry)
    print("done", dst)


if __name__ == "__main__":
    main()
