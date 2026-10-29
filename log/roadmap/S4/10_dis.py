"""S4.10 helper: disassemble golf_clean.exe functions (the anchored file) with hooks.csv names for call targets and
DLL!name for IAT slots; ranges from re/functions_ghidra.tsv.

    ~/bin/py -3.12 log/roadmap/S4/10_dis.py 0x484440 [0x484450 ...]     # whole functions
    ~/bin/py -3.12 log/roadmap/S4/10_dis.py --n 40 0x484440              # n instructions from an address
"""
import csv
import pathlib
import struct
import sys

import pefile
from capstone import CS_ARCH_X86, CS_MODE_32, Cs

ROOT = pathlib.Path(__file__).resolve().parents[3]
EXE = ROOT / "original" / "golf_clean.exe"
PE = pefile.PE(str(EXE))
IBASE = PE.OPTIONAL_HEADER.ImageBase
IMG = bytearray(PE.OPTIONAL_HEADER.SizeOfImage)
IMG[:PE.OPTIONAL_HEADER.SizeOfHeaders] = PE.__data__[:PE.OPTIONAL_HEADER.SizeOfHeaders]
for s in PE.sections:
    raw = s.get_data()[:max(s.Misc_VirtualSize, 0) or s.SizeOfRawData]
    IMG[s.VirtualAddress:s.VirtualAddress + len(raw)] = raw
IMPS = {}
for d in PE.DIRECTORY_ENTRY_IMPORT:
    for i in d.imports:
        IMPS[i.address] = f"{d.dll.decode()}!{(i.name or b'#%d' % i.ordinal).decode()}"


def names():
    out = {}
    with open(ROOT / "hooks.csv", newline="") as f:
        for r in csv.DictReader(f):
            if r["module"] in ("", "golf_clean.exe"):
                out[int(r["addr"], 16)] = r["name"]
    return out


NAMES = names()


def ranges():
    out = {}
    with open(ROOT / "re" / "functions_ghidra.tsv", newline="") as f:
        for r in csv.DictReader(f, delimiter="\t"):
            e = int(r["entry"], 16)
            rg = r.get("ranges") or ""
            out[e] = [tuple(int(x, 16) for x in g.split("-")) for g in rg.split(";") if g] if rg else []
    return out


RANGES = ranges()


def u32(va):
    return struct.unpack_from("<I", IMG, va - IBASE)[0]


def annotate(ins):
    t = ins.op_str
    extra = []
    for tok in t.replace("[", " ").replace("]", " ").replace(",", " ").split():
        if tok.startswith("0x"):
            v = int(tok, 16)
            if v in NAMES:
                extra.append(NAMES[v])
            elif v in IMPS:
                extra.append(IMPS[v])
    return "  ; " + ", ".join(extra) if extra else ""


def dis(va, n=None):
    md = Cs(CS_ARCH_X86, CS_MODE_32)
    rgs = RANGES.get(va) if n is None else None
    if not rgs:
        rgs = [(va, va + (n or 40) * 15)]
    print(f"== 0x{va:08x} {NAMES.get(va, '?')}")
    for lo, hi in rgs:
        k = 0
        for ins in md.disasm(bytes(IMG[lo - IBASE:hi - IBASE]), lo):
            print(f"  {ins.address:08x}  {ins.bytes.hex():<16} {ins.mnemonic} {ins.op_str}{annotate(ins)}")
            k += 1
            if n is not None and k >= n:
                break


if __name__ == "__main__":
    a = sys.argv[1:]
    n = None
    if a and a[0] == "--n":
        n = int(a[1])
        a = a[2:]
    for x in a:
        dis(int(x, 16), n)
