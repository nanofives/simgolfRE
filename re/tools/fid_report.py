"""Function ID report: which functions of a module match Ghidra's installed FidDbs plus
re/fid/simgolf_vc6.fidb (built by re/tools/build_fiddb.py). Read-only on a pool slot; applies nothing.

Usage: py -3.12 re/tools/fid_report.py [--program golf_clean.exe] [--fidb re/fid/simgolf_vc6.fidb]
Writes re/fid/<program>.fid.tsv and prints a summary: matches by library, and how many functions
still named FUN_* in the master would get a unique name.
"""
import argparse, collections, csv, pathlib, re, subprocess, sys
ROOT = pathlib.Path(__file__).resolve().parents[2]
HEADLESS = r"C:\Users\maria\Desktop\Proyectos\TD5RE\ghidra\ghidra_12.0.3_PUBLIC\support\analyzeHeadless.bat"
BASH = r"C:\Program Files\Git\bin\bash.exe"
POOL = (ROOT / "scripts" / "ghidra_pool.sh").as_posix()

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--program", default="golf_clean.exe")
    ap.add_argument("--fidb", default=str(ROOT / "re" / "fid" / "simgolf_vc6.fidb"))
    a = ap.parse_args()
    out = ROOT / "re" / "fid" / f"{a.program}.fid.tsv"
    out.parent.mkdir(parents=True, exist_ok=True)
    slot = subprocess.run([BASH, POOL, "acquire"], capture_output=True, text=True).stdout.strip().splitlines()[-1]
    try:
        r = subprocess.run([HEADLESS, str(ROOT / "simgolf_pool"), slot, "-process", a.program, "-readOnly",
                            "-noanalysis", "-scriptPath", str(ROOT / "ghidra" / "scripts"),
                            "-postScript", "FidReport.java", str(pathlib.Path(a.fidb).resolve()), str(out)],
                           capture_output=True, text=True)
    finally:
        subprocess.run([BASH, POOL, "release", slot.replace("SimGolf_pool", "")], capture_output=True)
    log = r.stdout + r.stderr
    if "FIDREPORT" not in log:
        print(log[-4000:]); return 1
    rows = list(csv.DictReader(out.open(encoding="utf-8"), delimiter="\t"))
    by_lib = collections.Counter(row["library"].split("|")[0] + " / " + row["library"].split("|")[2] for row in rows)
    unnamed = [row for row in rows if re.match(r"(FUN|thunk_FUN)_", row["name"])]
    print(f"{a.program}: {len(rows)} functions with a FID match -> {out.relative_to(ROOT)}")
    for lib, n in by_lib.most_common(): print(f"  {n:5d}  {lib}")
    print(f"  still FUN_* in master: {len(unnamed)} matched, {sum(r['unique'] == '1' for r in unnamed)} with a unique name")
    return 0

if __name__ == "__main__":
    sys.exit(main())
