"""38g private build mirror: the working tree's tracked port/ files (the parent's merged state: modules.inc already lists
reports and holestats, commands.h / tick.cpp carry 38e's patch) plus the untracked modules those lines need
(holestats, reports) and this stream's routing.cpp, modules.inc = working tree + SG_MODULE(routing). Output:
port/build/m38g_root/port (+ re/model/include). Usage: ~/bin/py -3.12 log/p38/38g_mirror.py"""
import pathlib, shutil, subprocess

ROOT = pathlib.Path(__file__).resolve().parents[2]
DST = ROOT / "port" / "build" / "m38g_root" / "port"
MODEL = ROOT / "re" / "model" / "include"
EXTRA = [  # (untracked files are picked up from the working tree anyway)
    "port/src/sim/holestats.cpp", "port/include/portsim/sim/holestats.h", "port/src/sim/reports.cpp",
         "port/src/sim/routing.cpp"]


def git(*a):
    return subprocess.run(["git", *a], cwd=ROOT, capture_output=True, check=True).stdout


names = [n for n in git("ls-files", "port/").decode().split("\n") if n and not n.startswith("port/corpus/")]
names += [n for n in git("ls-files", "-o", "--exclude-standard", "port/").decode().split(chr(10)) if n and not n.startswith("port/corpus/") and not n.startswith("port/build/")]
seen = set()
for n in names + EXTRA:
    src = ROOT / n
    if not src.exists():
        continue
    rel = src.relative_to(ROOT / "port")
    d = DST / rel
    seen.add(d)
    d.parent.mkdir(parents=True, exist_ok=True)
    data = src.read_bytes()
    if n == "port/src/sim/modules.inc":
        data = data.rstrip(b"\r\n") + b"\nSG_MODULE(routing)\n"
    if not d.exists() or d.read_bytes() != data:
        d.write_bytes(data)
for p in MODEL.rglob("*"):
    if p.is_file():
        d = DST.parent / "re" / "model" / "include" / p.relative_to(MODEL)
        d.parent.mkdir(parents=True, exist_ok=True)
        if not d.exists() or d.read_bytes() != p.read_bytes():
            shutil.copy2(p, d)
for d in list(DST.rglob("*")):
    if d.is_file() and d not in seen and "build" not in d.relative_to(DST).parts:
        if not str(d).startswith(str(DST.parent / "re")):
            d.unlink()
print("mirrored", len(seen), "files to", DST)
