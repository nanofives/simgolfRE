"""Print an instruction range of an original function as a VC6 `__asm { }` block, with labels for jump targets.

Debug-build functions whose loops use only registers were written with inline assembly; the C part around them
is written by hand and the block is pasted from here. Operands stay literal (`dword ptr [ebp - 0x18]` is valid
inline asm), so the block assembles to the same bytes.

    py -3.12 re/tools/asm2inline.py jgld.dll 0x1000f9e0 --from 0x1000fac3 --to 0x1000faf0
    py -3.12 re/tools/asm2inline.py jgld.dll 0x1000f9e0 --list          # numbered listing to pick the range

A function whose body has several ranges in re/functions_ghidra*.tsv (loadInterfaceArt 0x00442180 has four) is read
range by range, so the listing reaches the last range. Bytes capstone cannot decode (jump tables inside a range) are
emitted as `_emit` lines instead of ending the listing."""
import argparse, pathlib, re, sys

import capstone
import pefile

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "re" / "tools"))
import match  # noqa: E402

# the string instructions MASM takes without operands (`rep stosd`); movsx / movzx / cmpsd-SSE are not among them
STRING_OPS = {f"{p}{s}" for p in ("stos", "movs", "lods", "scas", "cmps") for s in ("b", "w", "d")}


def is_string_op(mnemonic: str, op_str: str) -> bool:
    """True for stosb/movsd/... with or without a rep prefix; an SSE `movsd xmm0, ...` has xmm operands and is not."""
    return mnemonic.split()[-1] in STRING_OPS and "xmm" not in op_str


def function_ranges(module: str, fva: int):
    """[(lo, hi)] byte ranges of the function (hi exclusive): its Ghidra ranges when the list has several, else one
    block of ghidra_size bytes from the entry."""
    import xref
    for va, _name, size, rngs in xref.functions(module):
        if va == fva:
            return sorted(rngs) if len(rngs) > 1 else [(fva, fva + size)]
    size = match.ghidra_size(module, fva)
    return [(fva, fva + size)]


def decode(pe, base, ranges):
    """Instructions of every range in address order. Undecodable bytes come back as Insn('.byte', '0x..')."""
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    md.skipdata = True
    out = []
    for lo, hi in ranges:
        out.extend(md.disasm(pe.get_data(lo - base, hi - lo), lo))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("module")
    ap.add_argument("func")
    ap.add_argument("--from", dest="lo")
    ap.add_argument("--to", dest="hi", help="last instruction address (inclusive)")
    ap.add_argument("--list", action="store_true")
    a = ap.parse_args()
    pe = pefile.PE(str(ROOT / "original" / a.module), fast_load=True)
    base = match.image_base(a.module)
    fva = int(a.func, 16)
    ranges = function_ranges(a.module, fva)
    ins = decode(pe, base, ranges)
    if a.list or not a.lo:
        for i in ins:
            print(f"0x{i.address:08x}  {i.mnemonic} {i.op_str}")
        return
    lo, hi = int(a.lo, 16), int(a.hi, 16)
    rng = [i for i in ins if lo <= i.address <= hi]
    starts = {i.address for i in rng}
    hexes = lambda s: [int(x, 16) for x in re.findall(r"0x[0-9a-f]+", s)]
    # labels: branch targets, and code addresses used as values (lea edx, [label] for computed jumps)
    targets = {v for i in rng if i.mnemonic != ".byte" for v in hexes(i.op_str) if v in starts}
    names = {t: f"L{t & 0xffff:04x}" for t in targets}
    img_lo, img_hi = base, base + pe.OPTIONAL_HEADER.SizeOfImage
    inside = lambda v: any(l <= v < h for l, h in ranges)
    data = set()
    print("    __asm {")
    for i in rng:
        if i.address in names:
            print(f"    {names[i.address]}:")
        if i.mnemonic == ".byte":
            print(f"        _emit {i.op_str}")
            continue
        op = i.op_str
        if is_string_op(i.mnemonic, op):
            op = ""                     # string instructions: MASM takes the bare mnemonic (rep stosd)
        def sub(m):
            v = int(m[0], 16)
            if v in names:
                return names[v]
            if img_lo <= v < img_hi and not inside(v):
                data.add(v)
                return f"g_{v:08x}"
            return m[0]
        op = re.sub(r"0x[0-9a-f]+", sub, op)
        op = re.sub(r"\[(L[0-9a-f]{4})\]", r"\1", op)          # lea edx, [Lxxxx] -> lea edx, Lxxxx
        op = re.sub(r"\[(g_[0-9a-f]{8})\]", r"\1", op)
        print(f"        {i.mnemonic} {op}".rstrip())
    print("    }")
    if data:
        print("    // globals: " + " ".join(f"extern int g_{v:08x};" for v in sorted(data)))


if __name__ == "__main__":
    main()
