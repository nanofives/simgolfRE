"""S3.6b test runner: pytest, one file per process, from the S3.6b source root (port/build/S3_6b_src) against this
step's builds. x64 uses a copy of the S3_6b DLL (port/build/S3_head2/port/build/S3_6b_run) so a rebuild never touches a
running DLL; Win32 goes through the junction port/build/S3_6b_src/port/build/Win32 -> S3_6b_w32 (lib32 reads that path).

    python log/roadmap/S3/6b_run.py [--w32] [--log <file>] [--base] <test file>... [-- <pytest args>]

Prints one line per file, `[N/M] <file> PASS|FAIL (<pytest summary>)`, and the full output to the log.
"""
import os
import pathlib
import shutil
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
SRC = ROOT / "port" / "build" / "S3_6b_src"
B = ROOT / "port" / "build" / "S3_head2" / "port" / "build"
PY = sys.executable


def junction(link: pathlib.Path, target: pathlib.Path):
    if link.exists():
        return
    link.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run(["cmd", "/c", "mklink", "/J", str(link), str(target)], check=True, capture_output=True)


def main():
    a = sys.argv[1:]
    extra = []
    if "--" in a:
        i = a.index("--")
        a, extra = a[:i], a[i + 1:]
    w32 = "--w32" in a
    base = "--base" in a
    log = None
    if "--log" in a:
        i = a.index("--log")
        log = pathlib.Path(a[i + 1])
        del a[i:i + 2]
    files = [x for x in a if not x.startswith("--")]
    env = dict(os.environ)
    if w32:
        junction(SRC / "port" / "build" / "Win32", B / "S3_6b_w32")
        env["SG_BACKEND"] = "win32"
    else:
        src = B / ("S3_6b_base" if base else "S3_6b") / "Release" / "portsim_c.dll"
        run = B / ("S3_6b_base_run" if base else "S3_6b_run")
        run.mkdir(exist_ok=True)
        dst = run / "portsim_c.dll"
        if not dst.exists() or dst.stat().st_mtime < src.stat().st_mtime:
            shutil.copyfile(src, dst)
        env["SG_PORTSIM_DLL"] = str(dst)
    out = open(log, "a", encoding="utf-8") if log else None
    bad = 0
    for k, f in enumerate(files, 1):
        p = subprocess.run([PY, "-m", "pytest", f, "-q", "-p", "no:cacheprovider", *extra], cwd=SRC, env=env,
                           capture_output=True, text=True, encoding="utf-8", errors="replace")
        text = p.stdout + p.stderr
        if out:
            out.write(f"===== {f} ({'win32' if w32 else 'x64'})\n{text}\n")
            out.flush()
        last = [l for l in text.splitlines() if l.strip()]
        summ = last[-1] if last else ""
        ok = p.returncode == 0
        bad += not ok
        print(f"[{k}/{len(files)}] {f} {'PASS' if ok else 'FAIL'} ({summ})", flush=True)
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
