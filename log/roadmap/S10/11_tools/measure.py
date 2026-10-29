"""S10.9: which functions do SMSG's PDBs name where we have no names row and the master says FUN_?
~/bin/py -3.12 log/roadmap/S10/9_tools/measure.py <sound|jgld|Terrain> [--out tsv]
Columns: va, size_ours, size_theirs, master name, their name, obj, has_names_row, hooks_level."""
import csv, glob, sys, pathlib

MODS = {"sound": "sound.dll", "jgld": "jgld.dll", "Terrain": "Terrain.dll"}
key = sys.argv[1]
mod = MODS[key]
root = pathlib.Path(__file__).resolve().parents[4]
procs = {}
for l in (root / f"log/roadmap/S10/pdb/{key}.procs.tsv").read_text(encoding="utf-8").splitlines():
    c = l.split("\t")
    if len(c) >= 4 and c[0].startswith("0x"):
        procs[int(c[0], 16)] = (int(c[1]), c[2], c[3])
master = {}
for l in (root / f"re/functions_ghidra_{mod}.tsv").read_text(encoding="utf-8").splitlines()[1:]:
    c = l.split("\t")
    master[int(c[0], 16)] = (c[1], int(c[2]))
named = {}
for f in glob.glob(str(root / "re/names/*.tsv")):
    if f.endswith("_globals.tsv") or f.endswith("withdrawn.tsv"):
        continue
    for l in open(f, encoding="utf-8").read().splitlines()[1:]:
        c = l.split("\t")
        if len(c) == 6 and c[0] == mod:
            named[int(c[1], 16) & 0xFFFFFF] = (c[2], pathlib.Path(f).name)
level = {}
with open(root / "hooks.csv", encoding="utf-8") as fh:
    for r in csv.DictReader(fh):
        if r["module"] == mod:
            level[int(r["addr"], 16)] = r["confidence"]
rows = []
for rva, (sz, nm, obj) in sorted(procs.items()):
    m = master.get(rva)
    if rva in named:
        continue
    mname = m[0] if m else "-"
    if m and not mname.startswith("FUN_"):
        continue
    rows.append((rva, m[1] if m else 0, sz, mname, nm, obj, level.get(rva, "-")))
out = [f"0x{0x10000000 + r[0]:08x}\t{r[1]}\t{r[2]}\t{r[3]}\t{r[4]}\t{r[5]}\t{r[6]}" for r in rows]
print(f"{mod}: procs {len(procs)}, named rows {len(named)}, candidates {len(rows)}"
      f" (in master as FUN_ {sum(1 for r in rows if r[3] != '-')}, not a master function {sum(1 for r in rows if r[3] == '-')})")
if "--out" in sys.argv:
    p = sys.argv[sys.argv.index("--out") + 1]
    pathlib.Path(p).write_text("va\tsize_ours\tsize_theirs\tmaster\ttheirs\tobj\tlevel\n" + "\n".join(out) + "\n", encoding="utf-8")
