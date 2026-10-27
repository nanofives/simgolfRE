"""S7.2 mirror sync: copies this step's files into the HEAD export (port/build/S7_2_head, recipe log/roadmap/S0/export.md)
and applies the one line the integrator will apply to port/CMakeLists.txt (2_merge.md). Usage:

    ~/bin/py -3.12 log/roadmap/S7/2_mirror.py            # sync
    ~/bin/py -3.12 log/roadmap/S7/2_mirror.py --build    # sync + build simgolf_input_check (x64 Release)
"""
from __future__ import annotations

import pathlib
import shutil
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
MIRROR = ROOT / "port" / "build" / "S7_2_head"
LINE = "add_subdirectory(app)"
FILES = [
    "port/app/CMakeLists.txt",
    "port/app/input_map.h",
    "port/app/input_map.cpp",
    "port/app/tools/input_check.cpp",
    "port/test/test_app_input.py",
]


def sync() -> None:
    for rel in FILES:
        src = ROOT / rel
        if not src.exists():
            continue
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
    bdir = MIRROR / "port" / "build" / "S7_2"
    r = subprocess.run([cmake, "-S", str(MIRROR / "port"), "-B", str(bdir), "-A", "x64"])
    if r.returncode:
        return r.returncode
    return subprocess.run([cmake, "--build", str(bdir), "--config", "Release", "--target", "simgolf_input_check"]).returncode


if __name__ == "__main__":
    sync()
    sys.exit(build() if "--build" in sys.argv else 0)
