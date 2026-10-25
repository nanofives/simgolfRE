"""I5: per-file summary lines of a suite run against I4's (HEAD + S1 + S8), and the sorted FAILED lines of both.
Usage: ~/bin/py -3.12 log/roadmap/merges/S2S3-5/suite_compare.py <new dir> <old dir>"""
import pathlib
import sys

new, old = pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2])


def summ(d):
    out, fails = {}, set()
    for f in sorted(d.glob("test_*.txt")):
        if ".rerun" in f.name:
            continue
        lines = f.read_text(encoding="utf-8", errors="replace").splitlines()
        out[f.stem] = lines[-1] if lines else "(empty)"
        fails |= {l.split(" - ")[0] for l in lines if l.startswith("FAILED ")}
    return out, fails


n, nf = summ(new)
o, of = summ(old)
for k in sorted(set(n) | set(o)):
    a, b = o.get(k, "(absent)"), n.get(k, "(absent)")
    strip = lambda s: s.split(" in ")[0]
    print(("same " if strip(a) == strip(b) else "DIFF ") + f"{k:28} old: {strip(a):40} new: {strip(b)}")
print("FAILED old", len(of), "new", len(nf))
for l in sorted(nf - of):
    print("NEW ", l)
for l in sorted(of - nf):
    print("GONE", l)
