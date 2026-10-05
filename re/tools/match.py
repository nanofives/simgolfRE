"""Matching decompilation check: compile annotated C++ with the ORIGINAL compiler (VC6, cl 12.00.8168, the
build in the game's Rich headers) and compare each function with the bytes in the anchored binary.

    py -3.12 re/tools/match.py re/match/terrain.cpp            # all MATCH annotations in the file
    py -3.12 re/tools/match.py re/match/terrain.cpp --flags "/O2" --show

Source annotations (one per function, on the line before its definition):
    // MATCH: <module> <0xaddress> <decorated-or-plain symbol name>
Flags, per module (or one line for all; --flags overrides):
    // FLAGS golf_clean.exe: /O2
    // FLAGS Terrain.dll: /Od /ZI /GZ
Language: `// LANG c` compiles the whole file as C (/Tc; MATCH names are then `_name`); default C++ (/Tp).
Modules: golf_clean.exe, Terrain.dll, jgld.dll, sound.dll (function lists in re/functions_ghidra*.tsv).
`--only 0x<addr>` compares one annotated function (used by re/tools/match_permute.py).

Comparison: the function's bytes come from its own COMDAT section (/Gy). Every relocation site in the obj
is masked on both sides (its absolute target differs by construction). Then both are disassembled and
compared instruction by instruction; accuracy = matching instructions / max(len). 100% is byte-identical
apart from relocated operands, i.e. the source IS the original code for this compiler.
Writes log/diff/<addr>_<name>.match.csv with VERDICT GREEN only at 100%.
"""
from __future__ import annotations

import argparse
import csv
import difflib
import pathlib
import re
import struct
import subprocess
import sys
import tempfile

import capstone
import pefile

ROOT = pathlib.Path(__file__).resolve().parents[2]
VC = ROOT / "tools" / "vc6" / "vc98"
GAME = ROOT / "original"
DEFAULT_FLAGS = "/O2"
MD = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)


# ------------------------------------------------------------------ COFF
def coff(path: pathlib.Path):
    d = path.read_bytes()
    machine, nsec, _, symptr, nsym, opt, _ = struct.unpack_from("<HHIIIHH", d, 0)
    strtab = symptr + nsym * 18
    def name(raw):
        if raw[:4] == b"\0\0\0\0":
            off = struct.unpack_from("<I", raw, 4)[0]
            return d[strtab + off:d.index(b"\0", strtab + off)].decode()
        return raw.rstrip(b"\0").decode()
    secs = []
    for i in range(nsec):
        o = 20 + opt + 40 * i
        n = name(d[o:o + 8])
        size, rawptr, relptr, _, nrel = struct.unpack_from("<IIIIH", d, o + 16)
        rels = [struct.unpack_from("<IIH", d, relptr + 10 * k) for k in range(nrel)]
        secs.append({"name": n, "data": d[rawptr:rawptr + size], "relocs": rels})
    syms, i = [], 0
    while i < nsym:
        o = symptr + 18 * i
        n = name(d[o:o + 8])
        value, secnum, typ, cls, naux = struct.unpack_from("<IhHBB", d, o + 8)
        syms.append({"name": n, "value": value, "sec": secnum, "type": typ, "cls": cls, "index": i})
        i += 1 + naux
    return secs, syms


def compile_obj(src: pathlib.Path, flags: str, lang: str = "c++") -> pathlib.Path:
    """`lang` "c" compiles the file as C (/Tc, undecorated `_name` symbols), whatever its extension."""
    out = pathlib.Path(tempfile.mkdtemp(prefix="vc6_")) / (src.stem + ".obj")
    env = {"PATH": str(VC / "bin"), "INCLUDE": str(VC / "include"), "LIB": str(VC / "lib"),
           "SystemRoot": r"C:\Windows", "TEMP": tempfile.gettempdir(), "TMP": tempfile.gettempdir()}
    srcarg = f"/Tc{src}" if lang == "c" else f"/Tp{src}"
    cmd = [str(VC / "bin" / "cl.exe"), "/nologo", "/c", "/Gy", *flags.split(), f"/Fo{out}", f"/Fd{out.parent}\\", srcarg]
    r = subprocess.run(cmd, capture_output=True, text=True, env=env)
    if r.returncode != 0:
        raise SystemExit(f"VC6 compile failed:\n{r.stdout}{r.stderr}")
    return out


