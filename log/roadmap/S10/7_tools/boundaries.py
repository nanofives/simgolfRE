"""S10.7: re/match/boundaries.tsv lines for the identical rows that need an extent different from our function list's.
~/bin/py -3.12 log/roadmap/S10/7_tools/boundaries.py > log/roadmap/S10/7_boundaries.tsv
A row needs a line when its 100% came from an extent variant (PDB size, padding trimmed, the library's code size) or from byte
identity; lines already in re/match/boundaries.tsv or in S10.9's 9_boundaries.tsv are listed in 7_boundaries_have.tsv instead."""
import pathlib, sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import libscore as S  # noqa: E402

ROOT = S.ROOT
have = set()
for f in (ROOT / "re/match/boundaries.tsv", ROOT / "log/roadmap/S10/9_boundaries.tsv"):
    for l in f.read_text(encoding="utf-8").splitlines()[1:]:
        c = l.split("\t")
        if len(c) >= 3:
            have.add((c[0], int(c[1], 16)))
new, old = [], []
for key, mod in S.MODS.items():
    idxs = {t: S.index(p) for t, p in S.LIBS[key]}
    psizes = {int(l.split("\t")[0], 16): int(l.split("\t")[3] or 0) for l in (ROOT / f"log/roadmap/S10/7_cand_{key}.tsv").read_text(encoding="utf-8").splitlines()[1:]}
    for l in (ROOT / f"log/roadmap/S10/7_score_{key}.tsv").read_text(encoding="utf-8").splitlines()[1:]:
        c = l.split("\t")
        if float(c[5]) != 100.0 or not c[8]:
            continue
        va, sym, tag, note = int(c[0], 16), c[1], c[3], c[8]
        ext, static = idxs[tag]
        member, secs, osyms = ([ext[sym]] if sym in ext else static[sym])[0]
        if note.startswith("byte-identity"):
            syms = [dict(s, type=0) if s["name"] != sym and s["type"] == 0x20 else s for s in osyms]
            size = len(S.match.function_from_obj(secs, syms, sym)[0])
        else:
            size = next((sz for sz, wh, lb in S.variants(mod, va, secs, osyms, sym, psizes.get(va, 0)) if lb == note), None)
            size = size or S.match.ghidra_size(mod, va)
        ours = S.match.ghidra_size(mod, va)
        if ours is None or size == ours:      # not a function of ours: an entry label, no extent to fix
            continue
        line = (f"{mod}\t0x{va:08x}\t{size}\t{sym}: {tag.split('_')[0].upper()} {c[4].split(chr(92))[-1]} identical over {size} bytes "
                f"({note}; S10.7); our function list gives {ours}")
        (old if (mod, va) in have else new).append(line)
(ROOT / "log/roadmap/S10/7_boundaries_have.tsv").write_text("\n".join(old) + "\n", encoding="utf-8")
print("module\tentry\tsize\treason")
print("\n".join(new))
print(len(new), "new,", len(old), "already present", file=sys.stderr)
