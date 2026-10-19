"""Build portsim_c.dll (the C ABI the harness loads) with CMake and the Visual Studio 2022 Build Tools.

    ~/bin/py -3.12 port/test/build.py [--arch x64|Win32] [--config Release|Debug] [--negctl 0|1]

The DLL's architecture must match the Python's (64-bit here: x64). Output: port/build/<arch>/<config>/portsim_c.dll.
The build also compiles the layout test (ctest gamestate_layout).
"""
from __future__ import annotations

import argparse
import os
import pathlib
import shutil
import struct
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
PORT = ROOT / "port"
VS = pathlib.Path(r"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools")
VS_CMAKE = VS / "Common7" / "IDE" / "CommonExtensions" / "Microsoft" / "CMake" / "CMake" / "bin" / "cmake.exe"


def default_arch() -> str:
    return "x64" if struct.calcsize("P") == 8 else "Win32"


def cmake_exe() -> str:
    if VS_CMAKE.exists():
        return str(VS_CMAKE)
    found = shutil.which("cmake")
    if not found:
        raise RuntimeError("cmake not found (Visual Studio 2022 Build Tools or PATH)")
    return found


def build_dir(arch: str) -> pathlib.Path:
    return PORT / "build" / arch


def dll_path(arch: str | None = None, config: str = "Release") -> pathlib.Path:
    return build_dir(arch or default_arch()) / config / "portsim_c.dll"


def sources() -> list[pathlib.Path]:
    pats = ("include/**/*.h", "include/*.inc", "src/**/*.cpp", "src/**/*.inc", "src/**/*.h", "CMakeLists.txt")
    out = []
    for p in pats:
        out += PORT.glob(p)
    out += (ROOT / "re" / "model" / "include").glob("*.h")
    return out


def stale(arch: str | None = None, config: str = "Release") -> bool:
    d = dll_path(arch, config)
    if not d.exists():
        return True
    t = d.stat().st_mtime
    return any(s.stat().st_mtime > t for s in sources())


def build(arch: str | None = None, config: str = "Release", negctl: int = 1, quiet: bool = False) -> pathlib.Path:
    arch = arch or default_arch()
    cm = cmake_exe()
    bd = build_dir(arch)
    run = dict(capture_output=quiet, text=True, errors="replace")
    r = subprocess.run([cm, "-S", str(PORT), "-B", str(bd), "-A", arch, f"-DPORTSIM_NEGCTL={'ON' if negctl else 'OFF'}"], **run)
    if r.returncode:
        raise RuntimeError(f"cmake configure failed\n{(r.stdout or '')[-4000:]}{(r.stderr or '')[-2000:]}")
    r = subprocess.run([cm, "--build", str(bd), "--config", config, "--parallel"], **run)
    if r.returncode:
        raise RuntimeError(f"cmake build failed\n{(r.stdout or '')[-6000:]}{(r.stderr or '')[-2000:]}")
    out = dll_path(arch, config)
    if not out.exists():
        raise RuntimeError(f"build finished but {out} is missing")
    return out


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--arch", default=default_arch(), choices=["x64", "Win32"])
    ap.add_argument("--config", default="Release", choices=["Release", "Debug"])
    ap.add_argument("--negctl", type=int, default=1, choices=[0, 1])
    a = ap.parse_args()
    print(build(a.arch, a.config, a.negctl))
    return 0


if __name__ == "__main__":
    sys.exit(main())
