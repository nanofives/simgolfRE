"""38b second-round mirror: the WORKING TREE's port/ (every stream's merged files) + this stream's modules before tick.
    ~/bin/py -3.12 log/p38/b_mirror_wt.py --build
DLL -> port/build/p38b_run (same place as b_mirror.py, newest wins in b_pytest.sh)."""
import pathlib, shutil, subprocess, sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
SRC = ROOT / "port"
DST = ROOT / "port" / "build" / "p38b_wt_root"
MODS = [] if "--nomods" in sys.argv else ["shotstyle", "loaders", "saveload"]
nl = chr(10)


def main():
    seen = set()
    for p in SRC.rglob("*"):
        rel = p.relative_to(SRC)
        if rel.parts[0] in ("build", "corpus", "__pycache__") or "__pycache__" in rel.parts or p.is_dir():
            continue
        if p.suffix in (".py", ".json"):
            continue
        d = DST / "port" / rel
        seen.add(d)
        b = p.read_bytes()
        d.parent.mkdir(parents=True, exist_ok=True)
        if not d.exists() or d.read_bytes() != b:
            d.write_bytes(b)
    mdl = ROOT / "re" / "model" / "include"
    for p in mdl.rglob("*"):
        if p.is_file():
            d = DST / "re" / "model" / "include" / p.relative_to(mdl)
            d.parent.mkdir(parents=True, exist_ok=True)
            if not d.exists() or d.read_bytes() != p.read_bytes():
                shutil.copy2(p, d)
    inc = DST / "port" / "src" / "sim" / "modules.inc"
    text = inc.read_text()
    for m in MODS:
        line = "SG_MODULE(" + m + ")"
        if line not in text:
            text = text.replace("SG_MODULE(tick)", line + nl + "SG_MODULE(tick)", 1)
    inc.write_text(text)
    for d in list((DST / "port").rglob("*")):
        if d.is_file() and d not in seen and d.suffix not in (".py", ".json") and "build" not in d.relative_to(DST).parts:
            d.unlink()
    if "--build" not in sys.argv:
        return
    cm = pathlib.Path(r"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe")
    bdir = DST / "port" / "build" / "x64"
    if not (bdir / "CMakeCache.txt").exists():
        subprocess.run([str(cm), "-S", str(DST / "port"), "-B", str(bdir), "-A", "x64"], check=True)
    (DST / "port" / "src" / "registry.cpp").touch()
    r = subprocess.run([str(cm), "--build", str(bdir), "--config", "Release", "--target", "portsim_c", "--", "/m"])
    if r.returncode:
        sys.exit(r.returncode)
    dll = bdir / "Release" / "portsim_c.dll"
    run = ROOT / "port" / "build" / "p38b_run"
    run.mkdir(parents=True, exist_ok=True)
    out = run / ("portsim_c_" + str(int(dll.stat().st_mtime)) + ".dll")
    shutil.copy2(dll, out)
    print(out)


main()
