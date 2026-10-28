"""S3.7 private build and test root.

    ~/bin/py -3.12 log/roadmap/S3/7_mirror.py [--w32] [--no-build] [--base] [--own-only] [--resnap]

Source root port/build/S3_7_src:
  - port/ = a frozen copy of the I6 integrator's port/ (port/build/I6: HEAD b3d61e41 + S6 wave 2 + S3 waves 2 / 3 + S2 wave 5),
    taken once into port/build/S3_7_src/_i6port (sha256 in S3_7_src/I6_SNAPSHOT); every other top-level entry of port/build/I6 is
    a junction (log, original and port/corpus point at the working tree);
  - S3.6 / S3.6b's files (gfx_hud.*, gfx_pixel.*, their tests) as frozen copies from the working tree (_s36 snapshot: S3.6b
    is still editing them), their family lines and 6_edits.py / 6b_edits.py;
  - S3.5's files (gfx_scene.*, test_gfx_scene.py), its 5_merge_edits.py EDITS (minus the families line, done here),
    EDITS_AFTER_S2W5 (S2 wave 5 is in I6) and EDITS_HUD;
  - this step's own files (OWN) and its shared-file edits (7_edits.py), unless --base.
Builds: port/build/S3_head2/port/build/S3_7 (x64), S3_7_w32 (Win32 with portsim_runner32), S3_7_base (--base: no S3.7 file
or edit; the baseline the gates compare with).
"""
import argparse
import hashlib
import importlib.util
import os
import pathlib
import shutil
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
I6 = ROOT / "port" / "build" / "I6"
S3H2 = ROOT / "port" / "build" / "S3_head2"
SRC = ROOT / "port" / "build" / "S3_7_src"
BUILD = {"x64": S3H2 / "port" / "build" / "S3_7", "Win32": S3H2 / "port" / "build" / "S3_7_w32",
         "base": S3H2 / "port" / "build" / "S3_7_base"}
CMAKE = r"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
S3L = ROOT / "log" / "roadmap" / "S3"

S36 = ["src/sim/gfx_hud.cpp", "include/portsim/sim/gfx_hud.h", "test/test_gfx_hud.py", "src/sim/gfx_pixel.cpp",
       "include/portsim/sim/gfx_pixel.h", "test/test_gfx_pixel.py"]
S35 = ["src/sim/gfx_scene.cpp", "include/portsim/sim/gfx_scene.h", "test/test_gfx_scene.py"]
OWN = ["src/sim/gfx_text.cpp", "include/portsim/sim/gfx_text.h", "test/test_gfx_text.py"]
FAMILIES_AFTER = "SG_GFX_FAMILY(dialogs)\n"
FAMILIES_S3 = "SG_GFX_FAMILY(scene)\nSG_GFX_FAMILY(hud)\nSG_GFX_FAMILY(pixel)\n"
FAMILY_S37 = "SG_GFX_FAMILY(text)\n"


def patch(text: str, old: str, new: str, what: str) -> str:
    if "\r\n" in text:
        old, new = old.replace("\n", "\r\n"), new.replace("\n", "\r\n")
    if text.count(old) != 1:
        raise SystemExit(f"{what}: anchor found {text.count(old)} times")
    return text.replace(old, new)


def patch_file(p: pathlib.Path, old: str, new: str, what: str):
    p.write_bytes(patch(p.read_bytes().decode(), old, new, what).encode())


def junction(link: pathlib.Path, target: pathlib.Path):
    if link.exists():
        return
    link.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run(["cmd", "/c", "mklink", "/J", str(link), str(target)], check=True, capture_output=True)


def _load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    m = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(m)
    return m


def tree_hash(d: pathlib.Path) -> str:
    h = hashlib.sha256()
    for p in sorted(d.rglob("*")):
        if p.is_file():
            h.update(str(p.relative_to(d)).encode() + p.read_bytes())
    return h.hexdigest()


