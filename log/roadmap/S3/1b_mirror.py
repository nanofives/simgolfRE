"""S3.1b private build: the S3 mirror's port/ (port/build/S3_head = git archive of c6d7d78b) plus S3.1's files and its
merge edit (log/roadmap/S3/1_mirror.py, reused as is) plus this step's own files (the rasteriser and its tests).

    ~/bin/py -3.12 log/roadmap/S3/1b_mirror.py [--w32] [--no-build]

Source copy: port/build/S3_1b_src/port (re/ beside it is a junction to the mirror's re/). Builds:
port/build/S3_head/port/build/S3_1b (x64) and S3_1b_w32 (Win32). Tests run in the mirror:
    cd port/build/S3_head && SG_PORTSIM_DLL=<dll> ~/bin/py -3.12 -m pytest port/test/test_gl_raster.py
"""
import argparse
import importlib.util
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
spec = importlib.util.spec_from_file_location("s31_mirror", ROOT / "log" / "roadmap" / "S3" / "1_mirror.py")
M = importlib.util.module_from_spec(spec)
spec.loader.exec_module(M)

M.SRC = ROOT / "port" / "build" / "S3_1b_src"
M.BUILD = {"x64": M.MIRROR / "port" / "build" / "S3_1b", "Win32": M.MIRROR / "port" / "build" / "S3_1b_w32"}
OWN_1B = ["port/src/sim/gl_raster.cpp", "port/include/portsim/sim/gl_raster.h", "port/test/test_gl_raster.py",
          "port/test/glraster.py", "port/test/glprobe.py"]
M.OWN = M.OWN + OWN_1B


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--w32", action="store_true")
    ap.add_argument("--no-build", action="store_true")
    a = ap.parse_args()
    M.copy_tree()
    if a.no_build:
        return 0
    print("SG_PORTSIM_DLL=" + str(M.build("x64")))
    if a.w32:
        print("Win32:", M.build("Win32"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
