"""Phase 3.7a: evidence for SegmentRunner's NON-CAUSAL list (port/test/segments.py NON_CAUSAL).

g_textBuf (0x0058a528, char[16]): every reference in golf_clean.exe is an address-take (re/model/inventory_addresses.tsv:
81 refs, 0 direct reads or writes). This scan finds each occurrence of the address as an instruction operand and prints the
next call after it; the object is non-causal when every one is the static CRT's __itoa (0x004ad425, which writes the digits
there and returns the buffer), so whatever the buffer held before a call is never read. usage: ~/bin/py -3.12 this
"""
import pathlib
import struct
import sys

import capstone
import pefile

ROOT = pathlib.Path(__file__).resolve().parents[2]
EXE = ROOT / "original" / "golf_clean.exe"
TARGET = 0x0058A528
ITOA = 0x004AD425


def main():
    pe = pefile.PE(str(EXE), fast_load=True)
    text = next(s for s in pe.sections if s.Name.rstrip(b"\0") == b".text")
    base = pe.OPTIONAL_HEADER.ImageBase + text.VirtualAddress
    code = text.get_data()
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    pat = struct.pack("<I", TARGET)
    hits, ok, other = 0, 0, []
    i = code.find(pat)
    while i >= 0:
        # decode from a few bytes back until an instruction covers the operand
        found = None
        for back in range(1, 8):
            start = i - back
            if start < 0:
                continue
            ins = next(md.disasm(code[start:start + 16], base + start), None)
            if ins and ins.address + ins.size >= base + i + 4 and hex(TARGET) in ins.op_str:
                found = (start, ins)
                break
        if found:
            hits += 1
            start, ins = found
            # the call that consumes the pushed address: walk forward counting the pushes made after it; a call before
            # __itoa must pop only those (`add esp, 4k` right after it, k <= the pushes above ours)
            above, verdict, last_call = -1, None, None
            for j in md.disasm(code[start:start + 0x200], base + start):
                if j.mnemonic == "push":
                    above += 1
                elif j.mnemonic == "call":
                    if j.op_str == hex(ITOA):
                        verdict = "itoa"
                        break
                    last_call = j
                elif last_call is not None and j.mnemonic == "add" and j.op_str.startswith("esp, "):
                    k = int(j.op_str.split(", ")[1], 16) // 4
                    if k > above:
                        verdict = "consumed by " + hex(last_call.address)
                        break
                    above -= k
                    last_call = None
            if verdict == "itoa":
                ok += 1
            else:
                other.append((hex(ins.address), ins.mnemonic, ins.op_str, verdict))
        i = code.find(pat, i + 1)
    print(f"operand references: {hits}; consumed by __itoa: {ok}; other: {len(other)}")
    for o in other:
        print("  ", o)
    return 0 if hits and not other else 1


if __name__ == "__main__":
    sys.exit(main())