# ------------------------------------------------------------------ compare
def function_from_obj(secs, syms, wanted: str):
    """(bytes, relocation offsets, symbol name, same-section reloc targets) for the function whose symbol matches `wanted` (exact or substring)."""
    # an exact name wins over a substring hit (`_inflate` is a substring of `_inflateReset`)
    cands = [s for s in syms if s["sec"] > 0 and s["type"] == 0x20 and s["name"] == wanted] or \
            [s for s in syms if s["sec"] > 0 and s["type"] == 0x20 and wanted in s["name"]]
    if not cands:
        raise SystemExit(f"symbol {wanted!r} not in obj; functions: {[s['name'] for s in syms if s['type'] == 0x20]}")
    s = cands[0]
    sec = secs[s["sec"] - 1]
    start = s["value"]
    later = sorted(x["value"] for x in syms if x["sec"] == s["sec"] and x["type"] == 0x20 and x["value"] > start)
    end = later[0] if later else len(sec["data"])
    by_index = {x["index"]: x for x in syms}
    # Relocations against ABSOLUTE symbols (section -1) are constants, not addresses: VC6 writes fs:[0] as
    # fs:__except_list, an absolute symbol of value 0 that still carries a relocation.
    # __except_list is an EXTERNAL symbol resolved by VC6's libc to the absolute value 0 (fs:[0] = TIB head).
    const_syms = {"___except_list", "__except_list"}

    def is_const(symidx):
        sym = by_index.get(symidx, {})
        return sym.get("sec") == -1 or sym.get("name") in const_syms

    relocs = [off - start for off, symidx, _ in sec["relocs"] if start <= off < end and not is_const(symidx)]
    # relocation site -> target offset inside this function's slice, for targets in the same section
    # (VC6 switch tables are addressed through local label symbols with a stored addend of 0)
    local = {off - start: by_index[symidx]["value"] - start for off, symidx, _ in sec["relocs"]
             if start <= off < end and by_index.get(symidx, {}).get("sec") == s["sec"]}
    return sec["data"][start:end], relocs, s["name"], local


TSV = {"golf_clean.exe": "functions_ghidra.tsv", "Terrain.dll": "functions_ghidra_Terrain.dll.tsv",
       "jgld.dll": "functions_ghidra_jgld.dll.tsv", "sound.dll": "functions_ghidra_sound.dll.tsv"}
_BASES = {}


def image_base(module: str) -> int:
    if module not in _BASES:
        _BASES[module] = pefile.PE(str(GAME / module), fast_load=True).OPTIONAL_HEADER.ImageBase
    return _BASES[module]


def tracker_key(module: str, addr: int) -> int:
    """VA for golf_clean.exe, RVA for the DLLs (the function lists and hooks.csv use the same keys)."""
    if module == "golf_clean.exe":
        return addr
    return addr - image_base(module) if addr >= image_base(module) else addr


BOUNDARIES = ROOT / "re" / "match" / "boundaries.tsv"


def boundary_overrides(module: str) -> dict[int, int]:
    """{tracker key: size} for functions whose Ghidra extent is wrong (re/match/boundaries.tsv: two functions merged
    because the first ends in a noreturn call, or an entry Ghidra does not have)."""
    out = {}
    if BOUNDARIES.is_file():
        for line in BOUNDARIES.read_text().splitlines()[1:]:
            c = line.split("	")
            if len(c) >= 3 and c[0] == module:
                out[tracker_key(module, int(c[1], 16))] = int(c[2])
    return out


def ghidra_size(module: str, addr: int) -> int | None:
    """Function body size from Ghidra (independent of our recompile, so a shorter body cannot hide), capped at the
    next function's entry (Ghidra counts non-contiguous chunks in a body's size); boundaries.tsv overrides it."""
    f = ROOT / "re" / TSV.get(module, "")
    if not f.is_file():
        return None
    key = tracker_key(module, addr)
    ov = boundary_overrides(module)
    if key in ov:
        return ov[key]
    rows = [line.split("	") for line in f.read_text().splitlines()[1:]]
    entries = sorted(set(int(c[0], 16) for c in rows) | set(ov))
    for c in rows:
        if int(c[0], 16) == key:
            size = int(c[2])
            later = [e for e in entries if e > key]
            return min(size, later[0] - key) if later else size
    return None


