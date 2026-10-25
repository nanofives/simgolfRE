import sys, pathlib, struct
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[4] / "re" / "tools"))
import vector_emu as VE, vector_lib as VL
from capstone import Cs, CS_ARCH_X86, CS_MODE_32
img = VE.load_image()
md = Cs(CS_ARCH_X86, CS_MODE_32)
def d(va): return struct.unpack_from("<I", img.data, va - VL.IMAGE_BASE)[0]
if sys.argv[1] == "slot":
    vt = int(sys.argv[2], 16)
    for s in sys.argv[3:]:
        s = int(s, 0); print(f"vtable {vt:08x} slot {s} (+{4*s:#x}) = {d(vt+4*s):08x}")
else:
    lo, hi = int(sys.argv[1], 16), int(sys.argv[2], 16)
    for i in md.disasm(img.data[lo - VL.IMAGE_BASE:hi - VL.IMAGE_BASE], lo):
        print(f"{i.address:08x} {i.mnemonic} {i.op_str}")
