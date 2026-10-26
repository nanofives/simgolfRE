"""S5.2 private build: HEAD's port/ and re/model/include (git archive) plus this step's own files, in port/build/S5_2_src,
built into port/build/S5_2 (x64) and port/build/S5_2_w32 (Win32). modules.inc gets one line, SG_MODULE(textlab); registry.cpp
is touched after modules.inc changes (MSBuild does not rebuild it).

    ~/bin/py -3.12 log/roadmap/S5/2_mirror.py [--build x64|Win32|both]
"""
import argparse
import io
import os
import pathlib
import subprocess
import sys
import tarfile

ROOT = pathlib.Path(__file__).resolve().parents[3]
SRC = ROOT / "port" / "build" / "S5_2_src"
OWN = ["port/src/sim/textlab.cpp", "port/include/portsim/sim/textlab.h"]
sys.path.insert(0, str(ROOT))
from port.test.build import cmake_exe  # noqa: E402

BUILDS = {"x64": ROOT / "port" / "build" / "S5_2", "Win32": ROOT / "port" / "build" / "S5_2_w32"}


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
                data = data.rstrip(b"\n") + b"\nSG_MODULE(textlab)\n"
            put(rel, data, seen)
    for rel in OWN:
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


def build(arch):
    b = BUILDS[arch]
    if not (b / "CMakeCache.txt").exists():
        subprocess.run([cmake_exe(), "-S", str(SRC / "port"), "-B", str(b), "-A", arch], check=True)
    subprocess.run([cmake_exe(), "--build", str(b), "--config", "Release", "--target", "portsim_c"]
                   + (["portsim_runner32"] if arch == "Win32" else []), check=True)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--build", default="both", choices=["x64", "Win32", "both", "none"])
    a = ap.parse_args()
    sync()
    for arch in (["x64", "Win32"] if a.build == "both" else [] if a.build == "none" else [a.build]):
        build(arch)


if __name__ == "__main__":
    main()
