"""S10.6: C3 -> C4 promote commands for sound.dll rows (inventory_sound.tsv c4_candidate, SMSG status 100), with the match
CSV written by `match.py re/match/smsg_sound_*.cpp`. A DLL row is addressed by its RVA with --module sound.dll.
Each line ends with the name and `8168 <score>` (the same body compiled with the default toolchain): only a row below 100.0
there needs `// TOOLCHAIN sp5`."""
import csv
import glob

tc = {r["addr"]: r["score_8168"] for r in csv.DictReader(open("log/roadmap/S10/6_tc_canonical.tsv", encoding="utf-8"), delimiter="\t")}
n = 0
for r in csv.DictReader(open("log/roadmap/S10/inventory_sound.tsv"), delimiter="\t"):
    if r["c4_candidate"] != "yes" or r["smsg_status"] != "100":
        continue
    rva = int(r["rva"], 16)
    hits = sorted(glob.glob(f"log/diff/{rva:08x}_sound_*.match.csv"))
    if len(hits) != 1:
        print(f"# {r['va']} {r['hooks_name']}: {len(hits)} match CSVs")
        continue
    print(f"~/bin/py -3.12 scripts/re_classify.py promote 0x{rva:08x} --module sound.dll --to C4 --evidence {hits[0].replace(chr(92), "/")}   # {r['hooks_name']}  8168 {tc.get(r['va'], '?')}")
    n += 1
print(f"# {n} commands")
