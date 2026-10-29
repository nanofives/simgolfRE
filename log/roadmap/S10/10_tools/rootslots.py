"""S10.10: slots of the root window table 0x004ba2fc (and any table given) with our names and golf.pdb's names.
~/bin/py -3.12 log/roadmap/S10/10_tools/rootslots.py 0x004ba2fc [first last]"""
import pathlib, struct, sys
import pefile
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from rows import name_rows  # noqa: E402
R = pathlib.Path(__file__).resolve().parents[4]
pe = pefile.PE(str(R / "original/golf_clean.exe"), fast_load=True)
pdb = {}
for l in (R / "log/roadmap/S10/pdb/golf.procs.tsv").read_text(encoding="utf-8").splitlines():
    c = l.split("\t")
    if len(c) > 2 and c[0].startswith("0x"):
        pdb.setdefault(int(c[0], 16) + 0x400000, set()).add(c[2])
rows = name_rows()
t = int(sys.argv[1], 16)
lo, hi = (int(sys.argv[2]), int(sys.argv[3])) if len(sys.argv) > 3 else (0, 90)
for k in range(lo, hi + 1):
    d = struct.unpack("<I", pe.get_data(t - 0x400000 + 4 * k, 4))[0]
    r = rows.get(d)
    names = sorted(pdb.get(d, set()))
    print(f"{k:3} +0x{4*k:03x} 0x{d:08x} {r[1][2] if r else '-':40} {len(names)} {', '.join(names)[:150]}")
