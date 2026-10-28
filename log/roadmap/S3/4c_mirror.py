"""S3.4c private tree: git archive of HEAD bbc9072f + S3 wave 2 (as I6 applied it, port/build/I6) + S3 wave 4 (as S3.6b applied
it on I6's tree, port/build/S3_6b_src) + this step's own files from the shared tree.

    ~/bin/py -3.12 log/roadmap/S3/4c_mirror.py [--recopy] [--own-only] [--no-build] [--w32]

Source root port/build/S3_4c_src (junctions for gitignored data as log/roadmap/merges/I6/mirror.py). port/ files: every file of
port/build/S3_6b_src/port that differs from HEAD's (CR / LF ignored) is copied from there; those are exactly S3 wave 2's
(flic.cpp, flic.h, test_flic.py, test_defects_flic.py, menu.cpp, modules.inc) and wave 4's (gfx_hud / gfx_pixel files,
commands_layout.inc, gfx.cpp, gfx_body.h, gfx_families.inc, tick.cpp) (checked in 4c_notes.md section 1). Non-port files of
wave 2: re/port/defect_profiles.tsv and defect_decisions.tsv from port/build/I6 (HEAD + the D53 row / the flic line), the two
recorders. Then OWN (this step's files) from the shared tree. Builds: port/build/S3_4c (x64), port/build/S3_4c_w32 (Win32).
"""
import filecmp
import os
import pathlib
import shutil
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
HEAD = "bbc9072f85feb9df48a4ba7ec0a18246835cfe01"
DST = ROOT / "port" / "build" / "S3_4c_src"
S6B = ROOT / "port" / "build" / "S3_6b_src" / "port"
I6 = ROOT / "port" / "build" / "I6"
JUNCTIONS = ["original", "log", "port/corpus", "tests/golden", "tests/fixtures", "re/versions", "tools/vc6", "shim/build",
             "re/replay", "extracted", "instances"]
NONPORT = ["re/port/defect_profiles.tsv", "re/port/defect_decisions.tsv", "re/tools/flic_cells_rec.py", "re/frida/js/flic_cells.js"]
OWN = ["port/test/test_menu.py", "port/test/test_terrain_live.py", "port/test/terrainlive.py", "port/test/test_flic.py",
       "port/test/test_defects_flic.py"]
OWN_GLOB = ["port/test/golden_menu*.json"]
BUILD = {"x64": ROOT / "port" / "build" / "S3_4c", "Win32": ROOT / "port" / "build" / "S3_4c_w32"}
CMAKE = r"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"


def norm(p):
    return p.read_bytes().replace(b"\r\n", b"\n")


def tree():
    DST.mkdir(parents=True, exist_ok=True)
    tar = subprocess.run(["git", "archive", HEAD], cwd=ROOT, capture_output=True, check=True).stdout
    subprocess.run(["tar", "-xf", "-"], cwd=DST, input=tar, check=True)
    (DST / "HEAD_COMMIT").write_text(HEAD + "\n")
    for j in JUNCTIONS:
        link = DST / j
        if link.exists() and not os.path.isjunction(link):
            shutil.rmtree(link)
        if not link.exists() and (ROOT / j).exists():
            link.parent.mkdir(parents=True, exist_ok=True)
            subprocess.run(["cmd", "/c", "mklink", "/J", str(link), str(ROOT / j)], check=True, capture_output=True)
    n = 0
    for p in S6B.rglob("*"):
        rel = p.relative_to(S6B)
        if not p.is_file() or rel.parts[0] in ("build", "corpus") or "__pycache__" in rel.parts:
            continue
        d = DST / "port" / rel
        if d.exists() and norm(d) == norm(p):
            continue
        d.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(p, d)
        print("wave2/4", "port/" + str(rel).replace("\\", "/"))
        n += 1
    for rel in NONPORT:
        shutil.copyfile(I6 / rel, DST / rel)
        print("wave2", rel)
    print(n, "port files from S3_6b_src")


def own():
    for rel in OWN:
        shutil.copyfile(ROOT / rel, DST / rel)
    for g in OWN_GLOB:
        for p in ROOT.glob(g):
            shutil.copyfile(p, DST / p.relative_to(ROOT))
    (DST / "port" / "src" / "registry.cpp").touch()


def build(arch):
    b = BUILD[arch]
    subprocess.run([CMAKE, "-S", str(DST / "port"), "-B", str(b), "-A", arch], check=True)
    subprocess.run([CMAKE, "--build", str(b), "--config", "Release", "--target", "portsim_c", "--", "/m"], check=True)
    if arch == "Win32":
        subprocess.run([CMAKE, "--build", str(b), "--config", "Release", "--target", "portsim_runner32", "--", "/m"], check=True)
    print("SG_PORTSIM_DLL=" + str(b / "Release" / "portsim_c.dll"))


def main():
    a = sys.argv[1:]
    if "--own-only" not in a and (not (DST / "port" / "CMakeLists.txt").exists() or "--recopy" in a):
        tree()
    own()
    if "--no-build" in a:
        return 0
    build("x64")
    if "--w32" in a:
        build("Win32")
    return 0


if __name__ == "__main__":
    sys.exit(main())
