"""S3.8 development check with S3.7: a copy of this step's source tree (port/build/S3_8_src) plus S3.7's files as they are in the working
tree (gfx_text.cpp / .h) and its shared-file edits (log/roadmap/S3/7_edits.py, paths relative to port/), the family line
SG_GFX_FAMILY(text); built into port/build/S3_head2/port/build/S3_8c. S3.7 is still running, so this measures a snapshot of its files.

    ~/bin/py -3.12 log/roadmap/S3/8_combo.py
"""
import importlib.util
import os
import pathlib
import shutil
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
SRC8 = ROOT / "port" / "build" / "S3_8_src"
DST = ROOT / "port" / "build" / "S3_8c_src"
BUILD = ROOT / "port" / "build" / "S3_head2" / "port" / "build" / "S3_8c"
CMAKE = r"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"


def load(path):
    s = importlib.util.spec_from_file_location(path.stem.replace(".", "_"), path)
    m = importlib.util.module_from_spec(s)
    s.loader.exec_module(m)
    return m


def main():
    m8 = load(ROOT / "log" / "roadmap" / "S3" / "8_mirror.py")
    DST.mkdir(exist_ok=True)
    for e in SRC8.iterdir():
        if e.name == "port":
            continue
        if e.is_dir():
            m8.junction(DST / e.name, pathlib.Path(os.path.realpath(e)))
        else:
            shutil.copyfile(e, DST / e.name)
    port = DST / "port"
    if port.exists():
        for e in port.iterdir():
            if e.is_junction():
                os.rmdir(e)
            elif e.is_dir():
                shutil.rmtree(e)
            else:
                e.unlink()
    shutil.copytree(SRC8 / "port", port, ignore=shutil.ignore_patterns("build", "corpus", "__pycache__"), dirs_exist_ok=True)
    m8.junction(port / "corpus", ROOT / "port" / "corpus")
    for rel in ("src/sim/gfx_text.cpp", "include/portsim/sim/gfx_text.h"):
        shutil.copyfile(ROOT / "port" / rel, port / rel)
    fam = port / "src" / "sim" / "gfx_families.inc"
    t = fam.read_bytes().decode()
    nl = "\r\n" if "\r\n" in t else "\n"
    if "SG_GFX_FAMILY(text)" not in t:
        fam.write_bytes((t.rstrip("\r\n") + nl + "SG_GFX_FAMILY(text)" + nl).encode())
    for rel, old, new in load(ROOT / "log" / "roadmap" / "S3" / "7_edits.py").EDITS:
        p = port / rel
        b = p.read_bytes().decode()
        p.write_bytes(m8.patch(b, old, new, "S3.7 " + rel).encode())
    (port / "src" / "registry.cpp").touch()
    subprocess.run([CMAKE, "-S", str(port), "-B", str(BUILD), "-A", "x64"], check=True)
    subprocess.run([CMAKE, "--build", str(BUILD), "--config", "Release", "--target", "portsim_c", "--", "/m"], check=True)
    print("SG_PORTSIM_DLL=" + str(BUILD / "Release" / "portsim_c.dll"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
