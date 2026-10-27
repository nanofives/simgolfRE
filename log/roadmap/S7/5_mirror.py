"""S7.5 mirror: port/build/S7_5_head = `git archive b3d61e41` (S2 waves 1-4 + S3 wave 1)
  + the S0 junctions (as S7.7's mirror: original, log, port/corpus, tests/golden, tests/fixtures, re/versions, tools/vc6, shim/build,
    re/replay)
  + S3.2's accepted files (log/roadmap/S3/files_w3.txt, port/ and re/ rows, from the shared tree, which equals S3.2's own root
    port/build/S3_2_root for every port/ file) and its shared-file edits (log/roadmap/S3/2_merge_edits.py EDITS, exact-match)
  + S7 wave 1 (log/roadmap/S7/MERGE.md): the four live files from log/roadmap/S7/7_live, 7_tick.patch, 7_capi.patch (runner32 op 40;
    S3.2's ops 28..32 stay), SG_MODULE(live), add_subdirectory(app), the shared tree's port/app (S7.4's patches applied by the lead)
    and the app tests.

    ~/bin/py -3.12 log/roadmap/S7/5_mirror.py                 # create (once)
    ~/bin/py -3.12 log/roadmap/S7/5_mirror.py --build [--w32] # + Release builds: port/build/S7_5 (x64), port/build/Win32 (mirror)
    ~/bin/py -3.12 log/roadmap/S7/5_mirror.py --status        # files of port/app / app tests that differ from the shared tree

This step's own edits live in the mirror (port/app/*, port/test/test_app_ingame.py); 5_files.txt lists them. The create step
never runs again on an existing mirror, so the mirror's edits are never overwritten.
"""
from __future__ import annotations

import importlib.util
import pathlib
import shutil
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
HEAD = "b3d61e41"
MIRROR = ROOT / "port" / "build" / "S7_5_head"
S7DIR = ROOT / "log" / "roadmap" / "S7"
S3DIR = ROOT / "log" / "roadmap" / "S3"
JUNCTIONS = ["original", "log", "port/corpus", "tests/golden", "tests/fixtures", "re/versions", "tools/vc6", "shim/build",
             "re/replay"]
LIVE_FILES = ["port/include/portsim/sim/live.h", "port/src/sim/live.cpp", "port/test/live.py", "port/test/test_live.py"]
APP_FILES = ["port/test/test_app_shell.py", "port/test/test_app_input.py", "port/test/test_app_live.py",
             "port/test/app_replay.py", "port/test/test_app_replay.py"]
CMAKE_LINE = "add_subdirectory(app)"
MODULE_LINE = "SG_MODULE(live)"
CMAKE = (r"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake"
         r"\CMake\bin\cmake.exe")


def run(cmd, **kw):
    print("+", " ".join(str(c) for c in cmd), flush=True)
    return subprocess.run(cmd, check=True, **kw)


def _load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    m = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(m)
    return m


def patch(text: str, old: str, new: str, what: str) -> str:
    if "\r\n" in text:
        old, new = old.replace("\n", "\r\n"), new.replace("\n", "\r\n")
    if text.count(old) != 1:
        raise SystemExit(f"{what}: anchor found {text.count(old)} times")
    return text.replace(old, new)


def s3_files() -> list[str]:
    rows = [ln.strip() for ln in (S3DIR / "files_w3.txt").read_text().splitlines()]
    return [r for r in rows if r.startswith(("port/", "re/tools/", "re/frida/"))]


def create() -> None:
    MIRROR.mkdir(parents=True)
    arc = subprocess.run(["git", "archive", "--format=tar", HEAD], cwd=ROOT, capture_output=True, check=True).stdout
    subprocess.run(["tar", "-x", "-f", "-", "-C", str(MIRROR)], input=arc, check=True)
    full = subprocess.run(["git", "rev-parse", HEAD], cwd=ROOT, capture_output=True, text=True, check=True).stdout.strip()
    (MIRROR / "HEAD_COMMIT").write_text(full + "\n")
    for j in JUNCTIONS:
        dst = MIRROR / j
        if dst.exists():
            if dst.is_symlink() or not dst.resolve().is_relative_to(MIRROR.resolve()):
                raise SystemExit(f"{dst} is not inside the fresh export; refusing")
            shutil.rmtree(dst)
        dst.parent.mkdir(parents=True, exist_ok=True)
        run(["cmd", "/c", "mklink", "/J", str(dst), str(ROOT / j)], capture_output=True)
    # S3.2 (wave 3): owned files, then the shared-file edits
    for rel in s3_files():
        (MIRROR / rel).parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(ROOT / rel, MIRROR / rel)
    me = _load("s32_merge", S3DIR / "2_merge_edits.py")
    for rel, old, new in me.EDITS:
        p = MIRROR / rel
        p.write_bytes(patch(p.read_bytes().decode(), old, new, rel).encode())
    # S7 wave 1
    for f in LIVE_FILES:
        (MIRROR / f).parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(S7DIR / "7_live" / f, MIRROR / f)
    for p in (S7DIR / "7_tick.patch", S7DIR / "7_capi.patch"):
        run(["patch", "-p1", "--dry-run", "-i", str(p)], cwd=MIRROR)
        run(["patch", "-p1", "-i", str(p)], cwd=MIRROR)
    mods = MIRROR / "port" / "src" / "sim" / "modules.inc"
    t = mods.read_text()
    if MODULE_LINE not in t:
        mods.write_text(t.rstrip("\n") + "\n" + MODULE_LINE + "\n")
    cm = MIRROR / "port" / "CMakeLists.txt"
    t = cm.read_text()
    if CMAKE_LINE not in t:
        cm.write_text(t.rstrip("\n") + "\n\n# Roadmap S7: the Windows host (mirror only; S7 MERGE.md 1.4)\n" + CMAKE_LINE + "\n")
    for src in (ROOT / "port" / "app").rglob("*"):
        if src.is_file() and "__pycache__" not in src.parts:
            dst = MIRROR / src.relative_to(ROOT)
            dst.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(src, dst)
    for rel in APP_FILES:
        shutil.copy2(ROOT / rel, MIRROR / rel)
    (MIRROR / "port" / "src" / "registry.cpp").touch()


def status() -> None:
    for src in list((ROOT / "port" / "app").rglob("*")) + [ROOT / r for r in APP_FILES]:
        if src.is_file() and "__pycache__" not in src.parts:
            dst = MIRROR / src.relative_to(ROOT)
            if not dst.exists() or dst.read_bytes() != src.read_bytes():
                print("differs from the shared tree:", src.relative_to(ROOT))


def build(arch: str) -> int:
    bdir = MIRROR / "port" / "build" / ("S7_5" if arch == "x64" else "Win32")
    r = subprocess.run([CMAKE, "-S", str(MIRROR / "port"), "-B", str(bdir), "-A", arch])
    if r.returncode:
        return r.returncode
    targets = ["portsim_c", "simgolf_port"] if arch == "x64" else ["portsim_c", "portsim_runner32"]
    return subprocess.run([CMAKE, "--build", str(bdir), "--config", "Release", "--target", *targets, "--", "/m"]).returncode


if __name__ == "__main__":
    if not MIRROR.exists():
        create()
    if "--status" in sys.argv:
        status()
    rc = 0
    if "--build" in sys.argv:
        rc = build("x64")
        if rc == 0 and "--w32" in sys.argv:
            rc = build("Win32")
    sys.exit(rc)
