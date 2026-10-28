"""S3.4 private build: the S3 mirror's port/ (port/build/S3_head = git archive of c6d7d78b) plus this step's own files plus the
one shared-file line of log/roadmap/S3/4_merge.md section 1 (`SG_MODULE(flic)` in port/src/sim/modules.inc, applied to the
source copy only; registry.cpp touched so MSBuild rebuilds it). The mirror gets only this step's own non-C++ files (the test,
the recorder): flic.cpp there without its modules.inc line would fail the mirror's test_every_module_file_is_listed.

    ~/bin/py -3.12 log/roadmap/S3/4_mirror.py [--w32] [--no-build]

Source copy: port/build/S3_4_src/port (re/ beside it is a junction to the mirror's re/, for the model header). Builds:
port/build/S3_head/port/build/S3_4 (x64) and S3_4_w32 (Win32). Prints SG_PORTSIM_DLL for the tests:
    cd port/build/S3_head && SG_PORTSIM_DLL=<dll> ~/bin/py -3.12 -m pytest port/test/test_flic.py
"""
import argparse
import pathlib
import shutil
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
MIRROR = ROOT / "port" / "build" / "S3_head"
SRC = ROOT / "port" / "build" / "S3_4_src"
BUILD = {"x64": MIRROR / "port" / "build" / "S3_4", "Win32": MIRROR / "port" / "build" / "S3_4_w32",
         "gfx": MIRROR / "port" / "build" / "S3_4_gfx"}
# --gfx: a third build whose source copy also takes the working tree's uncommitted gfx body interface (roadmap S2 wave 1), so
# flic::sheet_to_gfx's hook-up test runs; it is never copied into the mirror
GFX_WAVE1 = ["port/src/sim/gfx.cpp", "port/src/sim/gfx_body.h", "port/src/sim/gfx_families.inc", "port/src/sim/gfx_selftest.cpp",
             "port/include/portsim/sim/gfx.h"]
OWN = ["port/src/sim/flic.cpp", "port/include/portsim/sim/flic.h", "port/test/test_flic.py", "re/tools/flic_cells_rec.py",
       "re/frida/js/flic_cells.js"]
CMAKE = r"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"


def copy_tree(gfx: bool = False):
    dst = (SRC.parent / (SRC.name + "_gfx") if gfx else SRC) / "port"
    if dst.exists():
        shutil.rmtree(dst)
    shutil.copytree(MIRROR / "port", dst, ignore=shutil.ignore_patterns("build", "corpus", "__pycache__"))
    if not (dst.parent / "re").exists():
        subprocess.run(["cmd", "/c", "mklink", "/J", str(dst.parent / "re"), str(MIRROR / "re")], check=True, capture_output=True)
    for rel in OWN:
        src = ROOT / rel
        if not src.exists():
            continue
        if rel.startswith("port/"):
            shutil.copyfile(src, dst.parent / rel)
    for rel in GFX_WAVE1 if gfx else []:
        shutil.copyfile(ROOT / rel, dst.parent / rel)
    mods = dst / "src" / "sim" / "modules.inc"
    t = mods.read_bytes().decode()
    nl = "\r\n" if "\r\n" in t else "\n"
    if "SG_MODULE(flic)" not in t:
        t = t.rstrip("\r\n") + nl + "SG_MODULE(flic)" + nl
        mods.write_bytes(t.encode())
    (dst / "src" / "registry.cpp").touch()


def build(arch: str):
    b = BUILD[arch]
    src = SRC.parent / (SRC.name + "_gfx") if arch == "gfx" else SRC
    subprocess.run([CMAKE, "-S", str(src / "port"), "-B", str(b), "-A", "x64" if arch == "gfx" else arch], check=True)
    subprocess.run([CMAKE, "--build", str(b), "--config", "Release", "--target", "portsim_c", "--", "/m"], check=True)
    if arch == "Win32":
        subprocess.run([CMAKE, "--build", str(b), "--config", "Release", "--target", "portsim_runner32", "--", "/m"], check=True)
    return b / "Release" / "portsim_c.dll"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--w32", action="store_true")
    ap.add_argument("--no-build", action="store_true")
    ap.add_argument("--gfx", action="store_true", help="only the gfx-plumbing build (S3_4_gfx)")
    a = ap.parse_args()
    if a.gfx:
        copy_tree(gfx=True)
        print("SG_PORTSIM_DLL=" + str(build("gfx")))
        return 0
    copy_tree()
    if a.no_build:
        return 0
    print("SG_PORTSIM_DLL=" + str(build("x64")))
    for rel in OWN:                  # into the mirror only after the x64 build succeeded (siblings build from it); not the
        if rel.startswith(("port/src/", "port/include/")):   # C++ files: without the modules.inc line the mirror's own
            continue                                          # test_every_module_file_is_listed would fail for siblings
        if (ROOT / rel).exists():
            shutil.copyfile(ROOT / rel, MIRROR / rel)
    if a.w32:
        print("Win32:", build("Win32"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
