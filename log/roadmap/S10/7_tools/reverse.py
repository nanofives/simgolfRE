"""S10.7: leftover functions of ours (7_leftover.tsv) against EVERY function symbol of the archives (statics included, no PDB name
needed): same length (library body plus up to 16 bytes of fill), then match.py's comparison.  This answers what a FidDb pass
could still add.   ~/bin/py -3.12 log/roadmap/S10/7_tools/reverse.py <golf|jgld|sound|Terrain> > log/roadmap/S10/7_reverse_<key>.tsv"""
import pathlib, sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import libscore as S  # noqa: E402

key = sys.argv[1]
only = None
if "--only" in sys.argv:       # restrict to the entries of an earlier run (ambiguity pass)
    only = {l.split("	")[1] for l in open(sys.argv[sys.argv.index("--only") + 1], encoding="utf-8").read().splitlines()[1:] if l and not l.startswith("#")}
mod = S.MODS[key]
bodies = {}
for tag, path in S.LIBS[key]:
    ext, static = S.index(path)
    items = [(n, v) for n, v in ext.items()] + [(n, x) for n, vs in static.items() for x in vs]
    for name, (member, secs, osyms) in items:
        try:
            ln = len(S.match.function_from_obj(secs, osyms, name)[0])
        except SystemExit:
            continue
        bodies.setdefault(ln, []).append((tag, name, member, secs, osyms))
print("module\tentry\tsize\tlibrary symbol\ttag\tmember\tpct\tcsv\tidentical symbols")
n = ident = 0
for l in (S.ROOT / "log/roadmap/S10/7_leftover.tsv").read_text(encoding="utf-8").splitlines()[1:]:
    c = l.split("\t")
    if c[0] != mod:
        continue
    if only is not None and c[1] not in only:
        continue
    va, size = int(c[1], 16), int(c[2])
    same100 = set()
    n += 1
    best = (0.0, None, None, None)
    for ln in range(max(1, size - 16), size + 1):
        for tag, name, member, secs, osyms in bodies.get(ln, []):
            for sz in (None, ln):
                r = S.score(mod, va, secs, osyms, name, sz)
                if r and r[0] == 1:
                    same100.add(name)
                if r and r[0] > best[0]:
                    best = (r[0], (tag, name, member), r, S.LIBS_PATH[key][tag])
    if best[1] and best[0] >= 0.9:
        ident += best[0] == 1
        csv = "-"
        if best[0] == 1 and size >= 8:           # shorter bodies are the same few bytes in many library functions
            acc, a, b, full, same = best[2]
            k = va - (0 if key == "golf" else 0x10000000)
            d = S.ROOT / f"log/roadmap/S10/7_lib/{key}/rev_{best[1][0]}"
            d.mkdir(parents=True, exist_ok=True)
            fnm = f"{k:08x}_{key + '_' if key != 'golf' else ''}{S.re.sub(r'[^A-Za-z0-9_]', '_', best[1][1])}.match.csv"
            S.write_csv(d / fnm, best[3], best[1][2], mod, full, a, b, acc)
            csv = f"log/roadmap/S10/7_lib/{key}/rev_{best[1][0]}/{fnm}"
        print(f"{mod}\t{c[1]}\t{size}\t{best[1][1]}\t{best[1][0]}\t{best[1][2]}\t{100 * best[0]:.1f}\t{csv}\t{len(same100)}")
print(f"# {key}: {n} leftovers, {ident} identical", file=sys.stderr)
