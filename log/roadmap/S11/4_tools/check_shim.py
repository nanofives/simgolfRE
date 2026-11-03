"""S11.4: prove that the shim edits change comments only: for every shim/src file that differs between the base tree and the
edited tree, the code and string segments (lexrename.cpp_segments) must be identical, segment for segment.

    ~/bin/py -3.12 log/roadmap/S11/4_tools/check_shim.py [--base port/build/I22] [--tree port/build/S11_4]
"""
from __future__ import annotations

import pathlib
import sys

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parents[3]
sys.path.insert(0, str(HERE))
import lexrename as LX  # noqa: E402


def main() -> None:
    a = sys.argv
    base = ROOT / (a[a.index("--base") + 1] if "--base" in a else "port/build/I22")
    tree = ROOT / (a[a.index("--tree") + 1] if "--tree" in a else "port/build/S11_4")
    changed = bad = 0
    for p in sorted((tree / "shim" / "src").rglob("*")):
        if not p.is_file():
            continue
        rel = p.relative_to(tree)
        q = base / rel
        t1 = q.read_bytes().decode("utf-8").replace("\r\n", "\n")
        t2 = p.read_bytes().decode("utf-8").replace("\r\n", "\n")
        if t1 == t2:
            continue
        changed += 1
        s1 = [(k, s) for k, s in LX.cpp_segments(t1) if k != "comment"]
        s2 = [(k, s) for k, s in LX.cpp_segments(t2) if k != "comment"]
        if s1 != s2:
            bad += 1
            print("CODE CHANGED", rel)
        n1 = len([1 for k, _ in LX.cpp_segments(t1) if k == "comment"])
        n2 = len([1 for k, _ in LX.cpp_segments(t2) if k == "comment"])
        if n1 != n2:
            bad += 1
            print("COMMENT COUNT CHANGED", rel, n1, n2)
    print(f"shim files changed {changed}, with a code or string change {bad}")
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
