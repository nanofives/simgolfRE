"""S10.10: the classes create_sound (sound.dll 0x100093e0) constructs, from OUR image.
For each `call <thunk>` after an operator new (0x100424d7) in create_sound, follow the E9 thunk to the ctor body and list
the `mov dword ptr [reg], 0x1005xxxx` vtable stores up to its first ret (last one = most derived, the object's vtable).
~/bin/py -3.12 log/roadmap/S10/10_tools/sndclasses.py"""
import pathlib, struct, sys
import pefile, capstone
R = pathlib.Path(__file__).resolve().parents[4]
pe = pefile.PE(str(R / "original/sound.dll"), fast_load=True)
md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
B = 0x10000000


def thunk(va):
    b = pe.get_data(va - B, 5)
    return va + 5 + struct.unpack("<i", b[1:])[0] if b[0] == 0xE9 else va


def vtstores(va):
    out = []
    for i in md.disasm(pe.get_data(va - B, 0x200), va):
        if i.mnemonic == "mov" and i.op_str.startswith("dword ptr [e") and "], 0x1005" in i.op_str \
                and "+" not in i.op_str.split(",")[0]:
            out.append((i.address, int(i.op_str.split(", ")[1], 16)))
        if i.mnemonic == "ret":
            break
    return out


def classes():
    res, prev_new = [], None
    for i in md.disasm(pe.get_data(0x100093e0 - B, 0x3f0), 0x100093e0):
        if i.mnemonic == "push" and i.op_str.startswith("0x") and int(i.op_str, 16) < 0x10000:
            prev_new = int(i.op_str, 16)
        if i.mnemonic == "call" and i.op_str.startswith("0x") and int(i.op_str, 16) != 0x100424d7 and prev_new:
            t = int(i.op_str, 16)
            b = thunk(t)
            vs = vtstores(b)
            res.append((i.address, prev_new, t, b, vs))
            prev_new = None
    return res


if __name__ == "__main__":
    for site, size, t, b, vs in classes():
        print(f"call 0x{site:08x} size 0x{size:x} thunk 0x{t:08x} ctor 0x{b:08x} vt " +
              ", ".join(f"0x{v:08x}@0x{a:08x}" for a, v in vs))
