"""S7.12 mirror: port/build/S7_w4_root = `git archive 37e58e96 port re/model/include` + S7.10's / S7.11's / S7.12's owned files.

As 11_mirror.py, plus the junctions the lead added to S7_11_root by hand (every re/* directory but re/model, every re/model entry
but include, scripts, extracted, log, original, tests/golden, port/corpus) and copies of re's top-level files and pytest.ini, so the
app tests that read re/analysis, re/port (defect_profiles.tsv) or re/roadmap run inside the mirror. re/port is a junction: the
S7.12 rows of defect_profiles.tsv / defect_decisions.tsv are read from the shared tree.

    ~/bin/py -3.12 log/roadmap/S7/12_mirror.py                    # create (once) + sync the owned files
    ~/bin/py -3.12 log/roadmap/S7/12_mirror.py --build x64|w32|both [--target <t> ...]
"""
from __future__ import annotations

import pathlib
import shutil
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
MIRROR = ROOT / "port" / "build" / "S7_w4_root"
BASE = "c7412150"  # HEAD after S9 + P1; audio_exe.cpp rebased by a 3-way merge (log/roadmap/S7/w4_rebase)
I19 = ROOT / "port" / "build" / "I19"
# rebased onto I19 by a 3-way merge (log/roadmap/S7/w4_rebase); every other owned file comes from the shared tree
REBASED = {"port/app/main.cpp": ROOT / "log" / "roadmap" / "S7" / "w4_rebase" / "main_merged.cpp"}
JUNCTIONS = ["original", "extracted", "log", "scripts", "port/corpus", "tests/golden"]
OWNED = ["port/app/audio_out.h", "port/app/audio_out.cpp", "port/app/ds_sink.h", "port/app/ds_sink.cpp",
         "port/app/tools/audio_check.cpp", "port/app/main.cpp", "port/app/CMakeLists.txt", "port/test/test_app_audio.py",
         "port/src/sim/audio_exe.cpp", "port/include/portsim/sim/audio_exe.h",
         "port/test/app_replay.py", "port/test/test_defects_golfsfx.py"]  # S7.12's on top of S7.10 / S7.11's
VS_CMAKE = pathlib.Path(r"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions"
                        r"\Microsoft\CMake\CMake\bin\cmake.exe")
TARGETS = ["portsim_c", "simgolf_port", "simgolf_audio_check"]


def junction(dst: pathlib.Path, src: pathlib.Path) -> None:
    if dst.exists():
        shutil.rmtree(dst)
    dst.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run(["cmd", "/c", "mklink", "/J", str(dst), str(src)], check=True, capture_output=True)


def create() -> None:
    MIRROR.mkdir(parents=True)
    # the I19 tree (c7412150 + S3 wave 7: w7_assemble.py --root + 20_assemble.py --only-20), read only: port/ without its
    # build / corpus / caches, and re/model/include
    def skip(d, names):
        top = pathlib.Path(d) == I19 / "port"
        return [x for x in names if x == "__pycache__" or (top and x in ("build", "corpus"))]
    for rel in ("port", "re/model/include"):
        shutil.copytree(I19 / rel, MIRROR / rel, ignore=skip)
    (MIRROR / "HEAD_COMMIT").write_text(BASE + " + I19 (S3 wave 7)\n")
    for j in JUNCTIONS:
        junction(MIRROR / j, ROOT / j)
    for e in (ROOT / "re").iterdir():
        if e.is_dir() and e.name != "model":
            junction(MIRROR / "re" / e.name, e)
        elif e.is_file():
            shutil.copy(e, MIRROR / "re" / e.name)
    for e in (ROOT / "re" / "model").iterdir():
        if e.name != "include":
            if e.is_dir():
                junction(MIRROR / "re" / "model" / e.name, e)
            else:
                shutil.copy(e, MIRROR / "re" / "model" / e.name)
    shutil.copy(ROOT / "pytest.ini", MIRROR / "pytest.ini")


def sync() -> None:
    for rel in OWNED:
        src, dst = REBASED.get(rel, ROOT / rel), MIRROR / rel
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
