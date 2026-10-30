"""S3.12: does any committed file of a commit depend on content only the shared tree's uncommitted copies hold?

For every tracked code file the shared working tree has modified (git diff --name-only <commit>), the identifiers (4+ characters) in
the CODE of its working copy (comments and string literals stripped) that the commit's blob does not have are 'working-tree-only
names'. Each is searched in every other file of the commit under port/, re/tools, re/frida, tests (git grep -w); a hit counts only
when the name is in the code part of the committed line. Every hit is a candidate dependency and is printed with its lines.

    ~/bin/py -3.12 log/roadmap/S3/12_deps.py <commit> [...]
"""
import pathlib
import re
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
NAME = re.compile(r"\b[A-Za-z_][A-Za-z0-9_]{3,}\b")
CODE = (".cpp", ".h", ".inc", ".py", ".c", ".hpp")
STR = re.compile(r'"(?:\\.|[^"\\\n])*"' + "|" + r"'(?:\\.|[^'\\\n])*'")


def git(*a, ok=False):
    r = subprocess.run(["git", *a], cwd=ROOT, capture_output=True, text=True, encoding="utf-8", errors="replace")
    if r.returncode and not ok:
        raise SystemExit(r.stderr)
    return r.stdout


def code_only(text, py):
    """the text without comments and string literals (names in prose are not dependencies)"""
    if py:
        text = re.sub(r'""".*?"""', "", text, flags=re.S)
        text = STR.sub('""', text)
        return "\n".join(l.split("#", 1)[0] for l in text.splitlines())
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    text = STR.sub('""', text)
    return "\n".join(l.split("//", 1)[0] for l in text.splitlines())


def names(text, py):
    return set(NAME.findall(code_only(text, py)))


def main():
    for commit in sys.argv[1:] or ["HEAD"]:
        mod = [f for f in git("diff", "--name-only", commit).split() if f.endswith(CODE)]
        print(f"== {commit}: {len(mod)} code files modified in the working tree against it")
        hits = 0
        for f in mod:
            work = (ROOT / f).read_text(encoding="utf-8", errors="replace")
            blob = git("show", f"{commit}:{f}", ok=True)
            py = f.endswith(".py")
            new = sorted(names(work, py) - names(blob, py))
            print(f"  {f}: {len(new)} working-tree-only names")
            for n in new:
                out = git("grep", "-n", "-w", n, commit, "--", "port", "re/tools", "re/frida", "tests", ok=True)
                rows = []
                for l in out.splitlines():
                    if l.startswith(f"{commit}:{f}:"):
                        continue
                    path, _, rest = l[len(commit) + 1:].partition(":")
                    _, _, line = rest.partition(":")
                    if re.search(r"\b" + re.escape(n) + r"\b", code_only(line, path.endswith(".py"))):
                        rows.append(l)
                if rows:
                    hits += 1
                    print(f"    {n}: {len(rows)} committed code line(s) elsewhere")
                    for l in rows[:8]:
                        print("      " + l[:220])
        print(f"== {commit}: working-tree-only names used in committed code elsewhere: {hits}")


if __name__ == "__main__":
    main()
