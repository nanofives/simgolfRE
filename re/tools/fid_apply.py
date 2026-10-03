"""Apply Function ID names (installed FidDbs + re/fid/simgolf_vc6.fidb) to the MASTER project.

Usage: py -3.12 re/tools/fid_apply.py [--program golf_clean.exe ...] [--dry-run]
Writes re/fid/<program>.fid_applied.tsv (entry, old, new). Uses Ghidra's ApplyFidEntriesCommand with
alwaysApplyFidLabels=false, so USER_DEFINED / IMPORTED names are never replaced. Refuses to run while the
master is locked (GUI or MCP has it open). Afterwards: scripts/ghidra_pool.sh sync.
"""
import argparse, pathlib, re, subprocess, sys
ROOT = pathlib.Path(__file__).resolve().parents[2]
HEADLESS = r"C:\Users\maria\Desktop\Proyectos\TD5RE\ghidra\ghidra_12.0.3_PUBLIC\support\analyzeHeadless.bat"
PROGRAMS = ["golf_clean.exe", "Terrain.dll", "jgld.dll", "sound.dll"]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--program", nargs="+", default=PROGRAMS)
    ap.add_argument("--fidb", default=str(ROOT / "re" / "fid" / "simgolf_vc6.fidb"))
    ap.add_argument("--dry-run", action="store_true", help="run on the master read-only (nothing saved)")
    a = ap.parse_args()
    if list(ROOT.glob("SimGolf.lock*")):
        sys.exit("master SimGolf.gpr is locked (open in Ghidra/MCP); close it first")
    rc = 0
    for prog in a.program:
        out = ROOT / "re" / "fid" / f"{prog}.fid_applied{'.dry' if a.dry_run else ''}.tsv"
        r = subprocess.run([HEADLESS, str(ROOT), "SimGolf", "-process", prog, "-noanalysis",
                            *(["-readOnly"] if a.dry_run else []),
                            "-scriptPath", str(ROOT / "ghidra" / "scripts"),
                            "-postScript", "ApplyFid.java", str(pathlib.Path(a.fidb).resolve()), str(out)],
                           capture_output=True, text=True)
        log = r.stdout + r.stderr
        m = re.search(r"FIDAPPLY .*", log)
        print(m.group(0).split(" (GhidraScript)")[0] if m else f"{prog}: FAILED\n{log[-3000:]}")
        rc |= 0 if m else 1
    return rc


if __name__ == "__main__":
    sys.exit(main())
