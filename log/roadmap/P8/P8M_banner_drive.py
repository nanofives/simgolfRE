"""P8 merge check: P8A's loading banner on the merged tree with P8B's --wide default.
From the repo root: ~/bin/py -3.12 log/roadmap/P8/P8M_banner_drive.py load --resize 1600x900 --out log/roadmap/P8/P8M_runs/banner_wide"""
import pathlib, runpy, sys
ROOT = pathlib.Path.cwd()
sys.path.insert(0, str(ROOT / "log" / "roadmap" / "P5"))
sys.path.insert(0, str(ROOT / "log" / "roadmap" / "P8"))
import P5B_drive as P  # noqa: E402
import P8A_drive as A  # noqa: E402
A.P.EXE = ROOT / "port" / "build" / "P8M" / "port" / "build" / "x64" / "app" / "Release" / "simgolf_port.exe"
if __name__ == "__main__":
    A.main()
