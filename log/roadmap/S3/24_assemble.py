"""S3.24: the tree at f2dff003 (wave 7 committed) + 24_edits.py (the overlay line span rule).

    ~/bin/py -3.12 log/roadmap/S3/24_assemble.py [--dst S3_24] [--commit f2dff003]     # a fresh plain tree, then the edits
    ~/bin/py -3.12 log/roadmap/S3/24_assemble.py --root <dir> --only-24 [--dry]        # an existing tree at f2dff003

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


def apply24(dst: pathlib.Path, dry: bool = False):
    env = A.env_of(HERE / "24_edits.py")
    R = A.Root(dst, dry)
    A.edits(R, env["EDITS"], "s324 24_edits")
    R.flush()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--dst", default="S3_24")
    ap.add_argument("--commit", default="f2dff003")
    ap.add_argument("--root")
    ap.add_argument("--only-24", action="store_true")
    ap.add_argument("--dry", action="store_true")
    a = ap.parse_args()
    if a.root and a.only_24:
        dst = pathlib.Path(a.root).resolve()
    elif a.root:
        sys.exit("--root needs --only-24")
    else:
        subprocess.run([sys.executable, str(HERE / "w7_assemble.py"), "--dst", a.dst, "--commit", a.commit, "--plain"], check=True)
        dst = A.ROOT / "port" / "build" / a.dst
    apply24(dst, a.dry)
    print("done", dst)


if __name__ == "__main__":
    main()
