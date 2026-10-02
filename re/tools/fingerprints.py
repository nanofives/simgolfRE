"""Export per-function fingerprints from a Ghidra program (via a pool slot) to JSONL.
    py -3.12 re/tools/fingerprints.py golf_clean.exe re/versions/v103.jsonl [--project-dir DIR --project NAME]"""
import argparse, json, pathlib, subprocess, sys
ROOT = pathlib.Path(__file__).resolve().parents[2]
HEADLESS = r"C:\Users\maria\Desktop\Proyectos\TD5RE\ghidra\ghidra_12.0.3_PUBLIC\support\analyzeHeadless.bat"
BASH = r"C:\Program Files\Git\bin\bash.exe"
POOL = (ROOT / "scripts" / "ghidra_pool.sh").as_posix()

def main():
    ap = argparse.ArgumentParser(); ap.add_argument("program"); ap.add_argument("out")
    ap.add_argument("--project-dir"); ap.add_argument("--project")
    a = ap.parse_args()
    slot = None
    if a.project_dir:
        pdir, pname = a.project_dir, a.project
    else:
        slot = subprocess.run([BASH, POOL, "acquire"], capture_output=True, text=True).stdout.strip().splitlines()[-1]
        pdir, pname = str(ROOT / "simgolf_pool"), slot
    try:
        r = subprocess.run([HEADLESS, pdir, pname, "-process", a.program, "-readOnly", "-noanalysis",
                            "-scriptPath", str(ROOT / "ghidra" / "scripts"), "-postScript", "ExportFingerprints.java"],
                           capture_output=True, text=True, encoding="utf-8", errors="replace")
    finally:
        if slot:
            subprocess.run([BASH, POOL, "release", slot.replace("SimGolf_pool", "")], capture_output=True)
    n = 0
    with open(a.out, "w", encoding="utf-8") as f:
        for line in r.stdout.splitlines():
            if "FP:{" in line:
                js = line.split("FP:", 1)[1].rsplit(" (GhidraScript)", 1)[0].strip()
                json.loads(js); f.write(js + "\n"); n += 1
    print(f"{n} functions -> {a.out}")
    if not n:
        print(r.stdout[-3000:], r.stderr[-2000:])

if __name__ == "__main__":
    sys.exit(main())
