"""I16: apply log/roadmap/merges/I16/renames.tsv (file, addr, old, new) to re/names/<file>: the name column (index 2) of the
row whose addr matches, only when it holds `old`. Line endings kept. Prints the touched files.   [--write]"""
import pathlib, sys
from collections import defaultdict
R = pathlib.Path(__file__).resolve().parents[4]
todo = defaultdict(dict)
for l in (R / "log/roadmap/merges/I16/renames.tsv").read_text(encoding="utf-8").splitlines()[1:]:
    f, a, o, n, _ = l.split("\t")
    todo[f][int(a, 16)] = (o, n)
bad, files = 0, []
for f, m in todo.items():
    p = R / "re/names" / f
    raw = p.read_bytes().decode("utf-8")
    nl = "\r\n" if "\r\n" in raw else "\n"
    lines = raw.split(nl)
    done = set()
    for i, l in enumerate(lines):
        c = l.split("\t")
        if len(c) < 3 or not c[1].startswith("0x"):
            continue
        a = int(c[1], 16)
        if a in m:
            o, n = m[a]
            if c[2] != o:
                print(f"MISMATCH {f} {c[1]}: has {c[2]}, expected {o}"); bad += 1; continue
            c[2] = n; lines[i] = "\t".join(c); done.add(a)
    for a in set(m) - done:
        print(f"MISSING {f} 0x{a:08x}"); bad += 1
    files.append(f"re/names/{f}")
    if "--write" in sys.argv and not bad:
        p.write_bytes(nl.join(lines).encode("utf-8"))
print("files:", " ".join(files)); print("bad", bad)
