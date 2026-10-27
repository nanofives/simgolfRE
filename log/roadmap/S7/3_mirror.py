"""S7.3 mirror sync: copies this step's files (and S7.2's accepted port/app files) into the HEAD export port/build/S7_3_head
(recipe log/roadmap/S0/export.md), applies the one line of log/roadmap/S7/2_merge.md to the export's port/CMakeLists.txt, and
optionally builds the app the way port/test/test_app_shell.py does (standalone port/app with PORTAPP_SHELL=ON, x64 Release:
targets simgolf_port and portsim_c in port/build/S7_3_app of the export).

    ~/bin/py -3.12 log/roadmap/S7/3_mirror.py            # sync
    ~/bin/py -3.12 log/roadmap/S7/3_mirror.py --build    # sync + build
"""
from __future__ import annotations

import pathlib
import shutil
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
MIRROR = ROOT / "port" / "build" / "S7_3_head"
LINE = "add_subdirectory(app)"
DIRS = ["port/app"]
FILES = ["port/test/test_app_shell.py", "port/test/test_app_input.py"]


def sync() -> None:
    for d in DIRS:
        for src in (ROOT / d).rglob("*"):
            if src.is_file() and "__pycache__" not in src.parts:
                dst = MIRROR / src.relative_to(ROOT)
                dst.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(src, dst)
    for rel in FILES:
        src = ROOT / rel
        if src.exists():
            dst = MIRROR / rel
            dst.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(src, dst)
    cm = MIRROR / "port" / "CMakeLists.txt"
    text = cm.read_text()
    if LINE not in text:
        cm.write_text(text.rstrip("\n") + "\n\n# S7.2 (mirror only; the integrator applies it from log/roadmap/S7/2_merge.md)\n" + LINE + "\n")


def build() -> int:
    sys.path.insert(0, str(MIRROR))
    from port.test import build as B  # the export's own copy
    cmake = B.cmake_exe()
    bdir = MIRROR / "port" / "build" / "S7_3_app"
    r = subprocess.run([cmake, "-S", str(MIRROR / "port" / "app"), "-B", str(bdir), "-A", "x64", "-DPORTAPP_SHELL=ON"])
    if r.returncode:
        return r.returncode
    return subprocess.run([cmake, "--build", str(bdir), "--config", "Release", "--target", "simgolf_port", "portsim_c",
                           "--", "/m"]).returncode


if __name__ == "__main__":
    sync()
    sys.exit(build() if "--build" in sys.argv else 0)
