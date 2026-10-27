"""I10 (S7 waves 1-3 resubmission, log/roadmap/S7/9_merge.md): the mirror port/build/I10 = `git archive 78ffb06d` + the junctions
of log/roadmap/S0/export.md plus shim/build, re/replay and extracted (as I4..I9). S7's files and edits are then applied with
`~/bin/py -3.12 log/roadmap/S7/9_apply.py port/build/I10 --files` (run separately, logged to apply_mirror.log).

    ~/bin/py -3.12 log/roadmap/merges/I10/mirror.py [--dst <dir under port/build>]
"""
import os
import pathlib
import shutil
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[4]
B = ROOT / "port" / "build"
HEAD = "78ffb06d"
JUNCTIONS = ["original", "log", "port/corpus", "tests/golden", "tests/fixtures", "re/versions", "tools/vc6", "shim/build",
             "re/replay", "extracted"]


def main():
    a = sys.argv[1:]
    dst = B / (a[a.index("--dst") + 1] if "--dst" in a else "I10")
    if (dst / "port" / "CMakeLists.txt").exists():
        sys.exit(f"{dst} exists")
    dst.mkdir(parents=True, exist_ok=True)
    tar = subprocess.run(["git", "archive", HEAD], cwd=ROOT, capture_output=True, check=True).stdout
    subprocess.run(["tar", "-xf", "-"], cwd=dst, input=tar, check=True)
    full = subprocess.run(["git", "rev-parse", HEAD], cwd=ROOT, capture_output=True, text=True, check=True).stdout.strip()
    (dst / "HEAD_COMMIT").write_text(full + "\n")
    for j in JUNCTIONS:
        link = dst / j
        if link.exists() and not os.path.isjunction(link):   # tracked parts from the archive: the shared dir holds them unmodified
            assert dst in link.resolve().parents, link
            shutil.rmtree(link)
            print("replaced archive dir by a junction:", j)
        if not link.exists():
            if not (ROOT / j).exists():
                print("no shared dir, skipped junction", j)
                continue
            link.parent.mkdir(parents=True, exist_ok=True)
            subprocess.run(["cmd", "/c", "mklink", "/J", str(link), str(ROOT / j)], check=True, capture_output=True)
            print("junction", j)
    print("created", dst, full)


if __name__ == "__main__":
    main()
