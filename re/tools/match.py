"""Matching decompilation check: compile annotated C++ with the ORIGINAL compiler (VC6, cl 12.00.8168, the
build in the game's Rich headers) and compare each function with the bytes in the anchored binary.

    py -3.12 re/tools/match.py re/match/terrain.cpp            # all MATCH annotations in the file
    py -3.12 re/tools/match.py re/match/terrain.cpp --flags "/O2" --show

Source annotations (one per function, on the line before its definition):
    // MATCH: <module> <0xaddress> <decorated-or-plain symbol name>
Flags, per module (or one line for all; --flags overrides):
    // FLAGS golf_clean.exe: /O2
    // FLAGS Terrain.dll: /Od /ZI /GZ

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


def compile_obj(src: pathlib.Path, flags: str) -> pathlib.Path:
    out = pathlib.Path(tempfile.mkdtemp(prefix="vc6_")) / (src.stem + ".obj")
    env = {"PATH": str(VC / "bin"), "INCLUDE": str(VC / "include"), "LIB": str(VC / "lib"),
           "SystemRoot": r"C:\Windows", "TEMP": tempfile.gettempdir(), "TMP": tempfile.gettempdir()}
    cmd = [str(VC / "bin" / "cl.exe"), "/nologo", "/c", "/Gy", *flags.split(), f"/Fo{out}", str(src)]
    r = subprocess.run(cmd, capture_output=True, text=True, env=env)
    if r.returncode != 0:
        raise SystemExit(f"VC6 compile failed:\n{r.stdout}{r.stderr}")
    return out


# ------------------------------------------------------------------ compare
def function_from_obj(secs, syms, wanted: str):
    """(bytes, relocation offsets, symbol name, same-section reloc targets) for the function whose symbol matches `wanted` (exact or substring)."""
    cands = [s for s in syms if s["sec"] > 0 and s["type"] == 0x20 and (s["name"] == wanted or wanted in s["name"])]
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


TSV = {"golf_clean.exe": "functions_ghidra.tsv", "Terrain.dll": "functions_ghidra_Terrain.dll.tsv"}


def ghidra_size(module: str, addr: int) -> int | None:
    """Function body size from Ghidra (independent of our recompile, so a shorter body cannot hide)."""
    f = ROOT / "re" / TSV.get(module, "")
    if not f.is_file():
        return None
    key = f"{addr:08x}" if module == "golf_clean.exe" else f"{addr - 0x10000000 if addr >= 0x10000000 else addr:08x}"
    for line in f.read_text().splitlines()[1:]:
        c = line.split("	")
        if c[0] == key:
            return int(c[2])
    return None


def original_bytes(module: str, addr: int, n: int) -> bytes:
    pe = pefile.PE(str(GAME / module), fast_load=True)
    rva = addr - pe.OPTIONAL_HEADER.ImageBase if addr >= pe.OPTIONAL_HEADER.ImageBase else addr
    return pe.get_data(rva, n)


def normalize(code: bytes, base: int, start: int, is_abs) -> list[tuple[str, str]]:
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
        elif ins.bytes[0] in (0xE8, 0xE9, 0xEB) or (ins.bytes[0] in range(0x70, 0x80)) or (ins.bytes[0] == 0x0F and ins.bytes[1] in range(0x80, 0x90)):
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
    targets = [t for site, t in local.items() if site < t < len(code)]
    return code[:min(targets)] if targets else code


def file_flags(text: str, module: str) -> str | None:
    m = re.search(rf"//\s*FLAGS\s+{re.escape(module)}:\s*(.+)", text) or re.search(r"//\s*FLAGS:\s*(.+)", text)
    return m.group(1).strip() if m else None


def compare(src: pathlib.Path, flags: str, show: bool):
    """Per-module flags: `// FLAGS <module>: ...` (one source can match a release exe and a debug DLL)."""
    text = src.read_text()
    anns = re.findall(r"//\s*MATCH:\s*(\S+)\s+(0x[0-9a-fA-F]+)\s+(\S+)", text)
    objs = {}
    results = []
    for mod, addr, sym in anns:
        fl = flags or file_flags(text, mod) or DEFAULT_FLAGS
        if fl not in objs:
            objs[fl] = coff(compile_obj(src, fl))
        secs, syms = objs[fl]
        addr = int(addr, 16)
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

        pe_base = 0x400000 if mod == "golf_clean.exe" else 0x10000000
        img_size = pefile.PE(str(GAME / mod), fast_load=True).OPTIONAL_HEADER.SizeOfImage

        def orig_abs(ins, i, v):
            return pe_base <= v < pe_base + img_size

        a = normalize(strip_switch_tables(mine, local), 0, 0, mine_abs)
        b = normalize(orig, addr, addr, orig_abs)
        # MSVC pads functions with int3/nop up to 16 bytes; padding is not part of either body
        while a and a[-1][0].split()[0] in ("int3", "nop"):
            a.pop()
        sm = difflib.SequenceMatcher(None, [x[1] for x in a], [x[1] for x in b], autojunk=False)
        same = sum(bl.size for bl in sm.get_matching_blocks())
        acc = same / max(len(a), len(b))
        results.append((mod, addr, sym, full, acc, a, b, fl))
        print(f"{mod:14} 0x{addr:08x} {sym}: {100 * acc:.1f}% ({same}/{max(len(a), len(b))} instructions)  flags={fl}")
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
        key = addr if mod == "golf_clean.exe" else (addr - 0x10000000 if addr >= 0x10000000 else addr)
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
    a = ap.parse_args()
    res = compare(pathlib.Path(a.src), a.flags, a.show)
    if not a.no_evidence:
        write_evidence(res)
    return 0 if all(r[4] == 1 for r in res) else 1


if __name__ == "__main__":
    sys.exit(main())
