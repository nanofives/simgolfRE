"""S7.1: write 1_tick.patch / 1_capi.patch (the shared files' edits as unified diffs against HEAD, from the mirror
port/build/S7_1_head) and check each applies to HEAD with `git apply --check` in a scratch export.

    ~/bin/py -3.12 log/roadmap/S7/1_make_patches.py            (from the shared repo root)
"""
import difflib
import pathlib
import shutil
import subprocess
import sys
import tempfile

REPO = pathlib.Path(__file__).resolve().parents[3]
MIRROR = REPO / "port" / "build" / "S7_1_head"
OUT = REPO / "log" / "roadmap" / "S7"
PATCHES = {
    "1_tick.patch": ["port/src/sim/tick.cpp", "port/src/sim/tick_hook.h", "port/include/portsim/sim/tick.h"],
    "1_capi.patch": ["port/include/portsim_c.h", "port/src/portsim_c.cpp", "port/test/runner32/runner32.cpp"],
}
head = (MIRROR / "HEAD_COMMIT").read_text().strip()


def show(path):
    return subprocess.run(["git", "show", f"{head}:{path}"], cwd=REPO, capture_output=True, check=True).stdout.decode("utf-8")


ok = True
for name, files in PATCHES.items():
    text = ""
    for f in files:
        a = show(f).replace("\r\n", "\n").splitlines(keepends=True)
        b = (MIRROR / f).read_text(encoding="utf-8").replace("\r\n", "\n").splitlines(keepends=True)
        d = "".join(difflib.unified_diff(a, b, fromfile=f"a/{f}", tofile=f"b/{f}", n=3))
        if d:
            text += f"diff --git a/{f} b/{f}\n" + d
    (OUT / name).write_text(text, encoding="utf-8", newline="\n")
    tmp = pathlib.Path(tempfile.mkdtemp(prefix="s71_"))
    try:
        for f in files:
            p = tmp / f
            p.parent.mkdir(parents=True, exist_ok=True)
            p.write_text(show(f).replace("\r\n", "\n"), encoding="utf-8", newline="\n")
        subprocess.run(["git", "init", "-q"], cwd=tmp, check=True)
        r = subprocess.run(["git", "apply", "--check", "-v", str(OUT / name)], cwd=tmp, capture_output=True, text=True)
        print(name, "applies to", head[:8], "->", "OK" if r.returncode == 0 else "FAILED", r.stderr.strip()[-300:])
        ok = ok and r.returncode == 0
        r2 = subprocess.run(["git", "apply", "--check", str(OUT / name)], cwd=REPO, capture_output=True, text=True)
        print(name, "applies to the shared working tree ->", "OK" if r2.returncode == 0 else "conflicts: " + r2.stderr.strip()[-400:])
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
sys.exit(0 if ok else 1)
