"""S7.7: write 7_tick.patch and 7_capi.patch (the whole S7 change to the shared files, as unified diffs against HEAD 2c05f02a;
they replace S7.1's 1_tick.patch / 1_capi.patch and S7.6's 6_capi.patch) from the mirror port/build/S7_7_head, then check with
`git apply --check` that each applies to HEAD's files and to the shared working tree's current files (which hold other
sections' uncommitted edits, e.g. S3.2's runner32.cpp ops 28..32).

    ~/bin/py -3.12 log/roadmap/S7/7_make_patches.py            (from the shared repo root)
"""
import difflib
import pathlib
import shutil
import subprocess
import sys
import tempfile

REPO = pathlib.Path(__file__).resolve().parents[3]
MIRROR = REPO / "port" / "build" / "S7_7_head"
OUT = REPO / "log" / "roadmap" / "S7"
PATCHES = {
    "7_tick.patch": ["port/src/sim/tick.cpp", "port/src/sim/tick_hook.h", "port/include/portsim/sim/tick.h"],
    "7_capi.patch": ["port/include/portsim_c.h", "port/src/portsim_c.cpp", "port/test/runner32/runner32.cpp"],
}
head = (MIRROR / "HEAD_COMMIT").read_text().strip()


def show(path):
    return subprocess.run(["git", "show", f"{head}:{path}"], cwd=REPO, capture_output=True, check=True).stdout.decode("utf-8")


def lf(s):
    return s.replace("\r\n", "\n")


def check(tree_files, patch, label):
    tmp = pathlib.Path(tempfile.mkdtemp(prefix="s77_"))
    try:
        for f, text in tree_files.items():
            p = tmp / f
            p.parent.mkdir(parents=True, exist_ok=True)
            p.write_text(text, encoding="utf-8", newline="\n")
        subprocess.run(["git", "init", "-q"], cwd=tmp, check=True)
        r = subprocess.run(["git", "apply", "--check", str(OUT / patch)], cwd=tmp, capture_output=True, text=True)
        print(f"{patch} on {label}:", "OK" if r.returncode == 0 else "FAILED " + r.stderr[-400:])
        return r.returncode == 0
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


ok = True
for name, files in PATCHES.items():
    text = ""
    for f in files:
        a = lf(show(f)).splitlines(keepends=True)
        b = lf((MIRROR / f).read_text(encoding="utf-8")).splitlines(keepends=True)
        d = "".join(difflib.unified_diff(a, b, fromfile=f"a/{f}", tofile=f"b/{f}", n=3))
        if d:
            text += f"diff --git a/{f} b/{f}\n" + d
    (OUT / name).write_text(text, encoding="utf-8", newline="\n")
    print(name, len(text.splitlines()), "lines")
    ok = check({f: lf(show(f)) for f in files}, name, head[:8]) and ok
    ok = check({f: lf((REPO / f).read_text(encoding="utf-8")) for f in files}, name, "the shared working tree") and ok
sys.exit(0 if ok else 1)
