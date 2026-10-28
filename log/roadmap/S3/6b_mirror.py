"""S3.6b private build: the I6 integrator's mirror port/build/I6 (git archive of HEAD b3d61e41 = S2 waves 1-4 + S3 wave 1, plus
S6 wave 2, S3 waves 2 / 3 and S2 wave 5 applied by I6) plus S3.6's / S3.6b's own files and the shared-file edits of
log/roadmap/S3/6_edits.py (exact-match anchors, CRLF-aware) and of this step (EDITS_6B below, 6b_merge.md).

    ~/bin/py -3.12 log/roadmap/S3/6b_mirror.py [--w32] [--no-build] [--base] [--recopy]

Source root: port/build/S3_6b_src (port/ is a copy of port/build/I6/port without build / corpus; every other top-level entry
of port/build/I6 is a junction to it; port/corpus is a junction to the shared corpus). Builds: port/build/S3_head2/port/build/
S3_6b (x64) and S3_6b_w32 (Win32); --base builds the same tree without this step's files and edits into S3_6b_base (the
baseline the gates compare with). The I6 tree is snapshotted once (--recopy takes it again).
"""
import argparse
import hashlib
import os
import pathlib
import shutil
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
I6 = ROOT / "port" / "build" / "I6"
S3H2 = ROOT / "port" / "build" / "S3_head2"
SRC = ROOT / "port" / "build" / "S3_6b_src"
BUILD = {"x64": S3H2 / "port" / "build" / "S3_6b", "Win32": S3H2 / "port" / "build" / "S3_6b_w32",
         "base": S3H2 / "port" / "build" / "S3_6b_base"}
CMAKE = r"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"

OWN = ["src/sim/gfx_hud.cpp", "include/portsim/sim/gfx_hud.h", "test/test_gfx_hud.py", "src/sim/gfx_pixel.cpp",
       "include/portsim/sim/gfx_pixel.h", "test/test_gfx_pixel.py"]
FAM_ANCHOR = "SG_GFX_FAMILY(routing)\n"
FAM_NEW = "SG_GFX_FAMILY(routing)\nSG_GFX_FAMILY(hud)\nSG_GFX_FAMILY(pixel)\n"
EDITS_6 = ROOT / "log" / "roadmap" / "S3" / "6_edits.py"
EDITS_6B = ROOT / "log" / "roadmap" / "S3" / "6b_edits.py"


def patch(text: str, old: str, new: str, what: str) -> str:
    crlf = "\r\n" in text
    if crlf:
        old, new = old.replace("\n", "\r\n"), new.replace("\n", "\r\n")
    if text.count(old) != 1:
        raise SystemExit(f"{what}: anchor found {text.count(old)} times")
    return text.replace(old, new)


def patch_file(p: pathlib.Path, old: str, new: str, what: str):
    t = p.read_bytes().decode()
    p.write_bytes(patch(t, old, new, what).encode())


def junction(link: pathlib.Path, target: pathlib.Path):
    if link.exists():
        return
    subprocess.run(["cmd", "/c", "mklink", "/J", str(link), str(target)], check=True, capture_output=True)


def snapshot():
    """port/build/S3_6b_src/_i6port: a frozen copy of I6's port/ (the integrator may still edit its tree)."""
    snap = SRC / "_i6port"
    if snap.exists():
        return snap
    shutil.copytree(I6 / "port", snap, ignore=shutil.ignore_patterns("build", "corpus", "__pycache__"))
    h = hashlib.sha256()
    for p in sorted(snap.rglob("*")):
        if p.is_file():
            h.update(str(p.relative_to(snap)).encode() + p.read_bytes())
    (SRC / "I6_SNAPSHOT").write_text(f"port tree sha256 {h.hexdigest()}\nHEAD {(I6 / 'HEAD_COMMIT').read_text().strip()}\n")
    return snap


def copy_tree(own: bool):
    SRC.mkdir(parents=True, exist_ok=True)
    for e in I6.iterdir():
        if e.name == "port":
            continue
        if e.is_dir():
            junction(SRC / e.name, e)
        else:
            shutil.copyfile(e, SRC / e.name)
    snap = snapshot()
    dst = SRC / "port"
    if dst.exists():
        for e in dst.iterdir():
            if e.is_junction() or e.is_symlink():
                os.rmdir(e)
            elif e.is_dir():
                shutil.rmtree(e)
            else:
                e.unlink()
    dst.mkdir(exist_ok=True)
    shutil.copytree(snap, dst, dirs_exist_ok=True)
    junction(dst / "corpus", ROOT / "port" / "corpus")
    if not own:
        return
    for rel in OWN:
        shutil.copyfile(ROOT / "port" / rel, dst / rel)
    patch_file(dst / "src" / "sim" / "gfx_families.inc", FAM_ANCHOR, FAM_NEW, "gfx_families.inc")
    for path in (EDITS_6, EDITS_6B):
        if path.exists():
            env = {}
            exec(path.read_text(encoding="utf-8"), env)
            for rel, old, new in env["EDITS"]:
                patch_file(dst / rel, old, new, f"{path.name}: {rel}")
    (dst / "src" / "registry.cpp").touch()
    (dst / "src" / "sim" / "gfx.cpp").touch()


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
    ap.add_argument("--recopy", action="store_true")
    ap.add_argument("--own-only", action="store_true", help="copy this step's own files into the existing tree, then build")
    a = ap.parse_args()
    if a.recopy and (SRC / "_i6port").exists():
        shutil.rmtree(SRC / "_i6port")
    if a.own_only:
        for rel in OWN:
            shutil.copyfile(ROOT / "port" / rel, SRC / "port" / rel)
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
