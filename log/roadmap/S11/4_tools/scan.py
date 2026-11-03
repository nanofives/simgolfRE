"""S11.4: where the old names of 4_renames.tsv occur in a tree (whole word; for `Class::method` also `Class_method` and the
bare method part). Junctions / symlinks are not followed (they point at the shared data).

    ~/bin/py -3.12 log/roadmap/S11/4_tools/scan.py --root <tree> [--new] [--dirs port,shim,re/frida,re/model,tests,scripts]
        -> per-file counts (tsv on stdout)
"""
from __future__ import annotations

import csv
import os
import pathlib
import re
import sys

HERE = pathlib.Path(__file__).resolve().parent
TEXT = {".cpp", ".h", ".inc", ".py", ".js", ".json", ".tsv", ".md", ".txt", ".c", ".hpp", ".bat", ".ps1", ".sh", ".csv", ".ini",
        ".cmake", ".in"}


def variants(name: str) -> list[str]:
    v = [name]
    if "::" in name:
        cls, meth = name.split("::", 1)
        v += [f"{cls}_{meth}", meth]
    return v


def walk(root: pathlib.Path, rel: str):
    base = root / rel
    if not base.exists():
        return
    stack = [base]
    while stack:
        d = stack.pop()
        if d.is_symlink() or (hasattr(os.path, "isjunction") and os.path.isjunction(d)):
            continue
        if d.is_file():
            yield d
            continue
        for e in sorted(d.iterdir()):
            if e.is_symlink() or os.path.isjunction(e):
                continue
            if e.is_dir():
                if e.name in ("build", "__pycache__", ".pytest_cache", "corpus") and rel.startswith("port"):
                    if e.parent == root / "port":
                        continue
                if e.name in ("__pycache__",):
                    continue
                stack.append(e)
            elif e.suffix.lower() in TEXT:
                yield e


def main() -> None:
    a = sys.argv
    root = pathlib.Path(a[a.index("--root") + 1])
    dirs = (a[a.index("--dirs") + 1] if "--dirs" in a else "port,shim,re/frida,re/model,re/tools,re/port,tests,scripts").split(",")
    col = "new" if "--new" in a else "old"
    rows = list(csv.DictReader(open(HERE.parent / "4_renames.tsv", encoding="utf-8"), delimiter="\t"))
    pats = []
    for r in rows:
        for v in variants(r[col]):
            pats.append((r[col], v, re.compile(r"(?<![A-Za-z0-9_])" + re.escape(v) + r"(?![A-Za-z0-9_])")))
    print("file\tname\tvariant\tcount")
    for d in dirs:
        for f in walk(root, d):
            try:
                t = f.read_text(encoding="utf-8", errors="replace")
            except OSError:
                continue
            for name, v, p in pats:
                n = len(p.findall(t))
                if n:
                    print(f"{f.relative_to(root).as_posix()}\t{name}\t{v}\t{n}")


if __name__ == "__main__":
    main()
