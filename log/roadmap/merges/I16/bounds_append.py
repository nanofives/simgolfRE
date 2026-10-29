"""I16: append S10's boundaries rows to re/match/boundaries.tsv (CRLF kept): 9_boundaries.tsv, 6_boundaries_sound.tsv,
7_boundaries.tsv WITHOUT sound.dll 0x1004a010 (MERGE.md section 7). Skips a (module, entry) already present or proposed
twice. Writes log/roadmap/merges/I16/bounds_added.tsv.   [--write]"""
import pathlib, sys
R = pathlib.Path(__file__).resolve().parents[4]
S = R / "log/roadmap/S10"
B = R / "re/match/boundaries.tsv"
raw = B.read_bytes().decode("utf-8")
have = {tuple(l.split("\t")[:2]) for l in raw.splitlines()[1:] if l.strip()}
add, skipped = [], []
for name in ["9_boundaries.tsv", "6_boundaries_sound.tsv", "7_boundaries.tsv"]:
    for l in (S / name).read_text(encoding="utf-8").splitlines():
        c = l.split("\t")
        if not l.strip() or c[0] == "module":
            continue
        if len(c) != 4:
            sys.exit(f"{name}: bad row {l[:80]}")
        key = (c[0], c[1].lower())
        if key == ("sound.dll", "0x1004a010"):
            skipped.append(("excluded by MERGE.md 7", l))
            continue
        if key in have:
            skipped.append(("present", l))
            continue
        have.add(key)
        add.append(l)
print(len(add), "rows to add;", len(skipped), "skipped")
for why, l in skipped:
    print("  skip", why, l[:60])
(R / "log/roadmap/merges/I16/bounds_added.tsv").write_text("\n".join(add) + "\n", encoding="utf-8")
if "--write" in sys.argv:
    nl = "\r\n" if "\r\n" in raw else "\n"
    if not raw.endswith(nl):
        raw += nl
    B.write_bytes((raw + nl.join(add) + nl).encode("utf-8"))
    print("written")
