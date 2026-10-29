"""S10.7: identical library procedures whose existing names row disagrees with the library symbol (after dropping underscores,
@N and C++ decoration).  ~/bin/py -3.12 log/roadmap/S10/7_tools/conflicts.py > log/roadmap/S10/7_conflicts.tsv"""
import glob, pathlib, re, sys

ROOT = pathlib.Path(__file__).resolve().parents[4]
sys.path.insert(0, str(ROOT / "log/roadmap/S10/7_tools"))
MODS = {"golf": "golf_clean.exe", "jgld": "jgld.dll", "sound": "sound.dll", "Terrain": "Terrain.dll"}
rows = {}
for f in glob.glob(str(ROOT / "re/names/*.tsv")):
    p = pathlib.Path(f)
    if p.stem.endswith("_globals") or p.stem.endswith("_lib") or p.stem == "withdrawn":
        continue
    for l in p.read_text(encoding="utf-8").splitlines()[1:]:
        c = l.split("\t")
        if len(c) >= 3 and c[1].startswith("0x"):
            rows[(c[0], int(c[1], 16))] = (p.stem, c[2])


def norm(s):
    s = re.sub(r"@\d+$", "", s)
    m = re.match(r"\?+_?\w?([^@]+)@", s) if s.startswith("?") else None
    s = m.group(1) if m else s
    return re.sub(r"[^a-z0-9]", "", s.lower())


print("module\taddr\tlibrary symbol\tour name\tnames file")
for key, mod in MODS.items():
    for l in (ROOT / f"log/roadmap/S10/7_score_{key}.tsv").read_text(encoding="utf-8").splitlines()[1:]:
        c = l.split("\t")
        if float(c[5]) != 100.0:
            continue
        r = rows.get((mod, int(c[0], 16)))
        if r and norm(r[1]) != norm(c[1]) and norm(c[1]) not in norm(r[1]) and norm(r[1]) not in norm(c[1]):
            print(f"{mod}\t{c[0]}\t{c[1]}\t{r[1]}\t{r[0]}")
