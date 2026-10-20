import sys
sys.path.insert(0, "re/tools")
import vector_emu as VE, vector_lib as VL
from capstone import Cs, CS_ARCH_X86, CS_MODE_32
img = VE.load_image()
lo, hi = int(sys.argv[1], 16), int(sys.argv[2], 16)
md = Cs(CS_ARCH_X86, CS_MODE_32)
for i in md.disasm(img.data[lo - VL.IMAGE_BASE:hi - VL.IMAGE_BASE], lo):
    print(f"{i.address:#x} {i.mnemonic} {i.op_str}")
