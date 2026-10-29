"""S10.9: crt names rows from library-object identity (9_lib_<k>.tsv + the SMSG-extent re-runs in 9_lib/<k>/ext_*).
~/bin/py -3.12 log/roadmap/S10/9_tools/build_lib_rows.py <jgld|Terrain> <names id>  -> re/names/<id>.tsv, 9_libledger_<k>.tsv"""
import csv, glob, pathlib, re, sys

ROOT = pathlib.Path(__file__).resolve().parents[4]
key, nid = sys.argv[1], sys.argv[2]
mod = f"{key}.dll"
SPECIAL = {"operator new[]": "operator_new_array", "operator delete": "operator_delete", "$$$00001": None}
ours = {int(l.split("\t")[0], 16): int(l.split("\t")[2]) for l in (ROOT / f"re/functions_ghidra_{mod}.tsv").read_text().splitlines()[1:]}
taken = set(l.split("\t")[1] for l in (ROOT / f"re/functions_ghidra_{mod}.tsv").read_text().splitlines()[1:])
for f in glob.glob(str(ROOT / "re/names/*.tsv")):
    if not f.endswith("_globals.tsv") and pathlib.Path(f).stem != nid:
        for l in open(f, encoding="utf-8").read().splitlines()[1:]:
            c = l.split("\t")
            if len(c) == 6 and c[0] == mod:
                taken.add(c[2])


def verdict_csv(path):
    rows = list(csv.reader(open(path, encoding="utf-8")))
    v = rows[-1]
    return v[1] == "GREEN", v[3], rows[-2][1].split("member=")[1].split(" ")[0]


out, ledger = [], []
for l in (ROOT / f"log/roadmap/S10/9_lib_{key}.tsv").read_text(encoding="utf-8").splitlines()[1:]:
    va, theirs, sym, libm, pct = l.split("\t")
    a = int(va, 16)
    stem = f"{a - 0x10000000:08x}_{key}_{re.sub(r'[^A-Za-z0-9_]', '_', sym)}.match.csv"
    hits = [(p, *verdict_csv(p)) for p in glob.glob(str(ROOT / f"log/roadmap/S10/9_lib/{key}/*/{stem}"))]
    green = [h for h in hits if h[1]]
    if not green:
        best = max((h[2] for h in hits), default="0.0%")
        ledger.append(f"{va}\tREJECT_LIB\t{theirs}\t-\tno library member matches at 100% (best {best})")
        continue
    p, _, score, member = sorted(green, key=lambda h: "ext_" in h[0])[0]
    ext = "ext_" in p
    rel = pathlib.Path(p).relative_to(ROOT).as_posix()
    lib = "tools/vc6sp2/VC98/lib/lib/libcmtd.lib" if "lib0" in p or "ext_sp2" in p else \
        "tools/vc6sp2/VC98/lib/LIBCMTD.LIB" if "lib1" in p or "ext_LIBCMTD" in p else "tools/vc6sp2/VC98/lib/LIBCIMTD.LIB"
    name = SPECIAL.get(theirs, theirs) or (sym[1:] if sym.startswith("_") else sym)
    if name in taken:
        name = f"{name}_{va[2:]}"
    taken.add(name)
    size = int(re.search(r"\((\d+)/", score).group(1)) if "(" in score else 0
    extent = (f" with SMSG's extent instead of ours ({ours.get(a - 0x10000000, '?')} bytes in re/functions_ghidra_{mod}.tsv: "
              f"the body continues past it; boundaries row in log/roadmap/S10/9_merge.md)") if ext else ""
    ev = (f"library-object identity: {score} of the normalized instructions equal member {member} of {lib}{extent} "
          f"(log/c3/m36e_lib_match.py, CSV {rel}); symbol {sym}; SMSG-decomp's PDB names it {theirs} from libcmtd.lib")
    out.append("\t".join([mod, va, name, "crt", ev, f"VC6 SP2 debug CRT routine {theirs} (object {member.split(chr(92))[-1]})."]))
    ledger.append(f"{va}\tACCEPT_LIB{'_EXT' if ext else ''}\t{theirs}\t{name}\t")
(ROOT / f"re/names/{nid}.tsv").write_text("module\taddr\tname\tsubsystem\tevidence\tpurpose\n" + "\n".join(out) + "\n", encoding="utf-8")
(ROOT / f"log/roadmap/S10/9_libledger_{key}.tsv").write_text("va\tverdict\tsmsg\tname\treason\n" + "\n".join(ledger) + "\n", encoding="utf-8")
print(nid, len(out), "rows;", sum(1 for x in ledger if "REJECT" in x), "rejected")
