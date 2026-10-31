"""S7.10 mirror: port/build/S7_10_root = `git archive HEAD port re/model/include` + S7.10's own files from the shared tree.

The shared tree holds another step's uncommitted edits of port/ (host.cpp / host.h, terrain_live.*, gl_raster.*, gfx_terrain.cpp,
test_app_*.py), so S7.10 builds here. Every run re-copies the owned files (OWNED below) from the shared tree; the archive is made
once (delete the directory to start over). Builds: <mirror>/port/build/x64 and <mirror>/port/build/w32 (Release).

    ~/bin/py -3.12 log/roadmap/S7/10_mirror.py                    # create (once) + sync the owned files
    ~/bin/py -3.12 log/roadmap/S7/10_mirror.py --build x64|w32|both [--target <t> ...]
"""
from __future__ import annotations

import os
import pathlib
import shutil
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
MIRROR = ROOT / "port" / "build" / "S7_10_root"
JUNCTIONS = ["original", "port/corpus", "re/tools", "log", "tests/golden"]  # read-only uses (test_app_shell)
OWNED = ["port/app/audio_out.h", "port/app/audio_out.cpp", "port/app/ds_sink.h", "port/app/ds_sink.cpp",
         "port/app/tools/audio_check.cpp", "port/app/main.cpp", "port/app/CMakeLists.txt", "port/test/test_app_audio.py",
         "port/src/sim/audio_exe.cpp", "port/include/portsim/sim/audio_exe.h"]
VS_CMAKE = pathlib.Path(r"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions"
                        r"\Microsoft\CMake\CMake\bin\cmake.exe")
TARGETS = ["portsim_c", "simgolf_port", "simgolf_audio_check"]


def create() -> None:
    MIRROR.mkdir(parents=True)
    head = subprocess.run(["git", "rev-parse", "HEAD"], cwd=ROOT, capture_output=True, text=True, check=True).stdout.strip()
    arc = subprocess.run(["git", "archive", "--format=tar", head, "port", "re/model/include"], cwd=ROOT,
                         capture_output=True, check=True).stdout
    subprocess.run(["tar", "-x", "-f", "-", "-C", str(MIRROR)], input=arc, check=True)
    (MIRROR / "HEAD_COMMIT").write_text(head + "\n")
    for j in JUNCTIONS:
        dst = MIRROR / j
        if dst.exists():
            shutil.rmtree(dst)
        dst.parent.mkdir(parents=True, exist_ok=True)
        subprocess.run(["cmd", "/c", "mklink", "/J", str(dst), str(ROOT / j)], check=True, capture_output=True)


def sync() -> None:
    for rel in OWNED:
        src, dst = ROOT / rel, MIRROR / rel
        if not src.exists():
            continue
        if not dst.exists() or dst.read_bytes() != src.read_bytes():
            dst.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy(src, dst)  # a fresh mtime: an edit made during a build must still look newer than its objects
            print("synced", rel, flush=True)


def build(arch: str, targets: list[str]) -> int:
    bdir = MIRROR / "port" / "build" / arch
    cm = str(VS_CMAKE) if VS_CMAKE.exists() else "cmake"
    r = subprocess.run([cm, "-S", str(MIRROR / "port"), "-B", str(bdir), "-A", "x64" if arch == "x64" else "Win32"])
    if r.returncode:
        return r.returncode
    return subprocess.run([cm, "--build", str(bdir), "--config", "Release", "--target", *targets, "--", "/m"]).returncode


if __name__ == "__main__":
    if not MIRROR.exists():
        create()
    sync()
    args = sys.argv[1:]
    if "--build" in args:
        which = args[args.index("--build") + 1]
        tg = [args[i + 1] for i, a in enumerate(args) if a == "--target"] or TARGETS
        for arch in (["x64", "w32"] if which == "both" else [which]):
            rc = build(arch, tg)
            if rc:
                sys.exit(rc)
    sys.exit(0)
