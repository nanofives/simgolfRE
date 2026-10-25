"""I6: byte identity between the shared tree and the mirror port/build/I6b for every non-log path of the merged files lists
(S6 files.txt, S3 files_w3.txt, S2 files_wave5.txt; instances/, *_img* and *.tsv.d entries left out) and every shared file the
merge edits (shared_edited.txt). Prints eq / eqCR (equal up to line ends) / DIFF / MISSING per path and a summary.
Usage: ~/bin/py -3.12 log/roadmap/merges/I6/identity_check.py [--sections-only]"""
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[4]
M = ROOT / "port" / "build" / "I6b"
RM = ROOT / "log" / "roadmap"
LISTS = [RM / "S6" / "files.txt", RM / "S3" / "files_w3.txt", RM / "S2" / "files_wave5.txt"]


def paths(sections_only):
    out = []
    for lst in LISTS:
        for l in lst.read_text().splitlines():
            l = l.strip()
            if l and not l.startswith(("log/", "instances/")) and not l.endswith("/") and "_img" not in l and ".tsv.d" not in l:
                out.append(l)
    if not sections_only:
        out += [l.strip() for l in (RM / "merges" / "I6" / "shared_edited.txt").read_text().splitlines() if l.strip()]
    seen, res = set(), []
    for p in out:
        if p not in seen:
            seen.add(p)
            res.append(p)
    return res


def main():
    counts = {}
    for rel in paths("--sections-only" in sys.argv):
        a, b = ROOT / rel, M / rel
        if not a.exists() or not b.exists():
            st = "MISSING" + ("" if a.exists() else " (shared)") + ("" if b.exists() else " (mirror)")
        elif a.read_bytes() == b.read_bytes():
            st = "eq"
        elif a.read_bytes().replace(b"\r\n", b"\n") == b.read_bytes().replace(b"\r\n", b"\n"):
            st = "eqCR"
        else:
            st = "DIFF"
        counts[st.split()[0]] = counts.get(st.split()[0], 0) + 1
        print(f"{st:8} {rel}")
    print("summary:", counts)


if __name__ == "__main__":
    main()
