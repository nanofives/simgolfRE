"""S10.7: which archive version agrees with the module? For golf / sound, every candidate scored in rtm_cmt, sp5_cmt and
sp2_cmt separately (libscore.py keeps only the best).  ~/bin/py -3.12 log/roadmap/S10/7_tools/agree.py <golf|sound>
Writes log/roadmap/S10/7_agree_<key>.tsv: va, symbol, pct per library, and prints the count of 100% per library."""
import pathlib, sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import libscore as S  # noqa: E402

key = sys.argv[1]
mod = S.MODS[key]
TAGS = ["rtm_cmt", "sp5_cmt", "sp2_cmt"]
libs = {t: S.index(p) for t, p in S.LIBS[key] if t in TAGS}
cand = [l.split("\t") for l in (S.ROOT / f"log/roadmap/S10/7_cand_{key}.tsv").read_text(encoding="utf-8").splitlines()[1:]]
rows, full = [], dict.fromkeys(TAGS, 0)
for c in cand:
    va, syms, psize = int(c[0], 16), [s for s in c[1].split(",") if s], int(c[3] or 0)
    res = {}
    for t, (ext, static) in libs.items():
        for sym in syms:
            for member, secs, osyms in ([ext[sym]] if sym in ext else []) + static.get(sym, []):
                acc = S.best_acc(mod, va, secs, osyms, sym, psize)
                if acc > res.get(t, (-1,))[0]:
                    res[t] = (acc, sym)
    if res:
        sym = max(res.values())[1]
        rows.append(f"0x{va:08x}\t{sym}\t" + "\t".join(f"{100 * res[t][0]:.1f}" if t in res else "-" for t in TAGS))
        for t in res:
            full[t] += res[t][0] == 1
print(key, "candidates with a member in some library:", len(rows), "identical:", full)
(S.ROOT / f"log/roadmap/S10/7_agree_{key}.tsv").write_text(
    "va\tsymbol\t" + "\t".join(TAGS) + "\n" + "\n".join(rows) + "\n", encoding="utf-8")
