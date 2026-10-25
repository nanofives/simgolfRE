"""I4 (S1 + S8 merge): the mirror port/build/I4_S1S8 = git archive of HEAD + the junctions of log/roadmap/S0/export.md (plus
shim/build and re/replay) + every non-log path of log/roadmap/S1/files.txt and log/roadmap/S8/files.txt copied from the shared tree.
The shared-file edits are applied afterwards by apply_shared.py. Refuses to run over an existing mirror unless --recopy (then only the
section files are copied again).
Usage: ~/bin/py -3.12 log/roadmap/merges/S1S8-4/mirror.py [--recopy]"""
import os
import pathlib
import shutil
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[4]
DST = ROOT / "port" / "build" / "I4_S1S8"
JUNCTIONS = ["original", "log", "port/corpus", "tests/golden", "tests/fixtures", "re/versions", "tools/vc6", "shim/build", "re/replay", "extracted"]
LISTS = [ROOT / "log/roadmap/S1/files.txt", ROOT / "log/roadmap/S8/files.txt"]


def section_files():
    out = []
    for lst in LISTS:
        for l in lst.read_text().splitlines():
            l = l.strip()
            if l and not l.startswith("log/") and not l.endswith("/"):
                out.append(l)
    return out


def main():
    recopy = "--recopy" in sys.argv[1:]
    if (DST / "port" / "CMakeLists.txt").exists() and not recopy:
        sys.exit(f"{DST} exists (use --recopy)")
    if not recopy:
        DST.mkdir(parents=True, exist_ok=True)
        head = subprocess.run(["git", "rev-parse", "HEAD"], cwd=ROOT, capture_output=True, text=True, check=True).stdout.strip()
        tar = subprocess.run(["git", "archive", head], cwd=ROOT, capture_output=True, check=True).stdout
        subprocess.run(["tar", "-xf", "-"], cwd=DST, input=tar, check=True)
        (DST / "HEAD_COMMIT").write_text(head + "\n")
        for j in JUNCTIONS:
            link = DST / j
            if link.exists() and not os.path.isjunction(link):
                assert DST in link.resolve().parents, link   # the archive's tracked copy, inside DST only
                shutil.rmtree(link)
            if not link.exists():
                if not (ROOT / j).exists():
                    print("no shared dir, skipped junction", j)
                    continue
                link.parent.mkdir(parents=True, exist_ok=True)
                subprocess.run(["cmd", "/c", "mklink", "/J", str(link), str(ROOT / j)], check=True, capture_output=True)
                print("junction", j)
    for rel in section_files():
        src = ROOT / rel
        assert src.is_file(), rel
        d = DST / rel
        d.parent.mkdir(parents=True, exist_ok=True)
        d.write_bytes(src.read_bytes())
        print("copied", rel)


if __name__ == "__main__":
    main()
