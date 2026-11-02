"""S3.29: the wave 8 stack (w8_assemble.py) + 29_edits.py (U-1309: the textured walk in GlRaster, opt-in `#tune 32 1`; gl_raster.cpp / .h, test_gl_raster.py).

    ~/bin/py -3.12 log/roadmap/S3/29_assemble.py [--dst S3_29] [--commit 2291a342]     # a fresh wave 8 tree, then the edits
    ~/bin/py -3.12 log/roadmap/S3/29_assemble.py --root <dir> --only-29 [--dry]        # an existing wave 8 tree

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


def apply29(dst: pathlib.Path, dry: bool = False):
    env = A.env_of(HERE / "29_edits.py")
    R = A.Root(dst, dry)
    A.edits(R, env["EDITS"], "s329 29_edits")
    R.flush()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--dst", default="S3_29")
    ap.add_argument("--commit", default="2291a342")
    ap.add_argument("--root")
    ap.add_argument("--only-29", action="store_true")
    ap.add_argument("--dry", action="store_true")
    a = ap.parse_args()
    if a.root and a.only_29:
        dst = pathlib.Path(a.root).resolve()
    elif a.root:
        sys.exit("--root needs --only-29")
    else:
        subprocess.run([sys.executable, str(HERE / "w8_assemble.py"), "--dst", a.dst, "--commit", a.commit], check=True)
        dst = A.ROOT / "port" / "build" / a.dst
    apply29(dst, a.dry)
    print("done", dst)


if __name__ == "__main__":
    main()
