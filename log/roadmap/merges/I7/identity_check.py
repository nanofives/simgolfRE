"""I7: byte identity between the shared tree and the mirror port/build/I7 for every section file mirror.py copies (S2 waves 6 / 7 new
files, S5's files, S7's live files, port/app and app tests, S7.5's files) and every shared file the merge edits (shared_edited.txt).
Prints eq / eqCR (equal up to line ends) / DIFF / MISSING per path and a summary.
Usage: ~/bin/py -3.12 log/roadmap/merges/I7/identity_check.py [--drop <wave>...]"""
import importlib.util
import pathlib
import sys

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parents[3]
M = ROOT / "port" / "build" / (sys.argv[sys.argv.index("--mirror") + 1] if "--mirror" in sys.argv else "I7")


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    m = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(m)
    return m


def main():
    a = sys.argv[1:]
    drop = set(a[a.index("--drop") + 1:]) if "--drop" in a else set()
    mir = load("i7_mirror", HERE / "mirror.py")
    rels = [r for r, _ in mir.plan(drop)] + [l.strip() for l in (HERE / "shared_edited.txt").read_text().splitlines() if l.strip()]
    seen, counts = set(), {}
    for rel in rels:
        if rel in seen:
            continue
        seen.add(rel)
        x, y = ROOT / rel, M / rel
        if not x.exists() or not y.exists():
            st = "MISSING" + ("" if x.exists() else " (shared)") + ("" if y.exists() else " (mirror)")
        elif x.read_bytes() == y.read_bytes():
            st = "eq"
        elif x.read_bytes().replace(b"\r\n", b"\n") == y.read_bytes().replace(b"\r\n", b"\n"):
            st = "eqCR"
        else:
            st = "DIFF"
        counts[st.split()[0]] = counts.get(st.split()[0], 0) + 1
        print(f"{st:8} {rel}")
    print("summary:", counts, "paths:", len(seen))


if __name__ == "__main__":
    main()
