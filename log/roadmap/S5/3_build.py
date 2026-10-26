"""S5.3 builds, all through the VS cmake.exe (bare `cmake` is not allowed on claude2):

    ~/bin/py -3.12 log/roadmap/S5/3_build.py probe [--arch x64|Win32]     # log/roadmap/S5/3_probe -> port/build/S5_3_probe[_w32]
    ~/bin/py -3.12 log/roadmap/S5/3_build.py mirror [--build x64|Win32|both|none] [--fresh]

`mirror`: HEAD's port/ and re/model/include (git archive) into port/build/S5_3_src, plus S5.1's accepted files (gditext,
fttext), S5.2's accepted textlab and this step's own files; the shared-file changes the merge notes ask for are applied to the
mirror only: modules.inc + SG_MODULE(fttext) + SG_MODULE(textlab) + SG_MODULE(fttext_raster) (registry.cpp touched), port/CMakeLists.txt +
log/roadmap/S5/1_cmake_block.txt. Builds port/build/S5_3 (x64) and port/build/S5_3_w32 (Win32, with portsim_runner32), FreeType
from $SG_FREETYPE_ROOT or C:/Users/maria/vcpkg/installed.
"""
import argparse
import io
import os
import pathlib
import subprocess
import sys
import tarfile

ROOT = pathlib.Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT))
from port.test.build import cmake_exe  # noqa: E402

FT_ROOT = os.environ.get("SG_FREETYPE_ROOT", "C:/Users/maria/vcpkg/installed")
SRC = ROOT / "port" / "build" / "S5_3_src"
OWN = ["port/src/sim/gditext.cpp", "port/include/portsim/sim/gditext.h",
       "port/src/sim/fttext.cpp", "port/include/portsim/sim/fttext.h",
       "port/src/sim/textlab.cpp", "port/include/portsim/sim/textlab.h",
       "port/src/sim/fttext_raster.cpp", "port/include/portsim/sim/fttext_raster.h"]
CMAKE_BLOCK = ROOT / "log" / "roadmap" / "S5" / "1_cmake_block.txt"
BUILDS = {"x64": (ROOT / "port" / "build" / "S5_3", "x64"), "Win32": (ROOT / "port" / "build" / "S5_3_w32", "Win32"),
          "noft": (ROOT / "port" / "build" / "S5_3_noft", "x64")}


def run(cmd, **kw):
    print("+", " ".join(str(c) for c in cmd), flush=True)
    subprocess.run(cmd, check=True, **kw)


def probe(arch):
    b = ROOT / "port" / "build" / ("S5_3_probe" if arch == "x64" else "S5_3_probe_w32")
    run([cmake_exe(), "-S", str(ROOT / "log" / "roadmap" / "S5" / "3_probe"), "-B", str(b), "-A", arch])
    run([cmake_exe(), "--build", str(b), "--config", "Release", "--parallel"])


def put(rel, data, seen):
    d = SRC / rel
    seen.add(rel)
    d.parent.mkdir(parents=True, exist_ok=True)
    if not d.exists() or d.read_bytes() != data:
        d.write_bytes(data)
        if rel == "port/src/sim/modules.inc":
            reg = SRC / "port" / "src" / "registry.cpp"
            if reg.exists():
                os.utime(reg)


def sync():
    tar = subprocess.run(["git", "archive", "--format=tar", "HEAD", "port", "re/model/include"], cwd=ROOT,
                         capture_output=True, check=True).stdout
    seen = set()
    with tarfile.open(fileobj=io.BytesIO(tar)) as tf:
        for m in tf.getmembers():
            if not m.isfile():
                continue
            rel = m.name
            if rel.startswith("port/build/") or rel.startswith("port/corpus/"):
                continue
            data = tf.extractfile(m).read()
            if rel == "port/src/sim/modules.inc":
                data = data.rstrip(b"\n") + b"\nSG_MODULE(fttext)\nSG_MODULE(textlab)\nSG_MODULE(fttext_raster)\n"
            if rel == "port/CMakeLists.txt":
                data = data.rstrip(b"\n") + b"\n" + CMAKE_BLOCK.read_bytes()
            put(rel, data, seen)
    for rel in OWN:
        put(rel, (ROOT / rel).read_bytes(), seen)
    for p in list(SRC.rglob("*")):
        if p.is_file() and p.relative_to(SRC).as_posix() not in seen:
            p.unlink()


def mirror(kinds, fresh):
    sync()
    for k in kinds:
        b, arch = BUILDS[k]
        if fresh and (b / "CMakeCache.txt").exists():
            (b / "CMakeCache.txt").unlink()
        ft = [f"-DPORTSIM_FREETYPE_ROOT={FT_ROOT}"] if k != "noft" else ["-DPORTSIM_FREETYPE_ROOT=", "-DPORTSIM_FREETYPE=OFF"]
        run([cmake_exe(), "-S", str(SRC / "port"), "-B", str(b), "-A", arch] + ft)
        run([cmake_exe(), "--build", str(b), "--config", "Release", "--parallel", "--target", "portsim_c"]
            + (["portsim_runner32"] if arch == "Win32" else []))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("what", choices=["probe", "mirror"])
    ap.add_argument("--arch", default="x64", choices=["x64", "Win32"])
    ap.add_argument("--build", default="x64", choices=["x64", "Win32", "noft", "both", "none"])
    ap.add_argument("--fresh", action="store_true")
    a = ap.parse_args()
    if a.what == "probe":
        probe(a.arch)
    else:
        mirror({"both": ["x64", "Win32"], "none": []}.get(a.build, [a.build]), a.fresh)


if __name__ == "__main__":
    main()
