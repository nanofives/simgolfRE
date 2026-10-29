"""S10.7: which identical (100%) library procedures have no names row?  ~/bin/py -3.12 log/roadmap/S10/7_tools/gaps.py
Writes 7_gaps_<key>.tsv (va, symbol, lib, member, pct, names_file, our_name, hooks_level, note) for every 100% row of
7_score_<key>.tsv; names_file is "-" when no re/names/*.tsv (except this step's own smsg_<module>_lib.tsv) has the address."""
import csv, glob, pathlib

ROOT = pathlib.Path(__file__).resolve().parents[4]
MODS = {"golf": "golf_clean.exe", "jgld": "jgld.dll", "sound": "sound.dll", "Terrain": "Terrain.dll"}
BASE = {"golf": 0, "jgld": 0x10000000, "sound": 0x10000000, "Terrain": 0x10000000}
FN = {"golf": "functions_ghidra.tsv", "jgld": "functions_ghidra_jgld.dll.tsv", "sound": "functions_ghidra_sound.dll.tsv",
      "Terrain": "functions_ghidra_Terrain.dll.tsv"}
named = {}
for f in glob.glob(str(ROOT / "re/names/*.tsv")):
    p = pathlib.Path(f)
    if p.stem.endswith("_globals") or p.stem.endswith("_lib") or p.stem == "withdrawn":
        continue
    for l in p.read_text(encoding="utf-8").splitlines()[1:]:
        c = l.split("\t")
        if len(c) >= 3 and c[1].startswith("0x"):
            named[(c[0], int(c[1], 16))] = (p.stem, c[2])
hooks = {}
for r in csv.DictReader(open(ROOT / "hooks.csv", encoding="utf-8")):
    hooks[(r["module"], int(r["addr"], 16))] = (r["confidence"], r["name"])
for key, mod in MODS.items():
    fn = {}
    for l in (ROOT / "re" / FN[key]).read_text(encoding="utf-8").splitlines()[1:]:
        c = l.split("\t")
        fn[int(c[0], 16)] = c[1]
    out = ["va\tsymbol\tlib\tmember\tpct\tnames_file\tour_name\thooks_level\tnote"]
    for l in (ROOT / f"log/roadmap/S10/7_score_{key}.tsv").read_text(encoding="utf-8").splitlines()[1:]:
        c = l.split("\t")
        if float(c[5]) != 100.0:
            continue
        va = int(c[0], 16)
        k = va if key == "golf" else va - BASE[key]
        nf, nm = named.get((mod, va), named.get((mod, k), ("-", "")))
        hl, hn = hooks.get((mod, k), ("-", ""))
        out.append("\t".join([c[0], c[1], c[3], c[4], c[5], nf, fn.get(k, fn.get(va, "(not in function list)")),
                              hl, c[8] if len(c) > 8 else ""]))
    (ROOT / f"log/roadmap/S10/7_gaps_{key}.tsv").write_text("\n".join(out) + "\n", encoding="utf-8")
    rows = [o.split("\t") for o in out[1:]]
    print(key, "identical", len(rows), "no names row", sum(r[5] == "-" for r in rows),
          "levels", {lv: sum(r[7] == lv for r in rows) for lv in sorted({r[7] for r in rows})})
