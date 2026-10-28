"""S3.4 on the Win32 backend: port/test/test_flic.py's comparison through the Win32 runner of this step's build
(port/build/S3_head/port/build/S3_4_w32), the paths lib32 normally takes from port/build/Win32 redirected here.

    ~/bin/py -3.12 log/roadmap/S3/4_w32_run.py [--all] [--negctl N]
"""
import argparse
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT))
from port.test import build as B  # noqa: E402
from port.test import lib32  # noqa: E402
from port.test import test_flic as T  # noqa: E402

W32 = ROOT / "port" / "build" / "S3_head" / "port" / "build" / "S3_4_w32" / "Release"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--all", action="store_true")
    ap.add_argument("--negctl", type=int, default=0)
    a = ap.parse_args()
    B.runner32_path = lambda config="Release": W32 / "portsim_runner32.exe"
    B.dll_path = lambda arch=None, config="Release": W32 / "portsim_c.dll"
    B.runner32_stale = lambda config="Release": False
    lib = lib32.load32(rebuild=False)
    print("backend:", lib.path, "pointer size", lib.pointer_size)
    if a.negctl:
        lib.negctl("flic.load", a.negctl)
    r = T.run_all(lib, not a.all)
    for k, (eq, n) in r["by_rec"].items():
        print(f"{k}: {eq} / {n} loads equal")
    print(f"total {r['equal']} / {r['loads']} loads equal ({r['cells']} recorded cells)")
    for f in r["fail"][:10]:
        print("FAIL", f)
    return 0 if not r["fail"] or a.negctl else 1


if __name__ == "__main__":
    sys.exit(main())
