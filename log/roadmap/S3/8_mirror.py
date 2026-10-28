"""S3.8 private build: S3.6b's source copy port/build/S3_6b_src (the I6 integrator's tree = HEAD b3d61e41 + S6 wave 2 + S3 waves 2 / 3
+ S2 wave 5, plus S3 wave 4 = S3.6 / S3.6b with their shared-file edits applied) plus S3.5's own files and its shared-file edits
(log/roadmap/S3/5_merge_edits.py: EDITS, EDITS_HUD, EDITS_AFTER_S2W5; each skipped when already present), plus this step's own files and
the shared-file edits of log/roadmap/S3/8_edits.py (8_merge.md).

    ~/bin/py -3.12 log/roadmap/S3/8_mirror.py [--w32] [--no-build] [--base] [--own-only]

Source root: port/build/S3_8_src (port/ is a real copy; every other top-level entry of S3_6b_src is a junction to the same target;
port/corpus is a junction to the shared corpus; port/build/Win32 a junction to this step's Win32 build). Builds:
port/build/S3_head2/port/build/S3_8 (x64), S3_8_w32 (Win32); --base builds the tree without this step's files and edits into S3_8_base.
"""
import argparse
import os
import pathlib
import shutil
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
BASE = ROOT / "port" / "build" / "S3_6b_src"
S3H2 = ROOT / "port" / "build" / "S3_head2"
SRC = ROOT / "port" / "build" / "S3_8_src"
BUILD = {"x64": S3H2 / "port" / "build" / "S3_8", "Win32": S3H2 / "port" / "build" / "S3_8_w32",
         "base": S3H2 / "port" / "build" / "S3_8_base"}
CMAKE = r"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"

S35_OWN = ["port/src/sim/gfx_scene.cpp", "port/include/portsim/sim/gfx_scene.h", "port/test/test_gfx_scene.py"]
OWN = ["port/src/sim/terrain_overlay.cpp", "port/include/portsim/sim/terrain_overlay.h", "port/test/test_terrain_overlay.py"]
S35_EDITS = ROOT / "log" / "roadmap" / "S3" / "5_merge_edits.py"
EDITS_8 = ROOT / "log" / "roadmap" / "S3" / "8_edits.py"


def patch(text: str, old: str, new: str, what: str) -> str:
    if "\r\n" in text:
        old, new = old.replace("\n", "\r\n"), new.replace("\n", "\r\n")
    if text.count(old) != 1:
        raise SystemExit(f"{what}: anchor found {text.count(old)} times")
    return text.replace(old, new)


def apply(rel: str, old: str, new: str, what: str, skip_present: bool = True):
    p = SRC / rel
    b = p.read_bytes().decode()
    nn = new.replace("\n", "\r\n") if "\r\n" in b else new
    if skip_present and nn in b:
        return
    p.write_bytes(patch(b, old, new, what).encode())


def load(path: pathlib.Path) -> dict:
    env: dict = {}
    exec(path.read_text(encoding="utf-8"), env)
    return env


def junction(link: pathlib.Path, target: pathlib.Path):
    if link.exists():
        return
    link.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run(["cmd", "/c", "mklink", "/J", str(link), str(target)], check=True, capture_output=True)


def copy_tree(own: bool):
    SRC.mkdir(parents=True, exist_ok=True)
    for e in BASE.iterdir():
        if e.name in ("port", "I6_SNAPSHOT"):
            continue
        if e.is_dir():
            junction(SRC / e.name, pathlib.Path(os.path.realpath(e)))
        else:
            shutil.copyfile(e, SRC / e.name)
    dst = SRC / "port"
    if dst.exists():
        for e in dst.iterdir():
            if e.is_junction() or e.is_symlink():
                os.rmdir(e)
            elif e.is_dir():
                shutil.rmtree(e)
            else:
                e.unlink()
    shutil.copytree(BASE / "port", dst, ignore=shutil.ignore_patterns("build", "corpus", "__pycache__"), dirs_exist_ok=True)
    junction(dst / "corpus", ROOT / "port" / "corpus")
    BUILD["Win32"].mkdir(parents=True, exist_ok=True)
    junction(dst / "build" / "Win32", BUILD["Win32"])
    # S3.5 (accepted, not yet merged): its files and shared-file edits
    for rel in S35_OWN:
        shutil.copyfile(ROOT / rel, SRC / rel)
    me = load(S35_EDITS)
    for rel, old, new in me["EDITS"] + me.get("EDITS_HUD", []) + me.get("EDITS_AFTER_S2W5", []):
        apply(rel, old, new, "S3.5 " + rel)
    if own:
        copy_own()
    (dst / "src" / "registry.cpp").touch()
    (dst / "src" / "sim" / "gfx.cpp").touch()


def copy_own():
    for rel in OWN:
        if (ROOT / rel).exists():
            (SRC / rel).parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(ROOT / rel, SRC / rel)
    if EDITS_8.exists():
        for rel, old, new in load(EDITS_8)["EDITS"]:
            apply(rel, old, new, "S3.8 " + rel)


def build(arch: str, key: str):
    b = BUILD[key]
    subprocess.run([CMAKE, "-S", str(SRC / "port"), "-B", str(b), "-A", arch], check=True)
    subprocess.run([CMAKE, "--build", str(b), "--config", "Release", "--target", "portsim_c", "--", "/m"], check=True)
    if arch == "Win32":
        subprocess.run([CMAKE, "--build", str(b), "--config", "Release", "--target", "portsim_runner32", "--", "/m"], check=True)
    return b / "Release" / "portsim_c.dll"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--w32", action="store_true")
    ap.add_argument("--no-build", action="store_true")
    ap.add_argument("--base", action="store_true")
    ap.add_argument("--own-only", action="store_true", help="refresh this step's own files and edits in the existing tree")
    a = ap.parse_args()
    if a.own_only:
        copy_own()
        (SRC / "port" / "src" / "registry.cpp").touch()
        (SRC / "port" / "src" / "sim" / "gfx.cpp").touch()
    else:
        copy_tree(not a.base)
    if a.no_build:
        return 0
    if a.base:
        print("SG_PORTSIM_DLL=" + str(build("x64", "base")))
        return 0
    print("SG_PORTSIM_DLL=" + str(build("x64", "x64")))
    if a.w32:
        print("Win32:", build("Win32", "Win32"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
