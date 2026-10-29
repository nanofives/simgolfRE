"""Disassemble golf_clean.exe ranges: dis.py 0xSTART 0xEND [...]  or  dis.py --grep <regex> (whole .text)."""
import re, sys, pathlib
import pefile, capstone
R = pathlib.Path(__file__).resolve().parents[4]
pe = pefile.PE(str(R / "original/golf_clean.exe"), fast_load=True)
text = next(s for s in pe.sections if s.Name.startswith(b".text"))
base = 0x400000 + text.VirtualAddress
data = text.get_data()
md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)

def dis(lo, hi):
    for i in md.disasm(data[lo - base:hi - base], lo):
        print(f"{i.address:08x}  {i.mnemonic} {i.op_str}")

if sys.argv[1] == "--grep":
    rx = re.compile(sys.argv[2])
    off = 0
    while off < len(data):
        n = 0
        for i in md.disasm(data[off:], base + off):
            n = i.address + i.size - base
            s = f"{i.mnemonic} {i.op_str}"
            if rx.search(s):
                print(f"{i.address:08x}  {s}")
        off = max(n, off) + 1
else:
    a = sys.argv[1:]
    for k in range(0, len(a), 2):
        dis(int(a[k], 16), int(a[k + 1], 16)); print("--")