def original_bytes(module: str, addr: int, n: int) -> bytes:
    pe = pefile.PE(str(GAME / module), fast_load=True)
    rva = addr - pe.OPTIONAL_HEADER.ImageBase if addr >= pe.OPTIONAL_HEADER.ImageBase else addr
    return pe.get_data(rva, n)


def normalize(code: bytes, base: int, start: int, is_abs, obj: bool = False) -> list[tuple[str, str]]:
    """Disassemble and normalize: absolute addresses -> <addr>, calls -> <fn>, internal branch targets
    relative to the function start. `is_abs(ins, operand_index, value)` says whether a value is an address.
    Returns [(display text, comparison key)]."""
    out = []
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    md.detail = True
    for ins in md.disasm(code, base):
        m = ins.mnemonic
        if m == "call" and ins.bytes[0] == 0xE8:
            key = "call <fn>"
        elif ins.bytes[0] == 0xE9 and ins.operands and (
                is_abs(ins, 0, ins.operands[0].imm) if obj
                else not (start <= ins.operands[0].imm < start + len(code))):
            key = "jmp <fn>"            # tail call: relocated in the obj / leaves the original body; like a call
        elif ins.bytes[0] in (0xE8, 0xE9, 0xEB, 0xE0, 0xE1, 0xE2, 0xE3) or (ins.bytes[0] in range(0x70, 0x80)) or (ins.bytes[0] == 0x0F and ins.bytes[1] in range(0x80, 0x90)):
            # (0xE0-0xE3: loopne/loope/loop/jecxz, short relative branches in inline-asm loops)
            tgt = ins.operands[0].imm if ins.operands else 0
            key = f"{m} +0x{tgt - start:x}"
        else:
            ops = []
            for i, op in enumerate(ins.operands):
                if op.type == capstone.x86.X86_OP_IMM:
                    ops.append("<addr>" if is_abs(ins, i, op.imm & 0xFFFFFFFF) else hex(op.imm & 0xFFFFFFFF))
                elif op.type == capstone.x86.X86_OP_MEM:
                    mem = op.mem
                    disp = mem.disp & 0xFFFFFFFF
                    parts = [ins.reg_name(mem.base) if mem.base else "", ins.reg_name(mem.index) + f"*{mem.scale}" if mem.index else ""]
                    # an address displacement is masked with or without a base register (`mov cl, [edx+tbl]`)
                    d = "<addr>" if is_abs(ins, i, disp) else (hex(mem.disp) if mem.disp else "")
                    ops.append(f"{op.size}[" + "+".join(x for x in parts + [d] if x) + "]")
                else:
                    ops.append(ins.reg_name(op.reg))
            key = m + " " + ",".join(ops)
        out.append((f"{m} {ins.op_str}", key))
    return out


def strip_switch_tables(code: bytes, local: dict) -> bytes:
    """VC6 places a switch's jump table (and its index-byte table) right after the function's code, inside
    the obj symbol's range; Ghidra's function size stops before them. `local` maps relocation sites to their
    same-section targets (slice offsets), so the code ends at the lowest target the body points into."""
    # a jump table's first entry is itself relocated (it points back into the code); a label taken as a value
    # (inline asm `lea edx, label` for a computed jump) points at plain code and must not cut the body
    targets = [t for site, t in local.items() if site < t < len(code) and t in local]
    return code[:min(targets)] if targets else code


def switch_table_mismatches(mine: bytes, local: dict, orig_tail: bytes, addr: int) -> list[str]:
    """Compares the switch tables VC6 placed after the code (which strip_switch_tables cut off): every 4-byte
    entry relocated to a label in our obj must point at the same function offset as the original entry, and
    every other byte (index tables) must be equal. Only meaningful when the code itself matched, so both
    tables start at the same offset. Trailing int3/nop padding is ignored."""
    cut = len(strip_switch_tables(mine, local))
    tail = mine[cut:]
    bad, i = [], 0
    while i < len(tail):
        site = cut + i
        if site in local:
            want = int.from_bytes(orig_tail[i:i + 4], "little") - addr
            if want != local[site]:
                bad.append(f"entry @+0x{site:x}: original -> +0x{want:x}, ours -> +0x{local[site]:x}")
            i += 4
            continue
        if tail[i] in (0xCC, 0x90) and all(b in (0xCC, 0x90) for b in tail[i:]):
            break
        if i < len(orig_tail) and tail[i] != orig_tail[i]:
            bad.append(f"byte @+0x{site:x}: original {orig_tail[i]:#04x}, ours {tail[i]:#04x}")
        i += 1
    return bad


