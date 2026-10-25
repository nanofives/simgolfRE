"""S2.13: list the files that differ between two trees (mirror vs mirror, or mirror vs a git commit).

usage: ~/bin/py -3.12 log/roadmap/S2/13_tools/scan.py <tree B> (<tree A> | git:<commit>)
Walks B and A, skipping junctions, symlinks, port/build, __pycache__ and .pytest_cache; prints `new`, `mod` or `gone` per path.
"""
import os
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[4]
SKIP_DIRS = {"__pycache__", ".pytest_cache", ".git"}
SKIP_REL = {"port/build", "log", "original", "port/corpus", "tests/golden", "tests/fixtures", "re/versions", "tools/vc6",
            "re/replay", "shim/build", "ghidra", "SimGolf.rep", "iso", "extracted", "instances", "re/fid"}


def walk(tree, prefix=""):
    out = {}
    tree = pathlib.Path(tree)
    for dp, dns, fns in os.walk(tree):
        d = pathlib.Path(dp)
        rel = d.relative_to(tree).as_posix()
        keep = []
        for n in dns:
            p = d / n
            r = prefix + ((rel + "/" + n) if rel != "." else n)
            if n in SKIP_DIRS or r in SKIP_REL or os.path.islink(p) or os.path.isjunction(p):
                continue
            keep.append(n)
        dns[:] = keep
        for n in fns:
            r = (rel + "/" + n) if rel != "." else n
            out[r] = d / n
    return out


def git_files(commit):
    names = subprocess.run(["git", "-C", str(ROOT), "ls-tree", "-r", "--name-only", commit], capture_output=True,
                           text=True, check=True).stdout.split("\n")
    return {n for n in names if n and not any(n == s or n.startswith(s + "/") for s in SKIP_REL)}


def git_bytes(commit, path):
    return subprocess.run(["git", "-C", str(ROOT), "show", f"{commit}:{path}"], capture_output=True, check=True).stdout


def main():
    b = walk(sys.argv[1])
    a_spec = sys.argv[2]
    if a_spec.startswith("git:"):
        commit = a_spec[4:]
        a_names = git_files(commit)
        get_a = lambda p: git_bytes(commit, p)
    else:
        a = walk(a_spec)
        a_names = set(a)
        get_a = lambda p: a[p].read_bytes()
    for p in sorted(set(b) | a_names):
        if p not in a_names:
            print("new\t" + p)
        elif p not in b:
            print("gone\t" + p)
        else:
            if b[p].read_bytes().replace(b"\r\n", b"\n") != get_a(p).replace(b"\r\n", b"\n"):
                print("mod\t" + p)


if __name__ == "__main__":
    main()
