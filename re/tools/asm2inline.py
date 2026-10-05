"""Print an instruction range of an original function as a VC6 `__asm { }` block, with labels for jump targets.

Debug-build functions whose loops use only registers were written with inline assembly; the C part around them
is written by hand and the block is pasted from here. Operands stay literal (`dword ptr [ebp - 0x18]` is valid
inline asm), so the block assembles to the same bytes.

    py -3.12 re/tools/asm2inline.py jgld.dll 0x1000f9e0 --from 0x1000fac3 --to 0x1000faf0
    py -3.12 re/tools/asm2inline.py jgld.dll 0x1000f9e0 --list          # numbered listing to pick the range"""
import argparse, pathlib, re, sys

import capstone
import pefile

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "re" / "tools"))
import match  # noqa: E402


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
    size = match.ghidra_size(a.module, fva)
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    ins = list(md.disasm(pe.get_data(fva - base, size), fva))
    if a.list or not a.lo:
        for i in ins:
            print(f"0x{i.address:08x}  {i.mnemonic} {i.op_str}")
        return
    lo, hi = int(a.lo, 16), int(a.hi, 16)
    rng = [i for i in ins if lo <= i.address <= hi]
    starts = {i.address for i in rng}
    hexes = lambda s: [int(x, 16) for x in re.findall(r"0x[0-9a-f]+", s)]
    # labels: branch targets, and code addresses used as values (lea edx, [label] for computed jumps)
    targets = {v for i in rng for v in hexes(i.op_str) if v in starts}
    names = {t: f"L{t & 0xffff:04x}" for t in targets}
    img_lo, img_hi = base, base + pe.OPTIONAL_HEADER.SizeOfImage
    data = set()
    print("    __asm {")
    for i in rng:
        if i.address in names:
            print(f"    {names[i.address]}:")
        op = i.op_str
        if i.mnemonic.split()[-1][:4] in ("stos", "movs", "lods", "scas", "cmps") and len(i.mnemonic.split()[-1]) == 5:
            op = ""                     # string instructions: MASM takes the bare mnemonic (rep stosd)
        def sub(m):
            v = int(m[0], 16)
            if v in names:
                return names[v]
            if img_lo <= v < img_hi and not (fva <= v < fva + size):
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