def file_flags(text: str, module: str) -> str | None:
    m = re.search(rf"//\s*FLAGS\s+{re.escape(module)}:\s*(.+)", text) or re.search(r"//\s*FLAGS:\s*(.+)", text)
    return m.group(1).strip() if m else None


_RELOCS = {}


def image_relocs(mod: str):
    """(SizeOfImage, set of VAs that carry a base relocation) of an original module (empty set for the exe)."""
    if mod not in _RELOCS:
        pe = pefile.PE(str(GAME / mod), fast_load=True)
        pe.parse_data_directories(directories=[pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_BASERELOC"]])
        base = pe.OPTIONAL_HEADER.ImageBase
        sites = {base + e.rva for b in getattr(pe, "DIRECTORY_ENTRY_BASERELOC", []) for e in b.entries if e.type}
        _RELOCS[mod] = (pe.OPTIONAL_HEADER.SizeOfImage, sites)
    return _RELOCS[mod]


def score_function(mod: str, addr: int, secs, syms, sym: str):
    """Compare one obj function (by symbol) with the original at `addr`: (acc, a, b, full, same, table_bad, sm)."""
    mine, relocs, full, local = function_from_obj(secs, syms, sym)
    size = ghidra_size(mod, addr)
    if size is None:
        raise SystemExit(f"no Ghidra size for {mod} 0x{addr:08x}; refusing to guess the original's length")
    orig = original_bytes(mod, addr, size)
    reloc_sites = set(relocs)

    def mine_abs(ins, i, v, _r=reloc_sites):
        """True only for the operand whose encoded field carries a relocation (not every operand of an
        instruction that has one: `cmp [global], 0` keeps its immediate 0)."""
        op = ins.operands[i]
        if op.type == capstone.x86.X86_OP_IMM and ins.imm_offset:
            return ins.address + ins.imm_offset in _r
        if op.type == capstone.x86.X86_OP_MEM and ins.disp_offset:
            return ins.address + ins.disp_offset in _r
        return False

    pe_base = image_base(mod)
    img_size, orig_relocs = image_relocs(mod)
    first_sec = min(s.VirtualAddress for s in pefile.PE(str(GAME / mod), fast_load=True).sections)

    def orig_abs(ins, i, v):
        # a DLL's base relocations say exactly which fields are addresses (jgld.dll's `or ecx, 0x10000000` is
        # FILE_FLAG_RANDOM_ACCESS, equal to its image base); the exe has none, so there the range test stands
        if orig_relocs:
            op = ins.operands[i]
            off = ins.imm_offset if op.type == capstone.x86.X86_OP_IMM else ins.disp_offset
            return bool(off) and ins.address + off in orig_relocs
        # no relocations (the exe): an address points into a section; the image base itself is the PE header,
        # which code reaches only as a constant (`test eax, 0x400000` in 0x47d610 is a flag)
        return pe_base + first_sec <= v < pe_base + img_size

    a = normalize(strip_switch_tables(mine, local), 0, 0, mine_abs, obj=True)
    b = normalize(orig, addr, addr, orig_abs)
    # MSVC pads functions with int3/nop up to 16 bytes; padding is not part of either body
    # (and VC6 aligns a trailing switch table with multi-byte no-ops: lea ecx,[ecx] / mov edi,edi / lea esi,[esi])
    while a and (a[-1][0].split()[0] in ("int3", "nop")
                 or a[-1][0] in ("lea ecx, [ecx]", "mov edi, edi", "lea esi, [esi]", "lea esp, [esp]")):
        a.pop()
    sm = difflib.SequenceMatcher(None, [x[1] for x in a], [x[1] for x in b], autojunk=False)
    same = sum(bl.size for bl in sm.get_matching_blocks())
    acc = same / max(len(a), len(b))
    table_bad = []
    if acc == 1:
        cut = len(strip_switch_tables(mine, local))
        if cut < len(mine):
            table_bad = switch_table_mismatches(mine, local, original_bytes(mod, addr + cut, len(mine) - cut), addr)
            if table_bad:
                acc = 0.999                  # instructions equal, switch dispatch differs: not a match
    return acc, a, b, full, same, table_bad, sm


def compare(src: pathlib.Path, flags: str, show: bool, only: int | None = None, quiet: bool = False):
    """Per-module flags: `// FLAGS <module>: ...` (one source can match a release exe and a debug DLL).
    `// LANG c` compiles the whole file as C. `only` restricts to one annotated address."""
    text = src.read_text()
    anns = re.findall(r"//\s*MATCH:\s*(\S+)\s+(0x[0-9a-fA-F]+)\s+(\S+)", text)
    lang = "c" if re.search(r"//\s*LANG\s+c\b", text) else "c++"
    objs = {}
    results = []
    for mod, addr, sym in anns:
        if only is not None and int(addr, 16) != only:
            continue
        fl = flags or file_flags(text, mod) or DEFAULT_FLAGS
        if fl not in objs:
            objs[fl] = coff(compile_obj(src, fl, lang))
        secs, syms = objs[fl]
        addr = int(addr, 16)
        acc, a, b, full, same, table_bad, sm = score_function(mod, addr, secs, syms, sym)
        results.append((mod, addr, sym, full, acc, a, b, fl))
        if quiet:
            continue
        print(f"{mod:14} 0x{addr:08x} {sym}: {100 * acc:.1f}% ({same}/{max(len(a), len(b))} instructions)  flags={fl}"
              + (f"  SWITCH TABLE DIFFERS: {table_bad[:3]}" if table_bad else ""))
        if show or acc < 1:
            for tag, i1, i2, j1, j2 in sm.get_opcodes():
                if tag == "equal" and not show:
                    continue
                for k in range(max(i2 - i1, j2 - j1)):
                    x = a[i1 + k][0] if i1 + k < i2 else ""
                    y = b[j1 + k][0] if j1 + k < j2 else ""
                    print(f"   {'=' if tag == 'equal' else '!'} {x:42} | {y}")
    return results


def write_evidence(results):
    out = ROOT / "log" / "diff"
    out.mkdir(parents=True, exist_ok=True)
    for mod, addr, sym, full, acc, a, b, flags in results:
        # tracker key: VA for golf_clean.exe, RVA for DLLs (same as hooks.csv); readable Class_method name
        key = tracker_key(mod, addr)
        parts = re.match(r"\?(\w+)@(\w+)@@", sym)
        name = f"{parts.group(2)}_{parts.group(1)}" if parts else re.sub(r"[^A-Za-z0-9_]", "_", sym)
        if mod != "golf_clean.exe":
            name = f"{mod.split('.')[0]}_{name}"
        p = out / f"{key:08x}_{name}.match.csv"
        with p.open("w", newline="") as f:
            w = csv.writer(f)
            w.writerow(["instruction", "recompiled", "original", "match"])
            for i in range(max(len(a), len(b))):
                x = a[i] if i < len(a) else ("", "")
                y = b[i] if i < len(b) else ("", "")
                w.writerow([i, x[0], y[0], x[1] == y[1]])
            w.writerow(["meta", f"compiler=cl 12.00.8168 (VC6) flags={flags}", f"symbol={full}", f"module={mod}"])
            w.writerow(["VERDICT", "GREEN" if acc == 1 else "RED", len(a), f"{100 * acc:.1f}%"])


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("src")
    ap.add_argument("--flags", default="")
    ap.add_argument("--show", action="store_true")
    ap.add_argument("--no-evidence", action="store_true")
    ap.add_argument("--only", default="", help="one annotated address (hex)")
    a = ap.parse_args()
    res = compare(pathlib.Path(a.src), a.flags, a.show, int(a.only, 16) if a.only else None)
    if not a.no_evidence:
        write_evidence(res)
    return 0 if all(r[4] == 1 for r in res) else 1


if __name__ == "__main__":
    sys.exit(main())
