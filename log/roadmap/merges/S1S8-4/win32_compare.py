"""I4: the sorted FAILED lines of the Win32 suite against I1's (log/roadmap/merges/S6-1/suite_win32/, HEAD + S6: 20 failures in 8 files).
Usage: ~/bin/py -3.12 log/roadmap/merges/S1S8-4/win32_compare.py"""
import pathlib
import sys


def failed(d):
    out = set()
    for f in pathlib.Path(d).glob("test_*.txt"):
        for l in f.read_text(encoding="utf-8", errors="replace").splitlines():
            if l.startswith("FAILED "):
                out.add(l.split(" - ")[0])
    return out


old, new = failed("log/roadmap/merges/S6-1/suite_win32"), failed("log/roadmap/merges/S1S8-4/suite_win32")
print("baseline failures", len(old), "new failures", len(new))
for l in sorted(new - old):
    print("NEW", l)
for l in sorted(old - new):
    print("GONE", l)
print("identical" if old == new else "DIFFERENT")
sys.exit(0 if old == new else 1)
