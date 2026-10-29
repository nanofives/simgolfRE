"""S10.7: library candidates per module -> log/roadmap/S10/7_cand_<key>.tsv (va, symbols, source, size, lib/module).
~/bin/py -3.12 log/roadmap/S10/7_tools/cands.py <golf|jgld|sound|Terrain>
jgld / Terrain: every PDB procedure whose lib column is a libc*.lib archive, symbols = the decorated publics at its RVA.
golf / sound: the PDBs list no VC6 archive as a module, so a candidate is a public with no procedure covering its RVA,
inside the first code section and not a data / import symbol (the static CRT is linked without debug records)."""
import pathlib, sys

ROOT = pathlib.Path(__file__).resolve().parents[4]
key = sys.argv[1]
base = {"golf": 0x400000}.get(key, 0x10000000)
pdb = ROOT / "log/roadmap/S10/pdb"
procs = []
# 7_pdb_<key>.procs.tsv = `re/tools/smsg_pdb.py procs <key>` (rva size kind name module lib)
for l in (ROOT / f"log/roadmap/S10/7_pdb_{key}.procs.tsv").read_text(encoding="utf-8").splitlines():
    c = l.strip().split("\t")
    if len(c) >= 6 and c[0].startswith("0x"):
        procs.append((int(c[0], 16), int(c[1]), c[3], c[5]))
pubs = {}
for l in (pdb / f"{key}.publics.tsv").read_text(encoding="utf-8").splitlines():
    c = l.split("\t")
    if len(c) >= 2 and c[0].startswith("0x"):
        pubs.setdefault(int(c[0], 16), []).append(c[1])
rows = []
if key in ("jgld", "Terrain"):
    for rva, size, name, lib in sorted(procs):
        if "libc" in lib.lower():
            rows.append((rva, ",".join(pubs.get(rva, [])), "proc", size, name + " | " + lib))
else:
    starts = sorted((r, r + s) for r, s, _, _ in procs)
    import bisect
    ss = [a for a, _ in starts]

    def covered(r):
        i = bisect.bisect_right(ss, r) - 1
        return i >= 0 and starts[i][0] <= r < starts[i][1]
    TEXT_END = {"golf": 0xb942b, "sound": 0x5a575}[key]      # .text VirtualAddress + VirtualSize (pefile, original\)
    allp = sorted(pubs)
    for n, rva in enumerate(allp):
        if covered(rva) or rva >= TEXT_END:
            continue
        gap = (allp[n + 1] if n + 1 < len(allp) else TEXT_END) - rva      # to the next public of any kind
        names = [n for n in pubs[rva] if not n.startswith("__imp_")]
        if names:
            rows.append((rva, ",".join(names), "public", gap, "size = gap to the next PDB public"))
out = ["va\tsymbols\tsource\tsize\tinfo"]
for rva, syms, src, size, info in rows:
    out.append(f"0x{base + rva:08x}\t{syms}\t{src}\t{size}\t{info}")
(ROOT / f"log/roadmap/S10/7_cand_{key}.tsv").write_text("\n".join(out) + "\n", encoding="utf-8")
print(key, len(rows), "candidates")
