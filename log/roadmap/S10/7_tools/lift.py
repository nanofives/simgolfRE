"""S10.7: C3 rows that library identity lifts to C4.  ~/bin/py -3.12 log/roadmap/S10/7_tools/lift.py
Reads 7_gaps_<key>.tsv (hooks_level column) and 7_score_<key>.tsv; writes 7_promote_cmds.txt (copy step first, then the
promote commands with --evidence) and 7_c2_identical.tsv (identical rows still at C2 or C1: they need C3 first)."""
import pathlib, re

ROOT = pathlib.Path(__file__).resolve().parents[4]
MODS = {"golf": ("golf_clean.exe", 0), "jgld": ("jgld.dll", 0x10000000), "sound": ("sound.dll", 0x10000000),
        "Terrain": ("Terrain.dll", 0x10000000)}
copy, prom, low = [], [], ["module\taddr\tname\tlevel\tsymbol"]
for key, (mod, base) in MODS.items():
    sc = {l.split("\t")[0]: l.split("\t") for l in (ROOT / f"log/roadmap/S10/7_score_{key}.tsv").read_text(encoding="utf-8").splitlines()[1:]}
    for l in (ROOT / f"log/roadmap/S10/7_gaps_{key}.tsv").read_text(encoding="utf-8").splitlines()[1:]:
        c = l.split("\t")
        s = sc[c[0]]
        va = int(c[0], 16)
        k = va - base
        if c[7] in ("C1", "C2"):
            low.append(f"{mod}\t{c[0]}\t{c[6]}\t{c[7]}\t{c[1]}")
        if c[7] != "C3":
            continue
        stem = f"{k:08x}_{key + '_' if key != 'golf' else ''}{re.sub(r'[^A-Za-z0-9_]', '_', s[1])}.match.csv"
        src = f"log/roadmap/S10/7_lib/{key}/{s[3]}/{stem}"
        copy.append(f"cp {src} log/diff/{stem}")
        addr = f"0x{k:08x}" if key != "golf" else f"0x{va:08x}"
        prom.append(f"~/bin/py -3.12 scripts/re_classify.py promote {addr} --to C4 --module {mod} --evidence log/diff/{stem}"
                    f"   # {c[6]} = {s[1]}, {s[3]} {s[4].split(chr(92))[-1]}, note: {s[8] or 'extent as listed'}")
(ROOT / "log/roadmap/S10/7_promote_cmds.txt").write_text("# copy step (the C4 gate reads log/diff)\n" + "\n".join(copy) + "\n\n# promotions\n" + "\n".join(prom) + "\n", encoding="utf-8")
(ROOT / "log/roadmap/S10/7_c2_identical.tsv").write_text("\n".join(low) + "\n", encoding="utf-8")
print(len(prom), "C3 rows lifted;", len(low) - 1, "identical rows at C1/C2")
