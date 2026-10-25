"""S2.13: write log/roadmap/S2/13_shared.patch and check it.

The patch base is the tree after waves 1-4 (port/build/S2_13_pre holds those files as make_mirror.py found them before the
S2.7-S2.9 layers); the patch target is the integration mirror port/build/S2_13_head. Files that are new in S2.7-S2.9 are not
in the patch (13_merge.md lists them for copying); the wave-4 file port/test/test_s2_6_scenario.py is (it exists in the
working tree).

Check: the working tree's copies of the patched files go to a scratch tree, 3_shared.patch and 4_shared.patch are applied
there (the files waves 2-3 add are copied from the mirror), the scratch files must then equal S2_13_pre's (the base is the
working tree + waves 1-4), and `git apply --check` + `git apply` of 13_shared.patch must give the mirror's files exactly.

usage: ~/bin/py -3.12 log/roadmap/S2/13_tools/make_patch.py <scratch dir>
"""
import difflib
import pathlib
import shutil
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[4]
B = ROOT / "port" / "build"
MIRROR, PRE = B / "S2_13_head", B / "S2_13_pre"
S2 = ROOT / "log" / "roadmap" / "S2"
OUT = S2 / "13_shared.patch"
EXTRA = ["port/test/test_s2_6_scenario.py"]   # changed by S2.13 itself, not by a step layer


def lf(p):
    return p.read_bytes().replace(b"\r\n", b"\n")


def main():
    scratch = pathlib.Path(sys.argv[1])
    touched = [l for l in (PRE / "TOUCHED.txt").read_text().split("\n") if l]
    files = [r for r in touched if (PRE / r).exists()] + EXTRA
    chunks = []
    for r in files:
        old = (PRE / r) if (PRE / r).exists() else (ROOT / r)
        a, b = lf(old).decode("utf-8"), lf(MIRROR / r).decode("utf-8")
        if a == b:
            continue
        d = difflib.unified_diff(a.splitlines(True), b.splitlines(True), f"a/{r}", f"b/{r}", n=3)
        # a last line without its newline gets git's marker
        body = "".join(l if l.endswith("\n") else l + "\n\\ No newline at end of file\n" for l in d)
        chunks.append(f"diff --git a/{r} b/{r}\n" + body)
    OUT.write_text("".join(chunks), encoding="utf-8", newline="\n")
    names = [c.split("\n", 1)[0].split(" b/", 1)[1] for c in chunks]
    print(f"wrote {OUT} ({len(chunks)} files): " + ", ".join(names))
    # the check tree
    if scratch.exists():
        shutil.rmtree(scratch)
    for r in names:
        (scratch / r).parent.mkdir(parents=True, exist_ok=True)
        if (ROOT / r).exists():
            (scratch / r).write_bytes(lf(ROOT / r))
    subprocess.run(["git", "init", "-q", str(scratch)], check=True)
    for p in (S2 / "3_shared.patch", S2 / "4_shared.patch"):
        txt = p.read_text(encoding="utf-8")
        files_in = [l[len("+++ b/"):].strip() for l in txt.split("\n") if l.startswith("+++ b/")]
        inc = [f for f in files_in if f in names]
        if not inc:
            print(f"{p.name}: touches none of the patched files")
            continue
        args = ["git", "-C", str(scratch), "apply"] + [f"--include={f}" for f in inc] + [str(p)]
        r = subprocess.run(args, capture_output=True, text=True)
        print(f"{p.name} on the working-tree copies ({', '.join(inc)}): rc {r.returncode} {r.stderr.strip()[:500]}")
    bad = [r for r in names if (PRE / r).exists() and lf(scratch / r) != lf(PRE / r)]
    print(f"working tree + waves 2-3 == the patch base: {'yes' if not bad else 'NO: ' + ', '.join(bad)}")
    r = subprocess.run(["git", "-C", str(scratch), "apply", "--check", str(OUT)], capture_output=True, text=True)
    print(f"git apply --check 13_shared.patch: rc {r.returncode} {r.stderr.strip()[:800]}")
    if r.returncode == 0:
        subprocess.run(["git", "-C", str(scratch), "apply", str(OUT)], check=True)
        bad = [n for n in names if lf(scratch / n) != lf(MIRROR / n)]
        print(f"applied: equal to the mirror: {'yes' if not bad else 'NO: ' + ', '.join(bad)}")
    # the same with the working tree's own bytes (CRLF under core.autocrlf=true) in a repository configured like this one
    crlf = pathlib.Path(str(scratch) + "_crlf")
    if crlf.exists():
        shutil.rmtree(crlf)
    subprocess.run(["git", "init", "-q", str(crlf)], check=True)
    subprocess.run(["git", "-C", str(crlf), "config", "core.autocrlf", "true"], check=True)
    shutil.copy2(ROOT / ".gitattributes", crlf / ".gitattributes")
    for r in names:
        if (ROOT / r).exists():
            (crlf / r).parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(ROOT / r, crlf / r)
    ok = True
    for p in (S2 / "3_shared.patch", S2 / "4_shared.patch", OUT):
        txt = p.read_text(encoding="utf-8")
        inc = [f for f in (l[len("+++ b/"):].strip() for l in txt.split("\n") if l.startswith("+++ b/")) if f in names]
        if not inc:
            continue
        r = subprocess.run(["git", "-C", str(crlf), "apply", "--ignore-whitespace"] + [f"--include={f}" for f in inc] + [str(p)],
                           capture_output=True, text=True)
        ok = ok and r.returncode == 0
        print(f"CRLF copy, git apply --ignore-whitespace {p.name}: rc {r.returncode} {r.stderr.strip()[:400]}")
    if ok:
        bad = [n for n in names if lf(crlf / n) != lf(MIRROR / n)]
        print(f"CRLF copy after all three: equal to the mirror (line ends aside): {'yes' if not bad else 'NO: ' + ', '.join(bad)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