def snapshots(resnap: bool):
    snap = ROOT / "port" / "build" / "S3_7_src" / "_i6port"
    s36 = ROOT / "port" / "build" / "S3_7_src" / "_s36"
    if resnap:
        for d in (snap, s36):
            if d.exists():
                shutil.rmtree(d)
    SRC.mkdir(parents=True, exist_ok=True)
    snap.parent.mkdir(parents=True, exist_ok=True)
    if not snap.exists():
        shutil.copytree(I6 / "port", snap, ignore=shutil.ignore_patterns("build", "corpus", "__pycache__"))
    if not s36.exists():
        for rel in S36 + S35:
            (s36 / rel).parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(ROOT / "port" / rel, s36 / rel)
    (snap.parent / "I6_SNAPSHOT").write_text(f"port tree sha256 {tree_hash(snap)}\nHEAD {(I6 / 'HEAD_COMMIT').read_text().strip()}\n"
                                     f"S3.5 / S3.6 files sha256 {tree_hash(s36)}\n")
    return snap, s36


def copy_tree(own: bool, resnap: bool):
    SRC.mkdir(parents=True, exist_ok=True)
    for e in I6.iterdir():
        if e.name in ("port", "log", "original"):
            continue
        if e.is_dir():
            junction(SRC / e.name, e)
        else:
            shutil.copyfile(e, SRC / e.name)
    junction(SRC / "log", ROOT / "log")
    junction(SRC / "original", ROOT / "original")
    snap, s36 = snapshots(resnap)
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
    shutil.copytree(s36, dst, dirs_exist_ok=True)
    fam = FAMILIES_S3 + (FAMILY_S37 if own else "")
    patch_file(dst / "src/sim/gfx_families.inc", FAMILIES_AFTER, FAMILIES_AFTER + fam, "gfx_families.inc")
    for path in (S3L / "6_edits.py", S3L / "6b_edits.py"):
        env = {}
        exec(path.read_text(encoding="utf-8"), env)
        for rel, old, new in env["EDITS"]:
            patch_file(dst / rel, old, new, f"{path.name}: {rel}")
    me = _load("s35_merge", S3L / "5_merge_edits.py")
    for rel, old, new in me.EDITS + me.EDITS_AFTER_S2W5 + me.EDITS_HUD:
        if rel.endswith("gfx_families.inc"):
            continue
        patch_file(SRC / rel, old, new, f"5_merge_edits: {rel}")
    if own:
        for rel in OWN:
            if (ROOT / "port" / rel).exists():
                shutil.copyfile(ROOT / "port" / rel, dst / rel)
        e7 = S3L / "7_edits.py"
        if e7.exists():
            env = {}
            exec(e7.read_text(encoding="utf-8"), env)
            for rel, old, new in env["EDITS"]:
                patch_file(dst / rel, old, new, f"7_edits: {rel}")
    (dst / "src" / "registry.cpp").touch()
    (dst / "src" / "sim" / "gfx.cpp").touch()


def build(arch: str, key: str):
    b = BUILD[key]
    subprocess.run([CMAKE, "-S", str(SRC / "port"), "-B", str(b), "-A", arch], check=True)
    subprocess.run([CMAKE, "--build", str(b), "--config", "Release", "--target", "portsim_c", "--", "/m"], check=True)
    if arch == "Win32":
        subprocess.run([CMAKE, "--build", str(b), "--config", "Release", "--target", "portsim_runner32", "--", "/m"],
                       check=True)
    return b / "Release" / "portsim_c.dll"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--w32", action="store_true")
    ap.add_argument("--no-build", action="store_true")
    ap.add_argument("--base", action="store_true")
    ap.add_argument("--resnap", action="store_true", help="take the I6 and S3.5 / S3.6 snapshots again")
    ap.add_argument("--own-only", action="store_true", help="recopy only this step's files into the existing tree")
    ap.add_argument("--src", default="S3_7_src", help="source / test root under port/build (a run reads it: use another one)")
    a = ap.parse_args()
    global SRC
    SRC = ROOT / "port" / "build" / a.src
    if a.own_only:
        for rel in OWN:
            if (ROOT / "port" / rel).exists():
                shutil.copyfile(ROOT / "port" / rel, SRC / "port" / rel)
        (SRC / "port" / "src" / "sim" / "gfx.cpp").touch()
    else:
        copy_tree(not a.base, a.resnap)
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
