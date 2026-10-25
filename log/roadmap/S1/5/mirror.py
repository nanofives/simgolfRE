"""S1.5 private build tree: port/ mirrored into port/build/S1_5_root/port (and re/model/include beside it).

Every tracked file of port/ that differs from HEAD is taken at HEAD (siblings' in-flight edits never reach this build),
except this step's own files (OWN), which are taken from the working tree; untracked files of port/ are skipped except
OWN. PATCHES then applies this step's shared-file changes (log/roadmap/S1/5_merge.md) to the mirrored copies.
Usage: mirror.py   (prints the files that differ from HEAD in the mirror)"""
import pathlib
import shutil
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[4]
import os
BASE = os.environ.get("S15_BASE") == "1"   # the HEAD tree alone (no own files, no patches): the gates' comparison build
SRC, DST = ROOT / "port", ROOT / "port" / "build" / ("S1_5_base_root" if BASE else "S1_5_root") / "port"
MODEL = ROOT / "re" / "model" / "include"
OWN = set() if BASE else {"src/sim/chareditor.cpp", "test/test_chareditor.py", "test/golden_chareditor.json",
                          "test/test_chareditor_bio.py"}
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import patches  # noqa: E402  (log/roadmap/S1/5/patches.py: the shared-file edits of 5_merge.md)


def git(*a, binary=False):
    r = subprocess.run(["git", *a], cwd=ROOT, capture_output=True, check=True)
    return r.stdout if binary else r.stdout.decode()


tracked = set(l[len("port/"):] for l in git("ls-files", "--", "port/").split())
changed = set(l[len("port/"):] for l in git("diff", "--name-only", "HEAD", "--", "port/").split())
seen = set()
for rel in sorted(tracked | OWN):
    if rel.startswith(("build/", "corpus/")) or "__pycache__" in rel:
        continue
    src = SRC / rel
    if rel in OWN:
        if not src.exists():
            continue
        data = src.read_bytes()
    elif rel in changed:
        try:
            data = git("show", "HEAD:port/" + rel, binary=True)
        except subprocess.CalledProcessError:
            continue
    else:
        if not src.exists():
            continue
        data = src.read_bytes()
    if not BASE:
        data = patches.apply(rel, data)
    d = DST / rel
    seen.add(d)
    d.parent.mkdir(parents=True, exist_ok=True)
    if not d.exists() or d.read_bytes() != data:
        d.write_bytes(data)
for p in MODEL.rglob("*"):
    if p.is_file():
        d = DST.parent / "re" / "model" / "include" / p.relative_to(MODEL)
        d.parent.mkdir(parents=True, exist_ok=True)
        if not d.exists() or d.read_bytes() != p.read_bytes():
            shutil.copy2(p, d)
for d in list(DST.rglob("*")):
    if d.is_file() and d not in seen and "build" not in d.relative_to(DST).parts[:1]:
        d.unlink()
print("mirror:", DST, "own:", sorted(OWN & {r for r in tracked | OWN if (SRC / r).exists()}), "patched:", patches.names())
