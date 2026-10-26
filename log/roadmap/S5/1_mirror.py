"""S5.1 private build: HEAD's port/ and re/model/include (git archive) plus this step's own files, in port/build/S5_1_src,
built into port/build/S5_1 (x64), port/build/S5_1_w32 (Win32) and, for gate (a), port/build/S5_1_noft (x64 configured with
no FreeType root). Changes to the mirror's shared files, as log/roadmap/S5/1_merge.md asks the integrator for:
modules.inc gets SG_MODULE(fttext) (registry.cpp touched after a modules.inc change: MSBuild does not rebuild it), and
port/CMakeLists.txt gets log/roadmap/S5/1_cmake_block.txt appended at its end.

    ~/bin/py -3.12 log/roadmap/S5/1_mirror.py [--build x64|Win32|noft|both|all|none] [--with-textlab] [--fresh]
                                               [--ft-root C:/Users/maria/vcpkg/installed]

--ft-root (default $SG_FREETYPE_ROOT) is passed to x64 / Win32 as -DPORTSIM_FREETYPE_ROOT (a vcpkg `installed` dir: the
triplet is picked by the architecture); the noft build gets no root and SG_FREETYPE_ROOT removed from its environment, so
it shows what a host without FreeType builds. --fresh deletes the build directory's CMakeCache.txt first.

--with-textlab also copies S5.2's untracked textlab.cpp / textlab.h (compile check of the gditext refactor against it; no
modules.inc line, so the module is compiled but not registered).
"""
import argparse
import io
import os
import pathlib
import subprocess
import sys
import tarfile

ROOT = pathlib.Path(__file__).resolve().parents[3]
SRC = ROOT / "port" / "build" / "S5_1_src"
OWN = ["port/src/sim/gditext.cpp", "port/include/portsim/sim/gditext.h", "port/src/sim/fttext.cpp",
       "port/include/portsim/sim/fttext.h"]
TEXTLAB = ["port/src/sim/textlab.cpp", "port/include/portsim/sim/textlab.h"]
CMAKE_BLOCK = ROOT / "log" / "roadmap" / "S5" / "1_cmake_block.txt"
sys.path.insert(0, str(ROOT))
from port.test.build import cmake_exe  # noqa: E402

BUILDS = {"x64": (ROOT / "port" / "build" / "S5_1", "x64", True),
          "Win32": (ROOT / "port" / "build" / "S5_1_w32", "Win32", True),
          "noft": (ROOT / "port" / "build" / "S5_1_noft", "x64", False)}


def sync(with_textlab: bool):
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
                data = data.rstrip(b"\n") + b"\nSG_MODULE(fttext)\n"
            if rel == "port/CMakeLists.txt":
                data = data.rstrip(b"\n") + b"\n" + CMAKE_BLOCK.read_bytes()
            put(rel, data, seen)
    for rel in OWN + (TEXTLAB if with_textlab else []):
        put(rel, (ROOT / rel).read_bytes(), seen)
    for p in list(SRC.rglob("*")):
        if p.is_file() and p.relative_to(SRC).as_posix() not in seen:
            p.unlink()


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


def build(kind, ft_root, fresh):
    b, arch, with_ft = BUILDS[kind]
    if fresh and (b / "CMakeCache.txt").exists():
        (b / "CMakeCache.txt").unlink()
    env = dict(os.environ)
    env.pop("SG_FREETYPE_ROOT", None)
    extra = [f"-DPORTSIM_FREETYPE_ROOT={ft_root}"] if with_ft and ft_root else ["-DPORTSIM_FREETYPE_ROOT="]
    subprocess.run([cmake_exe(), "-S", str(SRC / "port"), "-B", str(b), "-A", arch] + extra, check=True, env=env)
    subprocess.run([cmake_exe(), "--build", str(b), "--config", "Release", "--parallel", "--target", "portsim_c"]
                   + (["portsim_runner32"] if arch == "Win32" else []), check=True, env=env)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--build", default="both", choices=["x64", "Win32", "noft", "both", "all", "none"])
    ap.add_argument("--with-textlab", action="store_true")
    ap.add_argument("--fresh", action="store_true")
    ap.add_argument("--ft-root", default=os.environ.get("SG_FREETYPE_ROOT", ""))
    a = ap.parse_args()
    sync(a.with_textlab)
    kinds = {"both": ["x64", "Win32"], "all": ["x64", "Win32", "noft"], "none": []}.get(a.build, [a.build])
    for k in kinds:
        build(k, a.ft_root, a.fresh)


if __name__ == "__main__":
    main()
