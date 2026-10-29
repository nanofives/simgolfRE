"""S10.8: diff golf.pdb procedure names against our names rows, master list and hooks.csv.
~/bin/py -3.12 log/roadmap/S10/8_tools/diff.py  -> log/roadmap/S10/8_diff.tsv"""
import csv, glob, re, pathlib
R = pathlib.Path(__file__).resolve().parents[4]
MOD = "golf_clean.exe"
GENERIC = re.compile(r"^(FUN_|thunk_|LAB_)|[_A-Za-z]([0-9a-f]{6})$|^(dynInit|atexit|exitDtor|gSurfaceDtor|initGlobalArray|staticInit)_|"
                     r"Slot\d+|slot\d+|fwd|Vfn|_adj[0-9a-f]+|call[0-9a-f]{2}\b|field[0-9a-f]+|Field[0-9a-f]+|^unk|_[0-9a-f]{5,}")

def theirs():
    out = {}
    for l in (R / "log/roadmap/S10/pdb/golf.procs.tsv").read_text(encoding="utf-8").splitlines():
        c = l.split("\t")
        if len(c) >= 4 and c[0].startswith("0x"):
            out[int(c[0], 16) + 0x400000] = (c[2], int(c[1]), c[3])
    return out

def ours():
    rows = {}
    for f in sorted(glob.glob(str(R / "re/names/*.tsv"))):
        if f.endswith("_globals.tsv"):
            continue
        for l in open(f, encoding="utf-8").read().splitlines()[1:]:
            c = l.split("\t")
            if len(c) == 6 and c[0] == MOD:
                rows[int(c[1], 16)] = (c[2], pathlib.Path(f).name)
    master = {}
    for l in (R / "re/functions_ghidra.tsv").read_text(encoding="utf-8").splitlines()[1:]:
        c = l.split("\t")
        master[int(c[0], 16)] = (c[1], int(c[2]))
    hooks = {}
    for r in csv.DictReader(open(R / "hooks.csv", encoding="utf-8")):
        if r["module"] == MOD:
            hooks[int(r["addr"], 16)] = (r["name"], r["confidence"])
    return rows, master, hooks

def main():
    t = theirs(); rows, master, hooks = ours()
    out = [["va", "their_name", "their_size", "obj", "our_row", "row_file", "master", "master_size", "hooks", "level", "class"]]
    for va in sorted(t):
        tn, ts, obj = t[va]
        rn, rf = rows.get(va, ("", ""))
        mn, ms = master.get(va, ("", ""))
        hn, hl = hooks.get(va, ("", ""))
        if "jpeg.lib" in obj:
            cls = "lib"
        elif not rn and (not mn or mn.startswith("FUN_")):
            cls = "NEW"
        elif rn and GENERIC.search(rn):
            cls = "GENERIC"
        elif not rn:
            cls = "MASTER_ONLY"
        else:
            cls = "named"
        out.append([hex(va), tn, ts, obj, rn, rf, mn, ms, hn, hl, cls])
    with open(R / "log/roadmap/S10/8_diff.tsv", "w", encoding="utf-8", newline="") as f:
        csv.writer(f, delimiter="\t", lineterminator="\n").writerows(out)
    import collections
    print(collections.Counter(r[-1] for r in out[1:]))

main()
