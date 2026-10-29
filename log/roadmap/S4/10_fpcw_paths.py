"""S4.10, U-1421: who reaches codecDecodeFrame / codecEncodeFrame, on which thread, and which functions on the way load an
x87 control word. Static: sound.dll's call graph (re/tools/xref.py's index), virtual-slot call sites (`call [reg + off]`
whose offset is a slot of a class table that holds the callee), the thread entry points (CreateThread's start address, the
timeSetEvent callback, the window procedures), and every `fldcw` in the module with its function.

    ~/bin/py -3.12 log/roadmap/S4/10_fpcw_paths.py > log/roadmap/S4/10_fpcw_paths.txt
"""
import collections
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


def functions():
    out = {}
    with open(ROOT / "re" / "functions_ghidra_sound.dll.tsv", newline="") as f:
        for r in csv.DictReader(f, delimiter="\t"):
            e = int(r["entry"], 16)
            rg = [tuple(int(x, 16) for x in g.split("-")) for g in (r.get("ranges") or "").split(";") if g]
            out[e] = (r["name"], rg)
    return out


FNS = functions()


def owner(rva):
    for e, (n, rgs) in FNS.items():
        for lo, hi in rgs:
            if lo <= rva < hi:
                return e
    return None


md = Cs(CS_ARCH_X86, CS_MODE_32)
direct = collections.defaultdict(set)     # callee rva -> caller entries
vsites = collections.defaultdict(set)     # slot offset -> caller entries (call [reg + off])
fldcw = collections.defaultdict(list)     # entry -> fldcw sites
for e, (n, rgs) in FNS.items():
    for lo, hi in rgs:
        for i in md.disasm(bytes(DATA[lo:hi]), lo):
            if i.mnemonic == "call" and i.op_str.startswith("0x"):
                t = int(i.op_str, 16)
                for _ in range(8):                      # follow incremental-link thunks
                    if DATA[t] == 0xE9:
                        t = (t + 5 + struct.unpack_from("<i", DATA, t + 1)[0]) & 0xFFFFFFFF
                    else:
                        break
                direct[t].add(e)
            m = re.fullmatch(r"dword ptr \[(e[a-z]{2}) \+ (0x[0-9a-f]+)\]", i.op_str)
            if i.mnemonic in ("call", "jmp") and m:
                vsites[int(m.group(2), 16)].add(e)
            if i.mnemonic == "fldcw":
                fldcw[e].append(i.address)


def slots_of(rva):
    """(table rva, slot offset) of every class-table slot holding rva or one of its thunks"""
    targets = {rva}
    for t in range(0x1000, 0x3000):                  # the thunk table at the start of .text
        if DATA[t] == 0xE9 and (t + 5 + struct.unpack_from("<i", DATA, t + 1)[0]) & 0xFFFFFFFF == rva:
            targets.add(t)
    out = []
    for o in range(0x5b000, 0x63000, 4):
        v = struct.unpack_from("<I", DATA, o)[0] - IBASE
        if v in targets:
            out.append(o)
    return out


def name(e):
    return FNS.get(e, ("?",))[0]


def walk(root, depth=6):
    seen, lines = set(), []

    def go(r, d, path):
        if d > depth or r in seen:
            return
        seen.add(r)
        callers = sorted(direct.get(r, ()))
        slot_callers = []
        for o in slots_of(r):
            # the slot offset inside its table: the nearest table start below is not known; list every call site whose
            # displacement is a multiple-of-4 offset that the slot can have in the tables around it
            for base in range(o - 0x200, o + 1, 4):
                off = o - base
                if off in vsites:
                    slot_callers.append((off, base))
        lines.append("  " * d + f"0x{r:x} {name(r)}  fldcw {['0x%x' % s for s in fldcw.get(r, [])]}  "
                     f"direct callers {[name(c) for c in callers]}")
        for c in callers:
            go(c, d + 1, path + [r])
    go(root, 0, [])
    return lines


if __name__ == "__main__":
    for r in (0x3a980, 0x3b150):
        print("\n".join(walk(r)))
        print()
    print("fldcw sites by function:")
    for e, s in sorted(fldcw.items()):
        print(f"  0x{e:x} {name(e)}: {', '.join('0x%x' % x for x in s)}")
    print("\nclass-table slots holding openMmioSound's caller Sound5c450::openMmio:", [hex(x) for x in slots_of(0x30710)])
    print("call sites [reg + 0x8c]:", sorted(name(e) for e in vsites.get(0x8c, ())))
