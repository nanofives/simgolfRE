"""S3.5b private build: S3.6b's source root port/build/S3_6b_src (I6's tree = HEAD b3d61e41 + S6 wave 2 + S3 waves 2 / 3 + S2 wave 5,
plus S3.6 / S3.6b's own files and edits = wave 4) plus S3.5's own files and the shared-file edits of 5_merge_edits.py
(EDITS, EDITS_AFTER_S2W5 where S2 wave 5's files are already in the tree, EDITS_HUD), each applied with an exact-match check.

    ~/bin/py -3.12 log/roadmap/S3/5b_mirror.py [--w32] [--no-build] [--base] [--own-only]

Source root port/build/S3_5b_src; builds port/build/S3_head2/port/build/S3_5b (x64), S3_5b_w32 (Win32), S3_5b_base (--base: wave 4
without S3.5, the baseline for comparisons).
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
SRC = ROOT / "port" / "build" / "S3_5b_src"
BUILD = {"x64": S3H2 / "port" / "build" / "S3_5b", "Win32": S3H2 / "port" / "build" / "S3_5b_w32",
         "base": S3H2 / "port" / "build" / "S3_5b_base"}
CMAKE = r"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
OWN = ["port/src/sim/gfx_scene.cpp", "port/include/portsim/sim/gfx_scene.h", "port/test/test_gfx_scene.py"]
EDITS_FILE = ROOT / "log" / "roadmap" / "S3" / "5_merge_edits.py"


def patch(text, old, new, what):
    if "\r\n" in text:
        old, new = old.replace("\n", "\r\n"), new.replace("\n", "\r\n")
    if text.count(old) != 1:
        raise SystemExit(f"{what}: anchor found {text.count(old)} times")
    return text.replace(old, new)


def junction(link, target):
    if link.exists():
        return
    subprocess.run(["cmd", "/c", "mklink", "/J", str(link), str(target)], check=True, capture_output=True)


def clear_port(dst):
    for e in dst.iterdir():
        if e.is_junction() or e.is_symlink():
            os.rmdir(e)
        elif e.is_dir():
            shutil.rmtree(e)
        else:
            e.unlink()


def copy_tree(own):
    SRC.mkdir(parents=True, exist_ok=True)
    for e in BASE.iterdir():
        if e.name in ("port", "_i6port"):
            continue
        if e.is_dir():
            # junctions of the base point at the same targets
            tgt = os.path.realpath(e)
            junction(SRC / e.name, pathlib.Path(tgt))
        else:
            shutil.copyfile(e, SRC / e.name)
    dst = SRC / "port"
    if dst.exists():
        clear_port(dst)
    dst.mkdir(exist_ok=True)
    shutil.copytree(BASE / "port", dst, dirs_exist_ok=True, ignore=shutil.ignore_patterns("build", "corpus", "__pycache__"))
    junction(dst / "corpus", ROOT / "port" / "corpus")
    if not own:
        return
    for rel in OWN:
        shutil.copyfile(ROOT / rel, SRC / rel)
    env = {}
    exec(EDITS_FILE.read_text(encoding="utf-8"), env)
    for name in ("EDITS", "EDITS_AFTER_S2W5", "EDITS_HUD"):
        for rel, old, new in env.get(name, []):
            p = SRC / rel
            p.write_bytes(patch(p.read_bytes().decode(), old, new, f"{name}: {rel}").encode())
    apply_5b()
    (dst / "src" / "registry.cpp").touch()
    (dst / "src" / "sim" / "gfx.cpp").touch()


def apply_5b():
    """this step's own shared-file edits (5b_edits.py), skipped where already applied"""
    env = {}
    exec((ROOT / "log" / "roadmap" / "S3" / "5b_edits.py").read_text(encoding="utf-8"), env)
    for rel, old, new in env["EDITS"]:
        p = SRC / rel
        b = p.read_bytes().decode()
        crlf = chr(13) + chr(10)
        nn = new.replace(chr(10), crlf) if crlf in b else new
        if nn in b:
            continue
        p.write_bytes(patch(b, old, new, f"5b_edits: {rel}").encode())


def build(arch, key):
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
    ap.add_argument("--own-only", action="store_true")
    a = ap.parse_args()
    if a.own_only:
        for rel in OWN:
            shutil.copyfile(ROOT / rel, SRC / rel)
        apply_5b()
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
