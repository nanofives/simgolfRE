"""Installers of functions that are reached only through a stored pointer (vtable slot, callback field, function table).

The gate's caller rule needs an identified caller, and xref.py only sees `call` instructions, so a function called
through a pointer has `callers none` in hooks.csv. Rounds 16-19 promoted such functions by hand with
`c3_verify.py --callers <addr>=<installer>` after checking the image (prepare_for_pass, finish_pass_master, falloc,
closeHookC). This tool does that check mechanically, for every module:

  1. the function's targets are its body address plus every incremental-link thunk (`jmp rel32`, size 5 in
     re/functions_ghidra*.tsv) that jumps to it (the debug DLLs store thunk addresses, not body addresses);
  2. every aligned or unaligned dword in the mapped image equal to a target is a reference;
  3. a reference inside a function's code ranges is a STORE (e.g. `mov [edi], 0x4b5810`): that function installs it;
  4. a reference in data is a TABLE slot: walking back through dwords that point into the module's code, the table
     start is the nearest address that some function's code holds (a constructor's `mov [reg], <vtable>`); those
     functions install the table. The slot index is reported. A slot whose table no code references is reported with
     no installer.

    py -3.12 re/tools/pointer_callers.py <module> 0x<addr>          # every reference, with kind / slot / installer
    py -3.12 re/tools/pointer_callers.py <module> --callers-arg 0x<addr>
        # the value for `re_classify.py promote --callers` (VAs; the gate converts DLL VAs to RVAs)
    py -3.12 re/tools/pointer_callers.py --survey                   # counts over every C2 row with `callers none`

<addr> is a VA for golf_clean.exe and an RVA or a VA for a DLL. An installer is evidence that the function is
reachable, not that it runs; the Purpose of a function promoted this way cites the store or the table slot.
"""
import argparse, bisect, csv, pathlib, struct, sys

import capstone
import pefile

ROOT = pathlib.Path(__file__).resolve().parents[2]
FILES = {"golf_clean.exe": "re/functions_ghidra.tsv", "Terrain.dll": "re/functions_ghidra_Terrain.dll.tsv",
         "jgld.dll": "re/functions_ghidra_jgld.dll.tsv", "sound.dll": "re/functions_ghidra_sound.dll.tsv"}
_CACHE = {}


class Module:
    def __init__(self, name):
        self.name = name
        pe = pefile.PE(str(ROOT / "original" / name), fast_load=True)
        self.base = pe.OPTIONAL_HEADER.ImageBase
        self.img = pe.get_memory_mapped_image()
        self.text = [(self.base + sec.VirtualAddress, self.base + sec.VirtualAddress + sec.Misc_VirtualSize)
                     for sec in pe.sections if sec.Characteristics & 0x20000000]
        self.ranges, self.entries, self.thunks = [], {}, {}
        for r in csv.DictReader(open(ROOT / FILES[name]), delimiter="\t"):
            e = int(r["entry"], 16)
            e = e if name == "golf_clean.exe" or e >= 0x10000000 else e + self.base   # DLL tsv rows are VAs or RVAs
            self.entries[e] = r["name"]
            if int(r["size"]) <= 5 and self.img[e - self.base] == 0xE9:
                # an incremental-link / tail thunk: index it by its target. Other bodies of 5 bytes or less (`ret`,
                # `xor eax, eax; ret 4`) are real functions and keep their range (round 35: endstate.py read
                # return0Pop4_487b00 as 0 bytes with 0 rets before this)
                o = e - self.base
                self.thunks.setdefault(e + 5 + struct.unpack("<i", self.img[o + 1:o + 5])[0], []).append(e)
                continue
            for rg in (r.get("ranges") or "").split(";"):
                if "-" in rg:
                    lo, hi = (int(x, 16) for x in rg.split("-"))
                    if name != "golf_clean.exe" and lo < 0x10000000:
                        lo, hi = lo + self.base, hi + self.base
                    self.ranges.append((lo, hi, e))
        # The debug DLLs open .text with the incremental-link thunk table: consecutive 5-byte `jmp rel32`, not all of
        # which are functions in the Ghidra listing. Read it directly so a slot holding any thunk resolves to its body.
        if name != "golf_clean.exe" and self.text:
            o = self.text[0][0] - self.base
            while o < len(self.img) and self.img[o] in (0xE9, 0xCC):
                if self.img[o] == 0xE9:
                    t = self.base + o + 5 + struct.unpack("<i", self.img[o + 1:o + 5])[0]
                    if self.base + o not in self.thunks.get(t, []):
                        self.thunks.setdefault(t, []).append(self.base + o)
                    o += 5
                else:
                    o += 1
        self.ranges.sort()
        self._starts = [r[0] for r in self.ranges]
        self._decoded = {}

    def function_at(self, va):
        i = bisect.bisect_right(self._starts, va) - 1
        if i >= 0 and self.ranges[i][0] <= va < self.ranges[i][1]:
            return self.ranges[i][2]
        return None

    def code_ref(self, va):
        """The function whose code holds `va` as the immediate or displacement of a decoded instruction, else None.
        A dword that only matches across instruction bytes (simulateShot 0x424120 holds 0x004bb904 at 0x004242ef,
        across `test` and `mov ecx, 0x4b`, round 27) is not a store. The function's range is decoded linearly from its start."""
        i = bisect.bisect_right(self._starts, va) - 1
        if i < 0 or not (self.ranges[i][0] <= va < self.ranges[i][1]):
            return None
        lo, hi, entry = self.ranges[i]
        if (lo, hi) not in self._decoded:
            md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
            md.detail = True
            md.skipdata = True
            ops = {}
            for ins in md.disasm(self.img[lo - self.base:hi - self.base], lo):
                if ins.id == 0:          # skipdata
                    continue
                for off in (ins.imm_offset, ins.disp_offset):
                    if off:
                        ops[ins.address + off] = ins.address
            self._decoded[(lo, hi)] = ops
        return entry if va in self._decoded[(lo, hi)] else None

    def in_code(self, v):
        return any(lo <= v < hi for lo, hi in self.text)

    def find_all(self, value):
        needle, out, i = struct.pack("<I", value), [], self.img.find(struct.pack("<I", value))
        while i >= 0:
            out.append(self.base + i)
            i = self.img.find(needle, i + 1)
        return out

    def dword(self, va):
        o = va - self.base
        return struct.unpack("<I", self.img[o:o + 4])[0] if 0 <= o <= len(self.img) - 4 else None


