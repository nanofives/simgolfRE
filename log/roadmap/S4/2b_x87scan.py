"""S4.2b: which ported sound.dll functions execute x87 instructions in their own bodies (the user's sound.dll, the ranges of
re/functions_ghidra_sound.dll.tsv), by class: compares (they leave C0..C3 for the caller: the x87 exit state), control-word
loads, stack pushes / pops, arithmetic. With the x87 unit's exit state compared on every key, a port body whose original
compares must set the unit's condition codes the same way.

    ~/bin/py -3.12 log/roadmap/S4/2b_x87scan.py [--steps S4.2 S4.4 S4.6 S4.8] [--out log/roadmap/S4/2b_x87scan.tsv]
"""
import argparse
import csv
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "port" / "test"))
import sound_emu as E  # noqa: E402
import capstone  # noqa: E402

CMP = ("fcom", "fcomp", "fcompp", "fucom", "fucomp", "fucompp", "ftst", "fxam", "fcomi", "fcomip", "fucomi", "fucomip",
       "ficom", "ficomp")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--steps", nargs="*", default=["S4.2", "S4.4", "S4.6", "S4.8"])
    ap.add_argument("--out", default=str(ROOT / "log" / "roadmap" / "S4" / "2b_x87scan.tsv"))
    a = ap.parse_args()
    with open(ROOT / "re" / "functions_ghidra_sound.dll.tsv", newline="") as f:
        ranges = {int(r["entry"], 16): r["ranges"] for r in csv.DictReader(f, delimiter="\t")}
    with open(ROOT / "re" / "roadmap" / "S4_functions.tsv", newline="") as f:
        rows = [r for r in csv.DictReader(f, delimiter="\t") if r["step"] in a.steps]
    img = E.load_image(0x10000000)
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    out = []
    for r in rows:
        rva = int(r["rva"], 16)
        cmp_, cw, fpu, cmps = 0, 0, 0, []
        for rg in (ranges.get(rva) or "").split(";"):
            if not rg:
                continue
            lo, hi = (int(x, 16) for x in rg.split("-"))
            for i in md.disasm(bytes(img.data[lo:hi]), lo):
                if i.mnemonic.startswith("f") or i.mnemonic in ("emms",):
                    fpu += 1
                    if i.mnemonic in CMP:
                        cmp_ += 1
                        cmps.append(f"0x{i.address:x}:{i.mnemonic}")
                    if i.mnemonic in ("fldcw", "fninit", "finit"):
                        cw += 1
        if fpu:
            out.append((r["step"], r["name"], r["rva"], fpu, cmp_, cw, " ".join(cmps)))
    with open(a.out, "w", newline="") as f:
        w = csv.writer(f, delimiter="\t")
        w.writerow(["step", "name", "rva", "x87_instructions", "compares", "cw_loads", "compare_sites"])
        w.writerows(out)
    for o in out:
        print("\t".join(map(str, o[:6])))
    print(f"{len(out)} functions with x87 instructions, {sum(1 for o in out if o[4])} with compares -> {a.out}")


if __name__ == "__main__":
    main()
