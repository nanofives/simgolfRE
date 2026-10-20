"""38b private build: HEAD's port/ + re/model/include, overlaid with this stream's files (log/p38/38b_files.txt, working
tree) and modules.inc = HEAD + the SG_MODULE lines in log/p38/38b_merge.md's `modules.inc:` block.

    ~/bin/py -3.12 log/p38/b_mirror.py [--build]     # mirror (and build Release x64); prints the DLL path

The DLL is copied to port/build/p38b_run/portsim_c_<mtime>.dll so a running test never locks the build tree.
Use it with SG_PORTSIM_DLL=<that path>.
"""
import pathlib, shutil, subprocess, sys, io, tarfile

ROOT = pathlib.Path(__file__).resolve().parents[2]
DST = ROOT / "port" / "build" / "p38b_root"
FILES = ROOT / "log" / "p38" / "38b_files.txt"
MODS = ["shotstyle", "loaders", "saveload"]     # SG_MODULE lines this stream adds (same text goes to 38b_merge.md)


def main():
    build = "--build" in sys.argv
    DST.mkdir(parents=True, exist_ok=True)
    # 1. HEAD snapshot
    data = subprocess.run(["git", "archive", "HEAD", "port", "re/model/include"], cwd=ROOT, capture_output=True,
                          check=True).stdout
    seen = set()
    with tarfile.open(fileobj=io.BytesIO(data)) as tf:
        for m in tf.getmembers():
            if not m.isfile() or m.name.startswith("port/build/") or m.name.startswith("port/corpus/"):
                continue
            d = DST / m.name
            seen.add(d)
            b = tf.extractfile(m).read()
            d.parent.mkdir(parents=True, exist_ok=True)
            if not d.exists() or d.read_bytes() != b:
                d.write_bytes(b)
    # 2. this stream's files from the working tree
    mine = [l.strip() for l in FILES.read_text().splitlines() if l.strip()] if FILES.exists() else []
    for rel in mine:
        if not rel.startswith("port/") or rel.endswith(".py") or rel.endswith(".json"):
            continue
        s = ROOT / rel
        d = DST / rel
        seen.add(d)
        if s.exists():
            d.parent.mkdir(parents=True, exist_ok=True)
            if not d.exists() or d.read_bytes() != s.read_bytes():
                d.write_bytes(s.read_bytes())
    # 3. modules.inc
    inc = DST / "port" / "src" / "sim" / "modules.inc"
    text = inc.read_text()
    # modules BEFORE tick: registry_find takes the first match, and `saveload` supersedes tick's saveGame placeholder
    for m in MODS:
        if not (DST / 'port' / 'src' / 'sim' / (m + '.cpp')).exists():
            continue
        line = f"SG_MODULE({m})"
        if line not in text:
            nl = chr(10)
            text = text.replace("SG_MODULE(tick)", line + nl + "SG_MODULE(tick)") if "SG_MODULE(tick)" in text else text + line + nl
    inc.write_text(text)
    # stale mirror files (deleted from HEAD) stay: harmless
    if not build:
        return
    cm = pathlib.Path(r"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions"
                      r"\Microsoft\CMake\CMake\bin\cmake.exe")
    bdir = DST / "port" / "build" / "x64"
    if not (bdir / "CMakeCache.txt").exists():
        subprocess.run([str(cm), "-S", str(DST / "port"), "-B", str(bdir), "-A", "x64"], check=True)
    # MSBuild does not see modules.inc: touch registry.cpp
    (DST / "port" / "src" / "registry.cpp").touch()
    r = subprocess.run([str(cm), "--build", str(bdir), "--config", "Release", "--target", "portsim_c", "portsim_core_test", "--", "/m"])
    if r.returncode:
        sys.exit(r.returncode)
    dll = bdir / "Release" / "portsim_c.dll"
    run = ROOT / "port" / "build" / "p38b_run"
    run.mkdir(parents=True, exist_ok=True)
    out = run / f"portsim_c_{int(dll.stat().st_mtime)}.dll"
    shutil.copy2(dll, out)
    print(out)


main()
