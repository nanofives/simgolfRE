"""38c private build mirror: port/ at HEAD (every tracked file at its committed version, so siblings' mid-edit files and
the parent's uncommitted CMakeLists change never reach this build) plus this stream's own files, with modules.inc =
HEAD + `SG_MODULE(reports)`. Output: port/build/m38c_root/port (+ re/model/include next to it).
Usage: ~/bin/py -3.12 log/p38/38c_mirror.py   then   build with log/p38/38c_build.ps1"""
import pathlib, shutil, subprocess

ROOT = pathlib.Path(__file__).resolve().parents[2]
DST = ROOT / "port" / "build" / "m38c_root" / "port"
MODEL = ROOT / "re" / "model" / "include"
OWN = ["port/src/sim/reports.cpp"]   # this stream's new port files (working tree versions)


def git(*a):
    return subprocess.run(["git", *a], cwd=ROOT, capture_output=True, check=True).stdout


names = [n for n in git("ls-files", "port/").decode().split("\n") if n and not n.startswith("port/corpus/")]
seen = set()
for n in names + OWN:
    rel = pathlib.Path(n).relative_to("port")
    d = DST / rel
    seen.add(d)
    d.parent.mkdir(parents=True, exist_ok=True)
    if n in OWN:
        data = (ROOT / n).read_bytes()
    elif n == "port/src/sim/modules.inc":
        data = git("show", "HEAD:" + n) + b"SG_MODULE(reports)\n"
    else:
        data = git("show", "HEAD:" + n)
    if not d.exists() or d.read_bytes() != data:
        d.write_bytes(data)
for p in MODEL.rglob("*"):
    if p.is_file():
        d = DST.parent / "re" / "model" / "include" / p.relative_to(MODEL)
        d.parent.mkdir(parents=True, exist_ok=True)
        if not d.exists() or d.read_bytes() != p.read_bytes():
            shutil.copy2(p, d)
for d in list(DST.rglob("*")):
    if d.is_file() and d not in seen:
        d.unlink()
print("mirrored", len(seen), "files to", DST)
