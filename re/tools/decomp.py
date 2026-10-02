"""Headless decompile (fallback when the Ghidra MCP is not loaded in the session).
Usage: py -3.12 re/tools/decomp.py [--program golf_clean.exe] 0x00449110 [0x... ...]
Runs against a pool slot (acquired + released here), never the master."""
import argparse, pathlib, re, subprocess, sys
ROOT = pathlib.Path(__file__).resolve().parents[2]
HEADLESS = r"C:\Users\maria\Desktop\Proyectos\TD5RE\ghidra\ghidra_12.0.3_PUBLIC\support\analyzeHeadless.bat"
BASH = r"C:\Program Files\Git\bin\bash.exe"
POOL = (ROOT / "scripts" / "ghidra_pool.sh").as_posix()

def main():
    ap = argparse.ArgumentParser(); ap.add_argument("--program", default="golf_clean.exe"); ap.add_argument("addrs", nargs="+")
    a = ap.parse_args()
    slot = subprocess.run([BASH, POOL, "acquire"], capture_output=True, text=True).stdout.strip().splitlines()[-1]
    try:
        r = subprocess.run([HEADLESS, str(ROOT / "simgolf_pool"), slot, "-process", a.program, "-readOnly", "-noanalysis",
                            "-scriptPath", str(ROOT / "ghidra" / "scripts"), "-postScript", "Decompile.java", *a.addrs],
                           capture_output=True, text=True)
    finally:
        subprocess.run([BASH, POOL, "release", slot.replace("SimGolf_pool", "")], capture_output=True)
    out, keep = [], False
    for line in r.stdout.splitlines():
        line = re.sub(r"^INFO  Decompile\.java> ", "", line).replace(" (GhidraScript)  ", "")
        if line.startswith("DECOMP-BEGIN"): keep = True; out.append(f"// ===== {line.split()[1]} ({a.program})"); continue
        if line.startswith("DECOMP-END"): keep = False; continue
        if keep and not line.startswith("INFO "): out.append(line)
    print("\n".join(out) if out else r.stdout[-3000:])

if __name__ == "__main__":
    sys.exit(main())
