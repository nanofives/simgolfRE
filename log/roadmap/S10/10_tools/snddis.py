"""Disassemble original/sound.dll ranges (VAs at image base 0x10000000): snddis.py 0xSTART 0xEND [...]."""
import sys, pathlib
import pefile, capstone
R = pathlib.Path(__file__).resolve().parents[4]
pe = pefile.PE(str(R / "original/sound.dll"), fast_load=True)
md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
a = sys.argv[1:]
for k in range(0, len(a), 2):
    lo, hi = int(a[k], 16), int(a[k + 1], 16)
    for i in md.disasm(pe.get_data(lo - 0x10000000, hi - lo), lo):
        print(f"{i.address:08x}  {i.mnemonic} {i.op_str}")
    print("--")
