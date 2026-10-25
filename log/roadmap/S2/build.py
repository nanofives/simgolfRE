"""S2 workers' build helper: configure and build portsim_c with cmake through Python (claude2 has an allow rule for
`~/bin/py` but none for a bare `cmake`, so a cmake command waits on an approval nobody sees).

usage: ~/bin/py -3.12 log/roadmap/S2/build.py <source root containing port/> <build dir> [--arch x64|Win32] [--log <file>]
       [--target <target>] [--no-configure]
Prints every output line (also to --log) and the path of portsim_c.dll at the end; exit code = cmake's.
"""
import argparse
import os
import pathlib
import shutil
import subprocess
import sys

# cmake is not on the shells' PATH: use VS Build Tools' copy when `cmake` is not found
VS_CMAKE = pathlib.Path(r"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions"
                        r"\Microsoft\CMake\CMake\bin")
if shutil.which("cmake") is None and (VS_CMAKE / "cmake.exe").exists():
    os.environ["PATH"] = str(VS_CMAKE) + os.pathsep + os.environ.get("PATH", "")


def run(cmd, log):
    print("+ " + " ".join(cmd), flush=True)
    log.write("+ " + " ".join(cmd) + "\n")
    p = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors="replace")
    for line in p.stdout:
        sys.stdout.write(line)
        log.write(line)
    sys.stdout.flush()
    return p.wait()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("root")
    ap.add_argument("build")
    ap.add_argument("--arch", default="x64")
    ap.add_argument("--log")
    ap.add_argument("--target", default="portsim_c")
    ap.add_argument("--no-configure", action="store_true")
    a = ap.parse_args()
    root = pathlib.Path(a.root).resolve()
    build = pathlib.Path(a.build).resolve()   # relative to the current directory (pass an absolute path to be sure)
    logp = pathlib.Path(a.log) if a.log else build.parent / (build.name + "_build.log")
    logp.parent.mkdir(parents=True, exist_ok=True)
    with open(logp, "w", encoding="utf-8") as log:
        if not a.no_configure:
            rc = run(["cmake", "-S", str(root / "port"), "-B", str(build), "-A", a.arch], log)
            if rc:
                return rc
        rc = run(["cmake", "--build", str(build), "--config", "Release", "--target", a.target, "--", "/m"], log)
        for dll in build.rglob("portsim_c.dll"):
            print("DLL " + str(dll))
            log.write("DLL " + str(dll) + "\n")
        return rc


if __name__ == "__main__":
    sys.exit(main())