def mod(name):
    if name not in _CACHE:
        _CACHE[name] = Module(name)
    return _CACHE[name]


def references(m, body):
    """[(ref_va, kind, detail, installers)] for the function whose body starts at VA `body`."""
    out = []
    for t in [body] + m.thunks.get(body, []):
        for ref in m.find_all(t):
            owner = m.function_at(ref)
            if owner is not None:
                if m.code_ref(ref) is not None:
                    out.append((ref, "store", f"target 0x{t:08x} written by code", [owner]))
                continue            # bytes inside code that are not an operand: a coincidental match
            # walk back through slots that point into code; the table starts at the nearest address that code
            # itself stores (a constructor's `mov [reg], <table>`), so a neighbouring table is never crossed
            start, installers = ref, []
            while m.dword(start) is not None and m.in_code(m.dword(start)):
                installers = sorted({f for f in map(m.code_ref, m.find_all(start)) if f is not None})
                if installers:
                    break
                start -= 4
            if not installers and m.dword(start) == 0:
                # a NULL-delimited table such as the CRT's C++ dynamic-initializer table (__xc_a 0x4c1000 .. __xc_z
                # 0x4c1188 in the exe): its first slot follows a NULL whose address the walker (__cinit) loads. In the
                # debug DLLs the .CRT$XC* groups leave runs of NULL slots between __xc_a and the first initializer
                # (jgld: __cinit 0x1007f160 loads __xc_a 0x10122000, the first slot is 0x10122104), so keep walking back
                # through NULL and code-pointer slots, at most 0x400 bytes, to the first address code loads.
                probe = start
                while not installers and start - probe < 0x400 and m.dword(probe) is not None \
                        and (m.dword(probe) == 0 or m.in_code(m.dword(probe))):
                    installers = sorted({f for f in map(m.code_ref, m.find_all(probe)) if f is not None})
                    if installers:
                        start = probe
                        break
                    probe -= 4
            if not installers:
                out.append((ref, "table", "in data; no code stores the address of any slot at or before it", []))
                continue
            slot = (ref - start) // 4
            out.append((ref, "table", f"slot {slot} (+0x{slot * 4:x}) of the table at 0x{start:08x}", installers))
    return out


def norm(m, addr):
    a = int(addr, 16)
    return a + m.base if m.name != "golf_clean.exe" and a < 0x10000000 else a


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("module", nargs="?")
    ap.add_argument("addr", nargs="?")
    ap.add_argument("--callers-arg")
    ap.add_argument("--survey", action="store_true")
    a = ap.parse_args()
    if a.survey:
        rows = [r for r in csv.DictReader(open(ROOT / "hooks.csv"))
                if r["confidence"] == "C2" and r["callers"] in ("", "none")]
        for name in FILES:
            m, n, found = mod(name), 0, 0
            for r in rows:
                if (r["module"] or "golf_clean.exe") != name:
                    continue
                n += 1
                if any(inst for *_, inst in references(m, norm(m, r["addr"]))):
                    found += 1
            print(f"{name}: {n} C2 rows with `callers none`, {found} with an identified installer")
        return 0
    m = mod(a.module)
    body = norm(m, a.callers_arg or a.addr)
    refs = references(m, body)
    if a.callers_arg:
        print(";".join(sorted({f"0x{i:08x}" for *_, inst in refs for i in inst})))
        return 0
    if not refs:
        print("no stored pointer to this function or its thunks")
    for ref, kind, detail, inst in refs:
        print(f"0x{ref:08x} {kind:5} {detail}; installers: "
              + (", ".join(f"0x{i:08x} {m.entries.get(i, '?')}" for i in inst) or "none found"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
