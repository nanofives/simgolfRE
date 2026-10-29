"""S10.7: for every miss (best score < 100%) of 7_score_<key>.tsv print the library body length, our body length and the
first differing instruction pairs.  ~/bin/py -3.12 log/roadmap/S10/7_tools/missdiff.py <key> > log/roadmap/S10/7_miss_<key>.log"""
import difflib, pathlib, sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import libscore as S  # noqa: E402

key = sys.argv[1]
mod = S.MODS[key]
libs = {t: (S.index(p)) for t, p in S.LIBS[key]}
psize = {int(l.split(chr(9))[0], 16): int(l.split(chr(9))[3] or 0) for l in (S.ROOT / f"log/roadmap/S10/7_cand_{key}.tsv").read_text(encoding="utf-8").splitlines()[1:]}
for l in (S.ROOT / f"log/roadmap/S10/7_score_{key}.tsv").read_text(encoding="utf-8").splitlines()[1:]:
    c = l.split("\t")
    if c[3] == "-" or float(c[5]) == 100.0:
        continue
    va, sym, tag = int(c[0], 16), c[1], c[3]
    ext, static = libs[tag]
    member, secs, osyms = ([ext[sym]] if sym in ext else static[sym])[0]
    best = None
    for size, whole, note in S.variants(mod, va, secs, osyms, sym, psize.get(va, 0)):
        r = S.score(mod, va, secs, osyms, sym, size, whole)
        if r and (best is None or r[0] > best[0][0]):
            best = (r, note)
    (acc, a, b, full, same), note = best
    print(f"== 0x{va:08x} {sym} {tag} {member} {100 * acc:.1f}% [{note}] lib {len(a)} ins, ours {len(b)} ins")
    sm = difflib.SequenceMatcher(None, [x[1] for x in a], [x[1] for x in b], autojunk=False)
    shown = 0
    for t, i1, i2, j1, j2 in sm.get_opcodes():
        if t == "equal" or shown >= 5:
            continue
        shown += 1
        print(f"   {t} lib[{i1}:{i2}] {'; '.join(x[0] for x in a[i1:i2][:3])}  |  ours[{j1}:{j2}] {'; '.join(x[0] for x in b[j1:j2][:3])}")
