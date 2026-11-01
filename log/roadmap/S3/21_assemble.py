"""S3.21: the tree with the generic GL's float32 vertex rule and frustum clipping in GlRaster = `w7_assemble.py --plain`'s tree
(git archive f2dff003, which holds wave 7) + 21_edits.py (EDITS: gl_raster.cpp, gl_raster.h, test_gl_raster.py).

    ~/bin/py -3.12 log/roadmap/S3/21_assemble.py [--dst S3_21] [--commit f2dff003]
    ~/bin/py -3.12 log/roadmap/S3/21_assemble.py --root <dir>      # an existing tree that already holds f2dff003's port/

Every edit is anchored (old text exactly once). Builds are separate (port/test/build.py). No shared file is edited."""
import argparse
import importlib.util
import pathlib
import subprocess
import sys

HERE = pathlib.Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("s3_11_assemble", HERE / "11_assemble.py")
A = importlib.util.module_from_spec(spec)
spec.loader.exec_module(A)


def apply21(dst: pathlib.Path, dry: bool = False):
    env = A.env_of(HERE / "21_edits.py")
    R = A.Root(dst, dry)
    A.edits(R, env["EDITS"], "s321 21_edits")
    R.flush()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--dst", default="S3_21")
    ap.add_argument("--commit", default="f2dff003")
    ap.add_argument("--root")
    ap.add_argument("--dry", action="store_true")
    a = ap.parse_args()
    if a.root:
        dst = pathlib.Path(a.root).resolve()
    else:
        subprocess.run([sys.executable, str(HERE / "w7_assemble.py"), "--dst", a.dst, "--commit", a.commit, "--plain"], check=True)
        dst = A.ROOT / "port" / "build" / a.dst
    apply21(dst, a.dry)
    print("done", dst)


if __name__ == "__main__":
    main()
