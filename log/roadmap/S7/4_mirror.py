"""S7.4 mirror (copy of 7_mirror.py, own dirs): port/build/S7_4_head = `git archive 2c05f02a` + the S0 junctions (log/roadmap/S0/export.md, plus shim/build and
re/replay as S7.1 / S7.6 had) + S7's unmerged live core and the S7.7 patches.

On creation only:
  - the patches 7_tick.patch and 7_capi.patch when they exist (the step's output), else S7.1's 1_tick.patch, 1_capi.patch and
    S7.6's 6_capi.patch (the step's starting point), each checked with `patch --dry-run` first;
  - the four live files from --from <mirror under port/build> (default S7_6_h2: S7.6's unfinished rebase, which is HEAD 2c05f02a
    + the same three patches + its live.cpp / test_live.py U-1604 edits; checked by S7.7, 7_notes.md section 2), or from
    log/roadmap/S7/7_live/ when that copy exists;
  - S7.1's modules.inc line, S7.2's CMake line.
Every run copies the shared tree's port/app and the app tests into the mirror (S7.6 left them there; files the mirror edited are
listed in 7_files.txt and are copied back by hand, never synced over).

    ~/bin/py -3.12 log/roadmap/S7/4_mirror.py            # create (once) + sync
    ~/bin/py -3.12 log/roadmap/S7/4_mirror.py --build    # + x64 Release build of portsim_c / simgolf_port (mirror port/build/S7_4)
"""
from __future__ import annotations

import pathlib
import shutil
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
HEAD = "2c05f02a12e800022422ee7c2dd48a3249d3983a"
_ARGS = sys.argv[1:]
_FROM = _ARGS[_ARGS.index("--from") + 1] if "--from" in _ARGS else "S7_6_h2"
MIRROR = ROOT / "port" / "build" / "S7_4_head"
S7DIR = ROOT / "log" / "roadmap" / "S7"
JUNCTIONS = ["original", "log", "port/corpus", "tests/golden", "tests/fixtures", "re/versions", "tools/vc6", "shim/build",
             "re/replay"]
LIVE_FILES = ["port/include/portsim/sim/live.h", "port/src/sim/live.cpp", "port/test/live.py", "port/test/test_live.py"]
SYNC_DIRS = ["port/app"]
SYNC_FILES = ["port/test/test_app_shell.py", "port/test/test_app_input.py", "port/test/test_app_live.py",
              "port/test/app_replay.py", "port/test/test_app_replay.py"]
CMAKE_LINE = "add_subdirectory(app)"
MODULE_LINE = "SG_MODULE(live)"


def run(cmd, **kw):
    print("+", " ".join(str(c) for c in cmd), flush=True)
    return subprocess.run(cmd, check=True, **kw)


def patches() -> list[pathlib.Path]:
    own = [S7DIR / "7_tick.patch", S7DIR / "7_capi.patch"]
    if all(p.exists() for p in own):
        return own
    return [S7DIR / "1_tick.patch", S7DIR / "1_capi.patch", S7DIR / "6_capi.patch"]


def create() -> None:
    MIRROR.mkdir(parents=True)
    arc = subprocess.run(["git", "archive", "--format=tar", HEAD], cwd=ROOT, capture_output=True, check=True).stdout
    subprocess.run(["tar", "-x", "-f", "-", "-C", str(MIRROR)], input=arc, check=True)
    (MIRROR / "HEAD_COMMIT").write_text(HEAD + "\n")
    for j in JUNCTIONS:
        dst = MIRROR / j
        if dst.exists():
            if dst.is_symlink() or not dst.resolve().is_relative_to(MIRROR.resolve()):
                raise SystemExit(f"{dst} is not inside the fresh export; refusing")
            shutil.rmtree(dst)
        dst.parent.mkdir(parents=True, exist_ok=True)
        run(["cmd", "/c", "mklink", "/J", str(dst), str(ROOT / j)], capture_output=True)
    for p in patches():
        run(["patch", "-p1", "--dry-run", "-i", str(p)], cwd=MIRROR)
        run(["patch", "-p1", "-i", str(p)], cwd=MIRROR)
    src_root = S7DIR / "7_live" if (S7DIR / "7_live").exists() else ROOT / "port" / "build" / _FROM
    for f in LIVE_FILES:
        dst = MIRROR / f
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src_root / f, dst)
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
                if not dst.exists():
                    shutil.copy2(src, dst)
                elif dst.read_bytes() != src.read_bytes():
                    shutil.copy2(src, dst)  # S7.4 owns the shared tree's port/app edits: the shared copy wins
                    print("synced from the shared tree:", src.relative_to(ROOT))
    for rel in SYNC_FILES:
        src = ROOT / rel
        if src.exists():
            dst = MIRROR / rel
            if not dst.exists():
                shutil.copy2(src, dst)
            elif dst.read_bytes() != src.read_bytes():
                shutil.copy2(src, dst)
                print("synced from the shared tree:", rel)


def build() -> int:
    sys.path.insert(0, str(MIRROR))
    from port.test import build as B  # the mirror's own copy
    cmake = B.cmake_exe()
    bdir = MIRROR / "port" / "build" / "S7_4"
    r = subprocess.run([cmake, "-S", str(MIRROR / "port"), "-B", str(bdir), "-A", "x64"])
    if r.returncode:
        return r.returncode
    return subprocess.run([cmake, "--build", str(bdir), "--config", "Release", "--target", "portsim_c", "simgolf_port",
                           "--", "/m"]).returncode


def apply_own_patches() -> None:
    """S7.4's patches of files it does not own (applied in this mirror only; 4_merge.md hands them to the integrator)"""
    for p in [S7DIR / "4_window.patch", S7DIR / "4_input_map.patch"]:
        if subprocess.run(["patch", "-p1", "-R", "--dry-run", "-s", "-f", "-i", str(p)], cwd=MIRROR,
                          capture_output=True).returncode == 0:
            continue  # already applied
        run(["patch", "-p1", "-i", str(p)], cwd=MIRROR)


if __name__ == "__main__":
    if not MIRROR.exists():
        create()
    sync()
    apply_own_patches()
    sys.exit(build() if "--build" in sys.argv else 0)
