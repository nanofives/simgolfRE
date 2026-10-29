"""S10.8: master FUN_ functions of golf_clean.exe without a names row, with golf.pdb publics / procs at the same RVA."""
import glob, pathlib
R = pathlib.Path(__file__).resolve().parents[4]
rows = set()
for f in glob.glob(str(R / "re/names/*.tsv")):
    if f.endswith("_globals.tsv"):
        continue
    for l in open(f, encoding="utf-8").read().splitlines()[1:]:
        c = l.split("\t")
        if len(c) == 6 and c[0] == "golf_clean.exe":
            rows.add(int(c[1], 16))
pub = {}
for l in (R / "log/roadmap/S10/pdb/golf.publics.tsv").read_text(encoding="utf-8").splitlines():
    c = l.split("\t")
    if c[0].startswith("0x"):
        pub.setdefault(int(c[0], 16) + 0x400000, []).append(c[1])
procs = {}
for l in (R / "log/roadmap/S10/pdb/golf.procs.tsv").read_text(encoding="utf-8").splitlines():
    c = l.split("\t")
    if c[0].startswith("0x"):
        procs[int(c[0], 16) + 0x400000] = c[2]
n = 0
for l in (R / "re/functions_ghidra.tsv").read_text(encoding="utf-8").splitlines()[1:]:
    c = l.split("\t")
    va, name, size = int(c[0], 16), c[1], int(c[2])
    if va in rows:
        continue
    n += 1
    print(f"{va:#010x}\t{name}\t{size}\t{procs.get(va, '')}\t{' '.join(pub.get(va, []))}")
print("no-row functions:", n)
