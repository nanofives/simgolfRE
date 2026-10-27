"""S7.8 mirror: port/build/S7_8_head = S7.5's mirror recipe (log/roadmap/S7/5_mirror.py: `git archive b3d61e41` + S0 junctions
+ S3.2's files and edits + S7 wave 1), then port/app/* and the app tests copied from S7.5's own mirror port/build/S7_5_head (S7.5
accepted: wave 2), so this mirror starts at exactly the state S7.5's gate measured. This step's edits then live in the mirror
(8_files.txt lists them); the create step never runs again on an existing mirror.

    ~/bin/py -3.12 log/roadmap/S7/8_mirror.py                 # create (once)
    ~/bin/py -3.12 log/roadmap/S7/8_mirror.py --build [--w32] # + Release builds: port/build/S7_8 (x64), port/build/S7_8_w32
"""
from __future__ import annotations

import importlib.util
import pathlib
import shutil
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
S7DIR = ROOT / "log" / "roadmap" / "S7"
S75 = ROOT / "port" / "build" / "S7_5_head"
MIRROR = ROOT / "port" / "build" / "S7_8_head"
EXTRA = ["port/test/test_app_ingame.py", "port/test/test_app_live.py", "port/test/test_app_shell.py",
         "port/test/test_app_input.py", "port/test/app_replay.py", "port/test/test_app_replay.py",
         "port/include/portsim/sim/live.h", "port/src/sim/live.cpp", "port/test/live.py", "port/test/test_live.py"]


def _load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    m = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(m)
    return m


def create() -> None:
    m5 = _load("s75_mirror", S7DIR / "5_mirror.py")
    m5.MIRROR = MIRROR
    m5.create()
    for src in (S75 / "port" / "app").rglob("*"):
        if src.is_file() and "__pycache__" not in src.parts:
            dst = MIRROR / src.relative_to(S75)
            dst.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(src, dst)
    for rel in EXTRA:
        shutil.copy2(S75 / rel, MIRROR / rel)
    # S3.2's files as S7.5's mirror holds them (the shared tree may have moved on since: S3 siblings edit it)
    for rel in m5.s3_files():
        if (S75 / rel).is_file():
            shutil.copy2(S75 / rel, MIRROR / rel)
    (MIRROR / "port" / "src" / "registry.cpp").touch()


def build(arch: str) -> int:
    m5 = _load("s75_mirror", S7DIR / "5_mirror.py")
    bdir = MIRROR / "port" / "build" / ("S7_8" if arch == "x64" else "S7_8_w32")
    r = subprocess.run([m5.CMAKE, "-S", str(MIRROR / "port"), "-B", str(bdir), "-A", arch])
    if r.returncode:
        return r.returncode
    targets = ["portsim_c", "simgolf_port"] if arch == "x64" else ["portsim_c", "portsim_runner32"]
    return subprocess.run([m5.CMAKE, "--build", str(bdir), "--config", "Release", "--target", *targets, "--", "/m"]).returncode


if __name__ == "__main__":
    if not MIRROR.exists():
        create()
    rc = 0
    if "--build" in sys.argv:
        rc = build("x64")
        if rc == 0 and "--w32" in sys.argv:
            rc = build("Win32")
    sys.exit(rc)
