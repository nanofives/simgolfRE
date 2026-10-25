"""S3.2 private build and test root.

    ~/bin/py -3.12 log/roadmap/S3/2_mirror.py [--w32] [--no-build] [--no-merge]

Source and test root port/build/S3_2_root:
  port/      = the S3 mirror's port/ (port/build/S3_head = git archive of c6d7d78b), minus build / corpus, plus
               - S2 wave 1 (S2.1, accepted): the files of log/roadmap/S2/files.txt under port/, from the working tree, and the
                 test_static.py edit of log/roadmap/S2/MERGE.md 1.1 (log/roadmap/S2/1_test_static.patch);
               - S3 wave 1 (S3.1 + S3.1b, accepted): the port files of log/roadmap/S3/files.txt from the working tree and the C ABI
                 edit of log/roadmap/S3/MERGE.md section 1 (log/roadmap/S3/1_mirror.py's OLD / NEW texts);
               - this step's own files (OWN below) and, unless --no-merge, the shared-file edits of log/roadmap/S3/2_merge.md
                 (2_merge_edits.py: exact-match replacements, refused when an anchor is not found exactly once);
  port/corpus, original, log, tests, re = junctions (port/corpus and original and log to the working tree; re and tests to the
               S3 mirror's HEAD copies).
Builds: port/build/S3_head/port/build/S3_2 (x64) and S3_2_w32 (Win32, with portsim_runner32). Tests run from the root:
    cd port/build/S3_2_root && SG_PORTSIM_DLL=<dll> ~/bin/py -3.12 -m pytest port/test/test_terrain_live.py
"""
import argparse
import importlib.util
import pathlib
import shutil
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
MIRROR = ROOT / "port" / "build" / "S3_head"
DST = ROOT / "port" / "build" / "S3_2_root"
BUILD = {"x64": MIRROR / "port" / "build" / "S3_2", "Win32": MIRROR / "port" / "build" / "S3_2_w32"}
OWN = ["port/src/sim/terrain_live.cpp", "port/include/portsim/sim/terrain_live.h", "port/src/sim/terrain_state.cpp",
       "port/include/portsim/sim/terrain_state.h", "port/src/sim/terrain_dll.cpp", "port/src/sim/terrain_gl_setup.cpp",
       "port/include/portsim/sim/terrain_gl_setup.h", "port/src/sim/gfx_terrain.cpp", "port/test/terrainlive.py",
       "port/test/test_terrain_live.py", "port/test/test_terrain_gl_setup.py", "port/test/runner32/runner32.cpp", "port/test/lib32.py"]
JUNCTIONS = {"port/corpus": ROOT / "port" / "corpus", "original": ROOT / "original", "log": ROOT / "log",
             "tests": MIRROR / "tests", "re": MIRROR / "re"}
CMAKE = r"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"


def _load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    m = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(m)
    return m


def wave_files(listing: pathlib.Path) -> list[str]:
    return [ln.strip() for ln in listing.read_text().splitlines() if ln.strip().startswith("port/")]


def patch(text: str, old: str, new: str, what: str) -> str:
    if "\r\n" in text:
        old, new = old.replace("\n", "\r\n"), new.replace("\n", "\r\n")
    if text.count(old) != 1:
        raise SystemExit(f"{what}: anchor found {text.count(old)} times")
    return text.replace(old, new)


def junction(link: pathlib.Path, target: pathlib.Path):
    if link.exists():
        return
    link.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run(["cmd", "/c", "mklink", "/J", str(link), str(target)], check=True, capture_output=True)


def copy_tree(merge: bool = True):
    DST.mkdir(parents=True, exist_ok=True)
    port = DST / "port"
    if port.exists():
        # the corpus junction first (rmtree would follow it)
        cj = port / "corpus"
        if cj.exists():
            subprocess.run(["cmd", "/c", "rmdir", str(cj)], check=True, capture_output=True)
        shutil.rmtree(port)
    shutil.copytree(MIRROR / "port", port, ignore=shutil.ignore_patterns("build", "corpus", "__pycache__"))
    for name in ("pytest.ini", "CLAUDE.md"):
        shutil.copyfile(MIRROR / name, DST / name)
    for rel, tgt in JUNCTIONS.items():
        junction(DST / rel, tgt)
    for rel in wave_files(ROOT / "log/roadmap/S2/files.txt") + wave_files(ROOT / "log/roadmap/S3/files.txt") + OWN:
        src = ROOT / rel
        if src.exists():
            (DST / rel).parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(src, DST / rel)
    # S2 MERGE 1.1: test_static.py's family-aware module listing
    ts = port / "test" / "test_static.py"
    t = ts.read_text()
    old = '    files = {p.stem for p in (PORT / "src" / "sim").glob("*.cpp")}\n'
    new = ('    # gfx body families (roadmap S2.1, port/src/sim/gfx_body.h): gfx_<f>.cpp files listed in gfx_families.inc, not modules\n'
           '    families = set(re.findall(r"^SG_GFX_FAMILY\\((\\w+)\\)", (PORT / "src" / "sim" / "gfx_families.inc").read_text(), re.M))\n'
           '    for f in families:\n'
           '        src = (PORT / "src" / "sim" / f"gfx_{f}.cpp").read_text(encoding="utf-8")\n'
           '        assert f"kGfxFamily_{f}" in src and f"g_negctl_gfx_{f}" in src, f\n'
           '    files = {p.stem for p in (PORT / "src" / "sim").glob("*.cpp")} - {f"gfx_{f}" for f in families}\n')
    ts.write_text(patch(t, old, new, "test_static.py"))
    # S3 MERGE section 1: the C ABI routing of every render entry kind
    m1 = _load("s31_mirror", ROOT / "log/roadmap/S3/1_mirror.py")
    c = port / "src" / "portsim_c.cpp"
    t = c.read_bytes().decode()
    t = patch(t, m1.OLD_RENDER, m1.NEW_RENDER, "sg_gl_render")
    t = patch(t, m1.OLD_RASTER, m1.NEW_RASTER, "sg_gl_render_to_raster")
    c.write_bytes(t.encode())
    h = port / "include" / "portsim_c.h"
    h.write_bytes(patch(h.read_bytes().decode(), m1.OLD_DOC, m1.NEW_DOC, "portsim_c.h").encode())
    if merge:
        me = _load("s32_merge", ROOT / "log/roadmap/S3/2_merge_edits.py")
        for rel, old, new in me.EDITS:
            p = DST / rel
            b = p.read_bytes().decode()
            p.write_bytes(patch(b, old, new, rel).encode())
    (port / "src" / "registry.cpp").touch()


def build(arch: str):
    b = BUILD[arch]
    subprocess.run([CMAKE, "-S", str(DST / "port"), "-B", str(b), "-A", arch], check=True)
    subprocess.run([CMAKE, "--build", str(b), "--config", "Release", "--target", "portsim_c", "--", "/m"], check=True)
    if arch == "Win32":
        subprocess.run([CMAKE, "--build", str(b), "--config", "Release", "--target", "portsim_runner32", "--", "/m"], check=True)
    return b / "Release" / "portsim_c.dll"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--w32", action="store_true")
    ap.add_argument("--no-build", action="store_true")
    ap.add_argument("--no-merge", action="store_true", help="without this step's shared-file edits (the control build)")
    a = ap.parse_args()
    copy_tree(merge=not a.no_merge)
    if a.no_build:
        return 0
    print("SG_PORTSIM_DLL=" + str(build("x64")))
    if a.w32:
        print("Win32:", build("Win32"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
