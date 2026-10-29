"""S10.9: library identity of the jgld / Terrain CRT candidates through log/c3/m36e_lib_match.py.
~/bin/py -3.12 log/roadmap/S10/9_tools/libid.py <jgld|Terrain>
For each candidate whose obj is a .lib: every decorated public at its RVA, tried against SP2's libcmtd.lib, the RTM one
and libcimtd.lib; CSVs in log/roadmap/S10/9_lib/<key>/ (never log/diff). Writes 9_lib_<key>.tsv: va, symbol, lib, pct."""
import pathlib, re, subprocess, sys

ROOT = pathlib.Path(__file__).resolve().parents[4]
key = sys.argv[1]
mod = {"jgld": "jgld.dll", "Terrain": "Terrain.dll"}[key]
LIBS = ["tools/vc6sp2/VC98/lib/lib/libcmtd.lib", "tools/vc6sp2/VC98/lib/LIBCMTD.LIB", "tools/vc6sp2/VC98/lib/LIBCIMTD.LIB"]
pubs = {}
for l in (ROOT / f"log/roadmap/S10/pdb/{key}.publics.tsv").read_text(encoding="utf-8").splitlines():
    c = l.split("\t")
    if len(c) >= 2 and c[0].startswith("0x"):
        pubs.setdefault(int(c[0], 16), []).append(c[1])
cand = [l.split("\t") for l in (ROOT / f"log/roadmap/S10/9_cand_{key}.tsv").read_text(encoding="utf-8").splitlines()[1:]]
out = ROOT / f"log/roadmap/S10/9_lib/{key}"
rows = []
for c in cand:
    if not c[5].lower().endswith(".lib") or c[3] == "-":
        continue
    va = int(c[0], 16)
    best = (0.0, "-", "-")
    for sym in pubs.get(va - 0x10000000, []):
        for lib in LIBS:
            r = subprocess.run([sys.executable, str(ROOT / "log/c3/m36e_lib_match.py"), lib, mod, f"0x{va:08x}={sym}",
                                "--out", str(out / f"lib{LIBS.index(lib)}")], capture_output=True, text=True, cwd=ROOT)
            m = re.search(r": member (\S+): ([\d.]+)%", r.stdout)
            if m and float(m.group(2)) > best[0]:
                best = (float(m.group(2)), sym, f"{lib}:{m.group(1)}")
            if best[0] == 100.0:
                break
        if best[0] == 100.0:
            break
    rows.append(f"0x{va:08x}\t{c[4]}\t{best[1]}\t{best[2]}\t{best[0]:.1f}")
    print(rows[-1], flush=True)
(ROOT / f"log/roadmap/S10/9_lib_{key}.tsv").write_text("va\ttheirs\tsymbol\tlib_member\tpct\n" + "\n".join(rows) + "\n", encoding="utf-8")
