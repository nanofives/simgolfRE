"""S1.6 private build tree: port/ mirrored into port/build/S1_6_root/port (and re/model/include beside it).

Tracked port/ files that differ from HEAD are taken at HEAD, except the own files of S1.4 and S1.5 (OWN: working tree), then
patches6.PATCHES (S1.5 + S1.4 + comments) are applied. S16_BASE=1 builds HEAD alone (S1_6_base_root).
Also writes log/roadmap/S1/tick_combined.diff (unified, paths port/..., CRLF kept) unless S16_BASE=1."""
import difflib
import os
import pathlib
import shutil
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[4]
BASE = os.environ.get("S16_BASE") == "1"
SRC = ROOT / "port"
DST = SRC / "build" / ("S1_6_base_root" if BASE else "S1_6_root") / "port"
MODEL = ROOT / "re" / "model" / "include"
OWN = set() if BASE else {"src/sim/chareditor.cpp", "test/test_chareditor.py", "test/golden_chareditor.json",
                          "test/test_chareditor_bio.py", "test/test_s1_3_scenarios.py", "test/test_s1_4_scenarios.py"}
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import patches6  # noqa: E402


def git(*a, binary=False):
    r = subprocess.run(["git", *a], cwd=ROOT, capture_output=True, check=True)
    return r.stdout if binary else r.stdout.decode()


tracked = set(l[len("port/"):] for l in git("ls-files", "--", "port/").split())
changed = set(l[len("port/"):] for l in git("diff", "--name-only", "HEAD", "--", "port/").split())
seen = set()
diffs = []
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
        new = patches6.apply(rel, data)
        if new != data and rel in patches6.PATCHES:
            diffs.append((rel, data, new))
        data = new
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
if not BASE:
    out = []
    for rel, old, new in diffs:
        a = old.decode("utf-8").splitlines(keepends=True)
        b = new.decode("utf-8").splitlines(keepends=True)
        out.extend(difflib.unified_diff(a, b, "a/port/" + rel, "b/port/" + rel, n=3))
    (ROOT / "log" / "roadmap" / "S1" / "tick_combined.diff").write_bytes("".join(out).encode("utf-8"))
print("mirror:", DST, "patched:", patches6.names())
