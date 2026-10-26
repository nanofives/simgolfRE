"""S5.2: the colour words the exe's text calls pass, read from the call sites (static scan of golf_clean.exe).

For every `call rel32` to one of the text drawers (colour = argument 4, the first push of the four) and to Widget_setQuad
(thiscall, colour = argument 1 = the last push, shadow = argument 2), the pushes before the call are walked back inside a
window of 24 instructions; an immediate push gives the word, a register or memory push gives `?`. Output: a TSV of
(callee, site, colour, shadow) and a count per colour word.

    ~/bin/py -3.12 log/roadmap/S5/2_colours.py > log/roadmap/S5/2_colours.tsv
"""
import collections
import pathlib
import sys

import capstone
import pefile

ROOT = pathlib.Path(__file__).resolve().parents[3]
EXE = ROOT / "original" / "golf_clean.exe"
# name -> (argument count, colour argument index 0-based, shadow index or None, fixed shadow word when the drawer sets it)
TARGETS = {
    0x004049d0: ("drawText4049d0", 4, 3, None, -1),
    0x00404a20: ("drawTextScaled", 4, 3, None, -1),
    0x00404ad0: ("drawTextSurface", 4, 3, None, 1),
    0x00404b70: ("drawCenteredText", 4, 3, None, -1),
    0x00404bc0: ("drawCenteredText2", 4, 3, None, 1),
    0x00476310: ("Widget_setQuad", 4, 0, 1, None),
}


def main():
    pe = pefile.PE(str(EXE), fast_load=True)
    text = next(s for s in pe.sections if s.Name.rstrip(b"\0") == b".text")
    base = pe.OPTIONAL_HEADER.ImageBase + text.VirtualAddress
    code = text.get_data()
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    rows = []
    for ins_list in function_insns(md, code, base):
        scan(ins_list, rows)
    emit(rows)


def function_insns(md, code, base):
    """every function of re/functions_ghidra.tsv, each range disassembled from its start"""
    import csv
    with (ROOT / "re" / "functions_ghidra.tsv").open(newline="") as fh:
        for r in csv.DictReader(fh, delimiter="	"):
            for rng in r["ranges"].replace(";", ",").split(","):
                lo, hi = (int(x, 16) for x in rng.split("-"))
                if lo < base or hi > base + len(code):
                    continue
                yield list(md.disasm(code[lo - base:hi - base], lo))


def scan(insns, rows):
    for i, ins in enumerate(insns):
        if ins.mnemonic != "call" or not ins.op_str.startswith("0x"):
            continue
        t = int(ins.op_str, 16)
        if t not in TARGETS:
            continue
        name, nargs, ci, si, fixed_shadow = TARGETS[t]
        pushes = []   # nearest first
        for j in range(i - 1, max(i - 24, 0), -1):
            p = insns[j]
            if p.mnemonic in ("call", "ret", "jmp") or p.mnemonic.startswith("j"):
                break
            if p.mnemonic == "push":
                pushes.append(p.op_str)
                if len(pushes) == nargs:
                    break
        def arg(k):
            if k is None or k >= len(pushes):
                return "?"
            s = pushes[k]
            try:
                return "0x%08x" % (int(s, 16) & 0xFFFFFFFF)
            except ValueError:
                return "?"
        colour = arg(ci)
        shadow = arg(si) if si is not None else "0x%08x" % (fixed_shadow & 0xFFFFFFFF)
        rows.append((name, "0x%08x" % ins.address, colour, shadow))


def emit(rows):
    print("callee\tsite\tcolour\tshadow")
    for r in rows:
        print("\t".join(r))
    cnt = collections.Counter((r[2], r[3]) for r in rows)
    print("# colour/shadow counts: " + ", ".join(f"{c}/{s} x{n}" for (c, s), n in cnt.most_common()), file=sys.stderr)
    print(f"# {len(rows)} call sites", file=sys.stderr)


if __name__ == "__main__":
    main()
