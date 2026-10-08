"""Cross-module callers: which golf_clean.exe functions call each Terrain.dll export through the exe's import table.

xref.py only sees calls inside one module, so every exported Terrain method has `callers none` in hooks.csv even
though the exe calls it. This resolves each import of Terrain.dll in golf_clean.exe to the export's body RVA (the
export points at an incremental-link `jmp` thunk; the thunk is followed) and lists the exe functions that reach the
import slot. The exe calls Terrain through its own import thunks (`jmp dword ptr [slot]`, FF 25, named
`TERRAIN.DLL::...` in the master), so the callers are xref.py's callers of that thunk. Direct uses of the slot
(`call dword ptr [slot]`, FF 15; `mov reg, [slot]`) are added only when they sit inside a known function: the image has
three `call [0x4ba19c]` instructions in the padding after a `ret` (0x00475de6, 0x0047d834, 0x004b1ad6) that nothing
reaches, and those are reported as "outside any function" and never counted.

    py -3.12 re/tools/import_callers.py                 # one line per export: body RVA, name, exe callers
    py -3.12 re/tools/import_callers.py --rva 0x32f0    # one export, with every site
    py -3.12 re/tools/import_callers.py --callers-arg 0x32f0
        # prints the value for `c3_verify.py --callers <rva>=<value>` / `re_classify.py promote --callers`:
        # golf_clean.exe:0x00412345;golf_clean.exe:0x00423456 (module-qualified, see re_classify._identified)
"""
import argparse, csv, pathlib, struct, sys

import capstone
import pefile

ROOT = pathlib.Path(__file__).resolve().parents[2]
EXE, DLL = "golf_clean.exe", "Terrain.dll"


def exe_functions():
    fns = []
    for r in csv.DictReader(open(ROOT / "re" / "functions_ghidra.tsv"), delimiter="\t"):
        if int(r["size"]) <= 5:
            continue
        for rg in (r.get("ranges") or "").split(";"):
            if "-" in rg:
                lo, hi = (int(x, 16) for x in rg.split("-"))
                fns.append((lo, hi, int(r["entry"], 16), r["name"]))
    return fns


def containing(fns, va):
    for lo, hi, entry, name in fns:
        if lo <= va < hi:
            return entry, name
    return None, None


def export_bodies():
    """Export name -> body RVA (thunk followed)."""
    pe = pefile.PE(str(ROOT / "original" / DLL))
    out = {}
    for s in pe.DIRECTORY_ENTRY_EXPORT.symbols:
        rva = s.address
        b = pe.get_data(rva, 5)
        if b[0] == 0xE9:                                       # incremental-link thunk: jmp rel32
            rva = rva + 5 + struct.unpack("<i", b[1:5])[0]
        out[s.name] = rva
    return out


def import_slots():
    pe = pefile.PE(str(ROOT / "original" / EXE))
    slots = {}
    for e in pe.DIRECTORY_ENTRY_IMPORT:
        if e.dll.decode().lower() == DLL.lower():
            for i in e.imports:
                slots[i.address] = i.name
    return pe, slots


def sites(pe, slots):
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    found = {s: [] for s in slots}
    for sec in pe.sections:
        if not sec.Characteristics & 0x20000000:              # executable
            continue
        base = pe.OPTIONAL_HEADER.ImageBase + sec.VirtualAddress
        data = sec.get_data()
        for slot in slots:
            needle = struct.pack("<I", slot)
            i = data.find(needle)
            while i >= 0:
                # FF 15 disp32 (call [slot]), A1 disp32 (mov eax,[slot]), 8B modrm(00 reg 101) disp32 (mov reg,[slot])
                for back, kind in ((2, "call"), (2, "jmp"), (1, "load"), (2, "load")):
                    st = i - back
                    if st < 0:
                        continue
                    ins = next(md.disasm(data[st:st + 6], base + st), None)
                    if ins and ins.size == back + 4 and f"0x{slot:x}" in ins.op_str and (
                            (kind == ins.mnemonic) or (kind == "load" and ins.mnemonic == "mov")):
                        found[slot].append((base + st, kind, f"{ins.mnemonic} {ins.op_str}"))
                        break
                i = data.find(needle, i + 1)
    return found


def table():
    sys.path.insert(0, str(ROOT / "re" / "tools"))
    import xref
    idx = xref.load(EXE)
    bodies = export_bodies()
    pe, slots = import_slots()
    fns = exe_functions()
    found = sites(pe, slots)
    rows = []
    for slot, name in slots.items():
        rva = bodies.get(name)
        ss = []
        for va, kind, text in found[slot]:
            if kind == "jmp":                                   # the exe's import thunk: its callers are the users
                for c in idx.get(va, {}).get("callers", []):
                    entry, fname = containing(fns, c)
                    ss.append((c, "via", f"call thunk 0x{va:08x}", entry, fname))
            else:
                entry, fname = containing(fns, va)
                ss.append((va, kind, text, entry, fname))
        rows.append((rva, name.decode(), slot, ss))
    return sorted(rows, key=lambda r: (r[0] is None, r[0] or 0))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--rva", help="one export by body RVA")
    ap.add_argument("--callers-arg", help="print the module-qualified callers value for this body RVA")
    a = ap.parse_args()
    want = int(a.rva or a.callers_arg or "0", 16) or None
    for rva, name, slot, ss in table():
        if want is not None and rva != want:
            continue
        callers = sorted({e for *_, e, _ in ss if e is not None})
        if a.callers_arg:
            print(";".join(f"{EXE}:0x{c:08x}" for c in callers))
            return 0
        print(f"{DLL}:{rva:08x}  {name}  slot 0x{slot:x}  callers {len(callers)}: "
              + (", ".join(f"0x{c:08x}" for c in callers) or "none"))
        if want is not None:
            for va, kind, text, entry, fname in ss:
                where = f"in {fname} 0x{entry:08x}" if entry is not None else "outside any function (not counted)"
                print(f"    0x{va:08x} {kind:4} {text:40} {where}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
