"""S10.10: bodies at given byte offsets of given jgld.dll vtables, with our names and jgld.pdb's names.
~/bin/py -3.12 log/roadmap/S10/10_tools/jgldslots.py <vtable> <off> [<off> ...]"""
import glob, pathlib, struct, sys
import pefile
R = pathlib.Path(__file__).resolve().parents[4]
pe = pefile.PE(str(R / "original/jgld.dll"), fast_load=True)
names = {}
for f in glob.glob(str(R / "re/names/*.tsv")):
    if not f.endswith("_globals.tsv"):
        for l in open(f, encoding="utf-8").read().splitlines()[1:]:
            c = l.split("\t")
            if len(c) == 6 and c[0] == "jgld.dll":
                names[int(c[1], 16)] = (c[2], pathlib.Path(f).name)
jp = {}
for l in (R / "log/roadmap/S10/pdb/jgld.procs.tsv").read_text(encoding="utf-8").splitlines():
    c = l.split("\t")
    if len(c) > 2 and c[0].startswith("0x"):
        jp.setdefault(int(c[0], 16) + 0x10000000, set()).add(c[2])


def dw(va): return struct.unpack("<I", pe.get_data(va - 0x10000000, 4))[0]


def body(va):
    b = pe.get_data(va - 0x10000000, 5)
    return va + 5 + struct.unpack("<i", b[1:])[0] if b[0] == 0xE9 else va


vt = int(sys.argv[1], 16)
for o in sys.argv[2:]:
    t = body(dw(vt + int(o, 16)))
    n = names.get(t, ("-", ""))
    print(o, hex(t), n[0], n[1], "|", ", ".join(sorted(jp.get(t, {"-"}))))
