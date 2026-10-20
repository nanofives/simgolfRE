"""38g: per-jcc branch coverage of the original's own ranges over the generated oracle cases; prints each missed side with
the two instructions before it. Usage: SG_PORTSIM_DLL=... ~/bin/py -3.12 log/p38/38g_cover.py N [seed] [--missing]"""
import sys, pathlib
ROOT = pathlib.Path(__file__).resolve().parents[2]; sys.path.insert(0, str(ROOT)); sys.path.insert(0, str(ROOT / "re" / "tools"))
from port.test import lib as L, test_routing as T, cmdemu as CE
import vector_emu as VE, vector_lib as VL
from capstone import Cs, CS_ARCH_X86, CS_MODE_32
lib = L.load()
n = int(sys.argv[1]); seed = sys.argv[2] if len(sys.argv) > 2 and not sys.argv[2].startswith("--") else "cover"
cover = {}
for case in T.iter_cases(lib, n, seed):
    img, args, inputs, cur = case
    T.emu(img, args, inputs, cur, cover=cover)
sites, both, missing = CE.coverage_summary(cover)
print(T.NAME, "jcc sites", sites, "both sides", both, "missing sides", len(missing))
if "--missing" in sys.argv:
    img = VE.load_image(); md = Cs(CS_ARCH_X86, CS_MODE_32)
    code = {}
    for lo, hi in img.func_ranges[T.ENTRY]:
        for i in md.disasm(img.data[lo - VL.IMAGE_BASE:hi - VL.IMAGE_BASE], lo):
            code[i.address] = i
    addrs = sorted(code)
    for a, side in missing:
        a = int(a, 16); k = addrs.index(a)
        prev = " | ".join(f"{code[x].mnemonic} {code[x].op_str}" for x in addrs[max(0, k - 2):k + 1])
        print(f"{a:#x} {side:12s} {prev}")
