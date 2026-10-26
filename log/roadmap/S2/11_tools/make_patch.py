"""S2.11: write log/roadmap/S2/11_shared.patch (the edits to files that exist in the base) and list the new files.

Base: port/build/S2_13_head (HEAD 2c05f02a + S2 waves 1-5, the S2.13 integration mirror); target: port/build/S2_11_head.
Every file under port/ (build dirs and the port/corpus junction excluded) is compared; files that differ go into the patch,
files only in the target are listed as new (copied by the integrator, 11_merge.md section 1). Check: the base's copies of the
patched files in a scratch git tree, `git apply --check` and `git apply`, each result equal to the mirror's file.

usage: ~/bin/py -3.12 log/roadmap/S2/11_tools/make_patch.py <scratch dir> [--wave6]
--wave6 (the lead's request after S2.10 became wave 6): base port/build/S2_10_head (S2_13_head + S2.10), target the combined mirror
port/build/S2_11w6_head (S2_10_head + this step's edits, 3-way merged; gfx_families.inc: wave 6's two lines, then this step's three).
"""
import difflib
import pathlib
import shutil
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[4]
B = ROOT / "port" / "build"
BASE, MIRROR = B / "S2_13_head", B / "S2_11_head"
OUT = ROOT / "log" / "roadmap" / "S2" / "11_shared.patch"
SKIP = ("port/build", "port/corpus")


def lf(p):
    return p.read_bytes().replace(b"\r\n", b"\n")


def files(root):
    out = set()
    for p in (root / "port").rglob("*"):
        r = p.relative_to(root).as_posix()
        if any(r == s or r.startswith(s + "/") for s in SKIP) or "__pycache__" in r or not p.is_file():
            continue
        out.add(r)
    return out


def main():
    global BASE, MIRROR
    scratch = pathlib.Path(sys.argv[1])
    if "--wave6" in sys.argv:
        BASE, MIRROR = B / "S2_10_head", B / "S2_11w6_head"
    fb, fm = files(BASE), files(MIRROR)
    new = sorted(fm - fb)
    gone = sorted(fb - fm)
    chunks = []
    for r in sorted(fb & fm):
        a, b = lf(BASE / r), lf(MIRROR / r)
        if a == b:
            continue
        d = difflib.unified_diff(a.decode("utf-8").splitlines(True), b.decode("utf-8").splitlines(True), f"a/{r}", f"b/{r}", n=3)
        body = "".join(l if l.endswith("\n") else l + "\n\\ No newline at end of file\n" for l in d)
        chunks.append((r, f"diff --git a/{r} b/{r}\n" + body))
    OUT.write_text("".join(c for _r, c in chunks), encoding="utf-8", newline="\n")
    names = [r for r, _c in chunks]
    print(f"wrote {OUT} ({len(names)} files): " + ", ".join(names))
    print("new files: " + ", ".join(new))
    print("removed files: " + (", ".join(gone) or "none"))
    if scratch.exists():
        shutil.rmtree(scratch)
    for r in names:
        (scratch / r).parent.mkdir(parents=True, exist_ok=True)
        (scratch / r).write_bytes(lf(BASE / r))
    subprocess.run(["git", "init", "-q", str(scratch)], check=True)
    r = subprocess.run(["git", "-C", str(scratch), "apply", "--check", str(OUT)], capture_output=True, text=True)
    print(f"git apply --check: rc {r.returncode} {r.stderr.strip()[:800]}")
    if r.returncode == 0:
        subprocess.run(["git", "-C", str(scratch), "apply", str(OUT)], check=True)
        bad = [n for n in names if lf(scratch / n) != lf(MIRROR / n)]
        print(f"applied: equal to the mirror: {'yes' if not bad else 'NO: ' + ', '.join(bad)}")


if __name__ == "__main__":
    main()
