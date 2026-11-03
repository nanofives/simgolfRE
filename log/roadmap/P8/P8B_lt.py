"""P8B driver: one `simgolf_port.exe --live-trace <scenario>` run of the P8B tree with the scenario's install files, the record
directory kept under <out>. Usage:

    ~/bin/py -3.12 log/roadmap/P8/P8B_lt.py <scenario> <out dir> [--win32] [-- <extra app args>]

Prints the session.json's world / records / stop fields and the rc. The app is port/build/P8B/port/build/<arch>/app/Release.
"""
import json
import pathlib
import shutil
import subprocess
import sys
import time

TREE = pathlib.Path(r"C:\Users\maria\Desktop\Proyectos\SimGolf\port\build\P8B")
sys.path.insert(0, str(TREE))


def main() -> int:
    argv = sys.argv[1:]
    extra = []
    if "--" in argv:
        i = argv.index("--")
        extra = argv[i + 1:]
        argv = argv[:i]
    arch = "Win32" if "--win32" in argv else "x64"
    argv = [a for a in argv if a != "--win32"]
    scenario, out = argv[0], pathlib.Path(argv[1])
    from port.test import segment_files
    if out.exists():
        shutil.rmtree(out)
    out.mkdir(parents=True)
    exe = TREE / "port" / "build" / arch / "app" / "Release" / "simgolf_port.exe"
    args = [str(exe), "--install", str(TREE / "original"), "--data", str(out / "data"), "--record-dir", str(out / "rec"),
            "--live-trace", str(TREE / "port" / "corpus" / "traces" / f"{scenario}.trace")]
    for i, (name, data) in enumerate(segment_files.files_for(scenario).items()):
        p = out / "puts" / f"{i}.bin"
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_bytes(data)
        args += ["--put", f"{name}={p}"]
    t0 = time.time()
    r = subprocess.run(args + extra, cwd=str(TREE), capture_output=True, text=True, timeout=7200)
    (out / "stdout.txt").write_text(r.stdout)
    (out / "stderr.txt").write_text(r.stderr)
    sj = out / "rec" / "session.json"
    s = json.loads(sj.read_text()) if sj.exists() else {}
    print(json.dumps({"rc": r.returncode, "seconds": round(time.time() - t0, 1), "records": s.get("records"), "stop": s.get("stop"),
                      "code": s.get("code"), "presents": s.get("presents"), "world": s.get("world"),
                      "error": s.get("error")}, indent=None))
    return r.returncode


if __name__ == "__main__":
    sys.exit(main())
