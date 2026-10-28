"""S3.11: compare two one-file-per-process suite dirs (each test_<x>.txt holds pytest -q output; the last line is the summary).

    ~/bin/py -3.12 log/roadmap/S3/11_compare_suite.py <old dir> <new dir>

One line per file: same / DIFF, old summary, new summary (summary without the timing)."""
import pathlib
import re
import sys


def summary(p: pathlib.Path) -> str:
    if not p.exists():
        return "(absent)"
    lines = [l for l in p.read_text(encoding="utf-8", errors="replace").splitlines() if l.strip()]
    if not lines:
        return "(empty)"
    s = lines[-1].strip().strip("=").strip()
    return re.sub(r"\s+in [0-9.]+s(\s*\([0-9:]+\))?$", "", s)


def main():
    old, new = pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2])
    names = sorted({p.stem for p in old.glob("test_*.txt")} | {p.stem for p in new.glob("test_*.txt")})
    for n in names:
        a, b = summary(old / f"{n}.txt"), summary(new / f"{n}.txt")
        print(f"{'same' if a == b else 'DIFF'} {n:28s} old: {a:44s} new: {b}")


if __name__ == "__main__":
    main()
