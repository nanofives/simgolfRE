"""S10.7: library-range functions of ours that stay unnamed after the identity rows (the FidDb question).
~/bin/py -3.12 log/roadmap/S10/7_tools/leftover.py > log/roadmap/S10/7_leftover.tsv
Range = lowest..highest library candidate address of 7_cand_<key>.tsv; a function of ours counts when its list name is FUN_ /
thunk_ and no re/names row (this step's included) has its address, and no non-library PDB procedure starts there."""
import glob, pathlib

ROOT = pathlib.Path(__file__).resolve().parents[4]
MODS = {"golf": ("golf_clean.exe", 0, "functions_ghidra.tsv"), "jgld": ("jgld.dll", 0x10000000, "functions_ghidra_jgld.dll.tsv"),
        "sound": ("sound.dll", 0x10000000, "functions_ghidra_sound.dll.tsv"), "Terrain": ("Terrain.dll", 0x10000000, "functions_ghidra_Terrain.dll.tsv")}
named = set()
for f in glob.glob(str(ROOT / "re/names/*.tsv")):
    if f.endswith("_globals.tsv") or f.endswith("withdrawn.tsv"):
        continue
    for l in open(f, encoding="utf-8").read().splitlines()[1:]:
        c = l.split("\t")
        if len(c) >= 3 and c[1].startswith("0x"):
            named.add((c[0], int(c[1], 16)))
print("module\tentry\tsize\tname\tinside lib range")
for key, (mod, base, fnf) in MODS.items():
    cand = [int(l.split("\t")[0], 16) for l in (ROOT / f"log/roadmap/S10/7_cand_{key}.tsv").read_text(encoding="utf-8").splitlines()[1:]]
    lo, hi = min(cand), max(cand)
    other = {int(l.split("\t")[0], 16) + base for l in (ROOT / f"log/roadmap/S10/7_pdb_{key}.procs.tsv").read_text(encoding="utf-8").splitlines()
             if l.startswith("0x") and "libc" not in l.split("\t")[5].lower()}
    n = 0
    for l in (ROOT / "re" / fnf).read_text(encoding="utf-8").splitlines()[1:]:
        c = l.split("\t")
        va = int(c[0], 16) + base
        if lo <= va <= hi and (c[1].startswith(("FUN_", "thunk_FUN_")) or c[1] == "") and (mod, va) not in named and va not in other:
            n += 1
            print(f"{mod}\t0x{va:08x}\t{c[2]}\t{c[1]}\t{lo:#x}..{hi:#x}")
    print(f"# {key}: {n} unnamed", file=__import__('sys').stderr)
