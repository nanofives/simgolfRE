"""S7.6 mirror: port/build/S7_6_head = `git archive HEAD` + the S0 junctions (log/roadmap/S0/export.md, plus shim/build and
re/replay as S7.1 had) + S7.1's accepted, unmerged files (live.h / live.cpp / live.py / test_live.py, copied from
port/build/S7_1_head on creation only) with its two patches applied (1_tick.patch, 1_capi.patch, checked with `patch
--dry-run` first) and its modules.inc line + S7.2's CMake line (add_subdirectory(app)).

Every run (also `--sync`) copies the shared tree's port/app (S7.2 / S7.3 files and this step's host.*) and the app tests
into the mirror. Files edited in the mirror itself (live.*, test_live.py, and any shared file this step patches) are never
overwritten after creation.

    ~/bin/py -3.12 log/roadmap/S7/6_mirror.py            # create (once) + sync
    ~/bin/py -3.12 log/roadmap/S7/6_mirror.py --build    # + build portsim_c / simgolf_port (x64 Release, port/build/S7_6 of the mirror)
"""
from __future__ import annotations

import pathlib
import shutil
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
# --name <dir under port/build> (default S7_6_head, made on HEAD c6d7d78b; S7_6_h2 = the rebase on HEAD 2c05f02a, which takes
# the live files from --from <mirror> and applies 6_capi.patch too)
_ARGS = sys.argv[1:]
_NAME = _ARGS[_ARGS.index("--name") + 1] if "--name" in _ARGS else "S7_6_head"
_FROM = _ARGS[_ARGS.index("--from") + 1] if "--from" in _ARGS else "S7_1_head"
MIRROR = ROOT / "port" / "build" / _NAME
S71 = ROOT / "port" / "build" / _FROM
JUNCTIONS = ["original", "log", "port/corpus", "tests/golden", "tests/fixtures", "re/versions", "tools/vc6", "shim/build",
             "re/replay"]
S71_FILES = ["port/include/portsim/sim/live.h", "port/src/sim/live.cpp", "port/test/live.py", "port/test/test_live.py"]
PATCHES = ["log/roadmap/S7/1_tick.patch", "log/roadmap/S7/1_capi.patch"] + (["log/roadmap/S7/6_capi.patch"] if _FROM != "S7_1_head" else [])
SYNC_DIRS = ["port/app"]
SYNC_FILES = ["port/test/test_app_shell.py", "port/test/test_app_input.py", "port/test/test_app_live.py"]
CMAKE_LINE = "add_subdirectory(app)"
MODULE_LINE = "SG_MODULE(live)"


def run(cmd, **kw):
    print("+", " ".join(str(c) for c in cmd), flush=True)
    return subprocess.run(cmd, check=True, **kw)


def create() -> None:
    head = subprocess.run(["git", "rev-parse", "HEAD"], cwd=ROOT, capture_output=True, text=True, check=True).stdout.strip()
    MIRROR.mkdir(parents=True)
    arc = subprocess.run(["git", "archive", "--format=tar", head], cwd=ROOT, capture_output=True, check=True).stdout
    subprocess.run(["tar", "-x", "-f", "-", "-C", str(MIRROR)], input=arc, check=True)
    (MIRROR / "HEAD_COMMIT").write_text(head + "\n")
    for j in JUNCTIONS:
        dst = MIRROR / j
        if dst.exists():
            # the archive's tracked copy (the shared dir holds the same tracked files, S0 export.md); a fresh extraction,
            # never a junction (create() runs only when the mirror did not exist)
            if dst.is_symlink() or not dst.resolve().is_relative_to(MIRROR.resolve()):
                raise SystemExit(f"{dst} is not inside the fresh export; refusing")
            shutil.rmtree(dst)
        dst.parent.mkdir(parents=True, exist_ok=True)
        run(["cmd", "/c", "mklink", "/J", str(dst), str(ROOT / j)], capture_output=True)
    for p in PATCHES:
        run(["patch", "-p1", "--dry-run", "-i", str(ROOT / p)], cwd=MIRROR)
        run(["patch", "-p1", "-i", str(ROOT / p)], cwd=MIRROR)
    for f in S71_FILES:
        dst = MIRROR / f
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(S71 / f, dst)
    mods = MIRROR / "port" / "src" / "sim" / "modules.inc"
    t = mods.read_text()
    if MODULE_LINE not in t:
        mods.write_text(t.rstrip("\n") + "\n" + MODULE_LINE + "\n")
    (MIRROR / "port" / "src" / "registry.cpp").touch()
    cm = MIRROR / "port" / "CMakeLists.txt"
    t = cm.read_text()
    if CMAKE_LINE not in t:
        cm.write_text(t.rstrip("\n") + "\n\n# S7.2 (mirror only; the integrator applies it from log/roadmap/S7/2_merge.md)\n" + CMAKE_LINE + "\n")


def sync() -> None:
    for d in SYNC_DIRS:
        for src in (ROOT / d).rglob("*"):
            if src.is_file() and "__pycache__" not in src.parts:
                dst = MIRROR / src.relative_to(ROOT)
                dst.parent.mkdir(parents=True, exist_ok=True)
                if not dst.exists() or dst.read_bytes() != src.read_bytes():
                    shutil.copy2(src, dst)
    for rel in SYNC_FILES:
        src = ROOT / rel
        if src.exists():
            dst = MIRROR / rel
            if not dst.exists() or dst.read_bytes() != src.read_bytes():
                shutil.copy2(src, dst)


def build() -> int:
    sys.path.insert(0, str(MIRROR))
    from port.test import build as B  # the mirror's own copy
    cmake = B.cmake_exe()
    bdir = MIRROR / "port" / "build" / "S7_6"
    r = subprocess.run([cmake, "-S", str(MIRROR / "port"), "-B", str(bdir), "-A", "x64"])
    if r.returncode:
        return r.returncode
    return subprocess.run([cmake, "--build", str(bdir), "--config", "Release", "--target", "portsim_c", "simgolf_port",
                           "--", "/m"]).returncode


if __name__ == "__main__":
    if not MIRROR.exists():
        create()
    sync()
    sys.exit(build() if "--build" in sys.argv else 0)
