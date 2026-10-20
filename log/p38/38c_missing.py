"""38c: for each jcc side the oracle cases did not reach, print the two instructions before it (classify by hand)"""
import sys, pathlib
ROOT = pathlib.Path(__file__).resolve().parents[2]; sys.path.insert(0, str(ROOT)); sys.path.insert(0, str(ROOT / "re" / "tools"))
from port.test import lib as L, test_reports as TR, cmdemu as CE
import vector_emu as VE, vector_lib as VL
from capstone import Cs, CS_ARCH_X86, CS_MODE_32
lib = L.load()
name = sys.argv[1]; n = int(sys.argv[2])
cover = {}
for case in TR.iter_cases(lib, name, n, sys.argv[3] if len(sys.argv) > 3 else "cover"):
    img, args, inputs, cur = case
    TR.emu(name, img, args, inputs, cur, cover=cover)
sites, both, missing = CE.coverage_summary(cover)
print(name, sites, both)
img = VE.load_image(); md = Cs(CS_ARCH_X86, CS_MODE_32)
code = {}
for lo, hi in img.func_ranges[TR.ENTRY[name]]:
    for i in md.disasm(img.data[lo - VL.IMAGE_BASE:hi - VL.IMAGE_BASE], lo):
        code[i.address] = i
addrs = sorted(code)
for a, side in missing:
    a = int(a, 16); k = addrs.index(a)
    prev = " | ".join(f"{code[x].mnemonic} {code[x].op_str}" for x in addrs[max(0, k - 2):k + 1])
    print(f"{a:#x} {side:12s} {prev}")
