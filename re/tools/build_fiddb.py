"""Build re/fid/simgolf_vc6.fidb: Ghidra Function ID database from the VC6 static libraries
(tools/vc6/vc98/lib, the exact compiler in the game's Rich headers) plus original/JPEG.lib.

Usage: py -3.12 re/tools/build_fiddb.py [--only libc,JPEG] [--out re/fid/x.fidb]
Work project: ghidra/fid_work/fidbuild (not committed). Imported libraries are kept there, so a
re-run only re-populates the database. Never touches the master project.
"""
import argparse, pathlib, re, subprocess, sys
ROOT = pathlib.Path(__file__).resolve().parents[2]
GHIDRA = pathlib.Path(r"C:\Users\maria\Desktop\Proyectos\TD5RE\ghidra\ghidra_12.0.3_PUBLIC")
HEADLESS = GHIDRA / "support" / "analyzeHeadless.bat"
COMMON = GHIDRA / "Ghidra" / "Features" / "FunctionID" / "data" / "common_symbols_win32.txt"
WORK = ROOT / "ghidra" / "fid_work"
VC6LIB = ROOT / "tools" / "vc6" / "vc98" / "lib"
VC6 = ("VC6", "12.00.8168")
# (family, version, variant, path). Release variants for golf_clean.exe (/O2), debug variants for
# the debug-built Terrain.dll / jgld.dll. All four game modules link the CRT statically.
LIBS = [(*VC6, v, VC6LIB / f"{v}.lib") for v in (
    "libc", "libcmt", "libcp", "libcpmt", "libci", "libcimt",
    "libcd", "libcmtd", "libcpd", "libcpmtd", "libcid", "libcimtd")]
LIBS.append(("IJG libjpeg", "SimGolf JPEG.lib", "JPEG", ROOT / "original" / "JPEG.lib"))
DUMMY = ROOT / "original" / "sound.dll"  # -process needs a program; it is never analyzed or saved

def run(args):
    r = subprocess.run([str(HEADLESS), str(WORK), "fidbuild", *args], capture_output=True, text=True)
    return r.stdout + r.stderr

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--only", help="comma-separated variants")
    ap.add_argument("--out", default=str(ROOT / "re" / "fid" / "simgolf_vc6.fidb"))
    a = ap.parse_args()
    libs = [l for l in LIBS if not a.only or l[2] in a.only.split(",")]
    WORK.mkdir(parents=True, exist_ok=True)
    pathlib.Path(a.out).parent.mkdir(parents=True, exist_ok=True)
    if not (WORK / "fidbuild.gpr").exists():
        run(["-import", str(DUMMY), "-noanalysis"])
    out = run(["-process", DUMMY.name, "-noanalysis", "-readOnly",
               "-scriptPath", str(ROOT / "ghidra" / "scripts"),
               "-postScript", "BuildFidDb.java", a.out, str(COMMON),
               *[f"{f}#{v}#{var}#{p}" for f, v, var, p in libs]])
    (WORK / "build_fiddb.log").write_text(out, encoding="utf-8")
    keep = [re.sub(r"^INFO  BuildFidDb\.java> | \(GhidraScript\)\s*$", "", l) for l in out.splitlines()
            if re.search(r"BuildFidDb\.java> (IMPORTED|SKIP|LIB|FIDDB|MEMBER-FAIL)|ERROR|Exception", l)]
    print("\n".join(keep) or out[-3000:])
    return 0 if any("FIDDB-WRITTEN" in l for l in keep) else 1

if __name__ == "__main__":
    sys.exit(main())
