"""Disassemble sound.dll functions (file VAs, base 0x10000000) into log/roadmap/S4/4_dis/<rva>_<name>.txt (S4.4 reading aid)."""
import csv, pathlib, sys
import pefile, capstone
ROOT = pathlib.Path(__file__).resolve().parents[3]
pe = pefile.PE(str(ROOT / "original" / "sound.dll"))
img = pe.get_memory_mapped_image()
md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
rows = {int(r["entry"], 16): r for r in csv.DictReader(open(ROOT / "re/functions_ghidra_sound.dll.tsv"), delimiter="\t")}
for a in sys.argv[1:]:
    rva = int(a, 16)
    r = rows[rva]
    out = []
    for rg in r["ranges"].split(";"):
        lo, hi = (int(x, 16) for x in rg.split("-"))
        for i in md.disasm(img[lo:hi], 0x10000000 + lo):
            out.append(f"{i.address:08x}  {i.bytes.hex():<20} {i.mnemonic} {i.op_str}")
    p = ROOT / "log/roadmap/S4/4_dis" / f"{rva:05x}_{r['name']}.txt"
    p.write_text("\n".join(out) + "\n")
    print(p.name, len(out))
