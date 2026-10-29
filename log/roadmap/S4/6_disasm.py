"""Disassemble sound.dll ranges (file image, RVAs): 6_disasm.py <rva> <size> [<rva> <size> ...]"""
import sys
import capstone
import pefile

pe = pefile.PE("original/sound.dll")
img = pe.get_memory_mapped_image()
base = pe.OPTIONAL_HEADER.ImageBase
md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
args = sys.argv[1:]
for i in range(0, len(args), 2):
    rva, size = int(args[i], 16), int(args[i + 1], 16)
    print(f"; --- 0x{rva:x} size 0x{size:x}")
    for ins in md.disasm(img[rva:rva + size], base + rva):
        print(f"0x{ins.address - base:05x}  {ins.mnemonic:8s} {ins.op_str}")
