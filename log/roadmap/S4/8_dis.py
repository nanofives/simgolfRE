"""S4.8 helper: disassemble sound.dll at an rva (the user's file, ImageBase addresses shown as rvas), resolve E9 thunks and
FF25 import jumps to hooks.csv names / DLL!name.

    ~/bin/py -3.12 log/roadmap/S4/8_dis.py 0x3a978 [n]      # n instructions (default 40)
    ~/bin/py -3.12 log/roadmap/S4/8_dis.py --who 0x1910 0x268a ...
"""
import csv
import pathlib
import struct
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "port" / "test"))
import sound_emu as E  # noqa: E402
from capstone import CS_ARCH_X86, CS_MODE_32, Cs  # noqa: E402

DATA, RELOCS, SECS, EXPS, ENTRY, IBASE = E._raw(str(E.SOUND_DLL))


def names():
    out = {}
    with open(ROOT / "hooks.csv", newline="") as f:
        for r in csv.DictReader(f):
            if r["module"].lower() == "sound.dll":
                out[int(r["addr"], 16)] = r["name"]
    return out


NAMES = names()


def imports():
    import pefile
    pe = pefile.PE(str(E.SOUND_DLL))
    out = {}
    for d in pe.DIRECTORY_ENTRY_IMPORT:
        for i in d.imports:
            out[i.address - IBASE] = f"{d.dll.decode()}!{(i.name or b'#%d' % i.ordinal).decode()}"
    pe.close()
    return out


IMPS = imports()


def who(rva):
    seen = []
    for _ in range(8):
        b = DATA[rva]
        if b == 0xE9:
            rva = (rva + 5 + struct.unpack_from("<i", DATA, rva + 1)[0]) & 0xFFFFFFFF
            seen.append(f"->0x{rva:x}")
            continue
        if DATA[rva] == 0xFF and DATA[rva + 1] == 0x25:
            slot = struct.unpack_from("<I", DATA, rva + 2)[0] - IBASE
            return " ".join(seen + [f"jmp [iat 0x{slot:x}] {IMPS.get(slot, '?')}"])
        break
    return " ".join(seen + [NAMES.get(rva, "?")])


def dis(rva, n=40):
    md = Cs(CS_ARCH_X86, CS_MODE_32)
    code = DATA[rva:rva + n * 15]
    for k, i in enumerate(md.disasm(code, IBASE + rva)):
        if k >= n:
            break
        op = i.op_str
        extra = ""
        if i.mnemonic in ("call", "jmp") and op.startswith("0x"):
            t = int(op, 16) - IBASE
            extra = f"   ; 0x{t:x} {who(t)}"
        elif "dword ptr [0x" in op:
            a = int(op.split("[0x")[1].split("]")[0], 16) - IBASE
            extra = f"   ; [0x{a:x}] {IMPS.get(a, '')}"
        print(f"{i.address - IBASE:06x}  {i.bytes.hex():<20} {i.mnemonic} {op}{extra}")
        if i.mnemonic == "ret" and k > 2 and n <= 40:
            pass


if __name__ == "__main__":
    a = sys.argv[1:]
    if a and a[0] == "--who":
        for x in a[1:]:
            print(x, who(int(x, 16)))
    else:
        dis(int(a[0], 16), int(a[1]) if len(a) > 1 else 40)
