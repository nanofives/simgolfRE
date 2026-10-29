"""S4.10: disassemble whole sound.dll functions (ranges from re/functions_ghidra_sound.dll.tsv) in one process, call targets
named (thunks followed, hooks.csv names), absolute operands shown as rvas.

    ~/bin/py -3.12 log/roadmap/S4/10_snd_dis.py 0x3a6b0 0x5670 ...
"""
import csv
import pathlib
import re
import struct
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "port" / "test"))
import sound_emu as E  # noqa: E402
from capstone import CS_ARCH_X86, CS_MODE_32, Cs  # noqa: E402

DATA, RELOCS, SECS, EXPS, ENTRY, IBASE = E._raw(str(E.SOUND_DLL))
NAMES = {}
with open(ROOT / "hooks.csv", newline="") as f:
    for r in csv.DictReader(f):
        if r["module"].lower() == "sound.dll":
            NAMES[int(r["addr"], 16)] = r["name"]
FN = {}
with open(ROOT / "re" / "functions_ghidra_sound.dll.tsv", newline="") as f:
    for r in csv.DictReader(f, delimiter="\t"):
        rg = [tuple(int(x, 16) for x in g.split("-")) for g in (r.get("ranges") or "").split(";") if g]
        FN[int(r["entry"], 16)] = (r["name"], rg)


def follow(t):
    for _ in range(8):
        if 0 <= t < len(DATA) and DATA[t] == 0xE9:
            t = (t + 5 + struct.unpack_from("<i", DATA, t + 1)[0]) & 0xFFFFFFFF
        else:
            break
    return t


def name(t):
    return NAMES.get(t) or FN.get(t, ("?",))[0]


def dis(rva):
    md = Cs(CS_ARCH_X86, CS_MODE_32)
    nm, rgs = FN.get(rva, ("?", [(rva, rva + 0x40)]))
    print(f"== 0x{rva:x} {NAMES.get(rva, nm)}")
    for lo, hi in rgs:
        for i in md.disasm(bytes(DATA[lo:hi]), lo):
            ops = i.op_str
            ops = re.sub(r"0x(1[0-9a-f]{7})", lambda m: f"rva:0x{int(m.group(1), 16) - IBASE:x}", ops)
            extra = ""
            if i.mnemonic in ("call", "jmp") and i.op_str.startswith("0x"):
                t = follow(int(i.op_str, 16))
                extra = f"  ; {name(t)} (0x{t:x})"
            print(f"  {i.address:06x}  {i.mnemonic} {ops}{extra}")


if __name__ == "__main__":
    for a in sys.argv[1:]:
        dis(int(a, 16))
