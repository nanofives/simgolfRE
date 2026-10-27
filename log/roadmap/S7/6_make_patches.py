"""S7.6: write 6_capi.patch (this step's edits of the C ABI files, as a unified diff over S7.1's 1_capi.patch applied to HEAD)
from the mirror port/build/S7_6_head, and check in a scratch git tree that 1_capi.patch then 6_capi.patch apply to HEAD's files
(`git apply --check`), and that the pair applies to the shared working tree.

    ~/bin/py -3.12 log/roadmap/S7/6_make_patches.py            (from the shared repo root)
"""
import difflib
import pathlib
import shutil
import subprocess
import sys
import tempfile

REPO = pathlib.Path(__file__).resolve().parents[3]
MIRROR = REPO / "port" / "build" / "S7_6_head"
OUT = REPO / "log" / "roadmap" / "S7"
FILES = ["port/include/portsim_c.h", "port/src/portsim_c.cpp", "port/test/runner32/runner32.cpp"]
head = (MIRROR / "HEAD_COMMIT").read_text().strip()


def show(path):
    return subprocess.run(["git", "show", f"{head}:{path}"], cwd=REPO, capture_output=True, check=True).stdout.decode("utf-8")


tmp = pathlib.Path(tempfile.mkdtemp(prefix="s76_"))
ok = True
try:
    for f in FILES:
        p = tmp / f
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(show(f).replace("\r\n", "\n"), encoding="utf-8", newline="\n")
    subprocess.run(["git", "init", "-q"], cwd=tmp, check=True)
    r = subprocess.run(["git", "apply", str(OUT / "1_capi.patch")], cwd=tmp, capture_output=True, text=True)
    print("1_capi.patch on", head[:8], "->", "OK" if r.returncode == 0 else "FAILED " + r.stderr[-300:])
    ok = ok and r.returncode == 0
    text = ""
    for f in FILES:
        a = (tmp / f).read_text(encoding="utf-8").splitlines(keepends=True)
        b = (MIRROR / f).read_text(encoding="utf-8").replace("\r\n", "\n").splitlines(keepends=True)
        d = "".join(difflib.unified_diff(a, b, fromfile=f"a/{f}", tofile=f"b/{f}", n=3))
        if d:
            text += f"diff --git a/{f} b/{f}\n" + d
    (OUT / "6_capi.patch").write_text(text, encoding="utf-8", newline="\n")
    print("6_capi.patch:", len(text.splitlines()), "lines")
    r = subprocess.run(["git", "apply", "--check", "-v", str(OUT / "6_capi.patch")], cwd=tmp, capture_output=True, text=True)
    print("6_capi.patch after 1_capi.patch ->", "OK" if r.returncode == 0 else "FAILED " + r.stderr[-300:])
    ok = ok and r.returncode == 0
    # the shared working tree: both in sequence, checked on a copy of its current files
    tree = pathlib.Path(tempfile.mkdtemp(prefix="s76_tree_"))
    try:
        for f in FILES:
            p = tree / f
            p.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(REPO / f, p)
        subprocess.run(["git", "init", "-q"], cwd=tree, check=True)
        r1 = subprocess.run(["git", "apply", "--ignore-whitespace", str(OUT / "1_capi.patch")], cwd=tree, capture_output=True, text=True)
        r2 = subprocess.run(["git", "apply", "--ignore-whitespace", "--check", str(OUT / "6_capi.patch")], cwd=tree,
                            capture_output=True, text=True)
        print("shared working tree: 1_capi ->", "OK" if r1.returncode == 0 else r1.stderr[-300:], "; 6_capi ->",
              "OK" if r2.returncode == 0 else r2.stderr[-300:])
    finally:
        shutil.rmtree(tree, ignore_errors=True)
finally:
    shutil.rmtree(tmp, ignore_errors=True)
sys.exit(0 if ok else 1)
