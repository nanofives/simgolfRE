"""Unicorn replay of ONE call of a golf_clean.exe function from the file image (phase 2.5 vector corpus).

The emulated process: golf_clean.exe's sections from original\\golf_clean.exe at their VAs (the ORIGINAL code: no hook
jumps, unlike a live image), a caller-supplied data-section image, fixture blocks at caller-chosen bases, a 1 MB stack,
a fake TEB behind fs (SEH chain end -1), and every IAT slot pointing at its own unmapped trap address, so a call into an
import stops the run and names the import. Every read and write outside the stack is recorded by address; the executed
basic blocks give the callees entered (function starts from re/functions_ghidra.tsv) and whether x87 code ran.

    run_call(img, data, blocks, abi, arg_types, arg_values, ret_type, fpcw, entry) -> CallResult
"""
from __future__ import annotations

import dataclasses
import functools
import pathlib
import struct

import pefile
from capstone import CS_ARCH_X86, CS_MODE_32, Cs
from unicorn import (UC_ARCH_X86, UC_HOOK_BLOCK, UC_MEM_FETCH_UNMAPPED, UC_HOOK_MEM_FETCH_UNMAPPED, UC_HOOK_MEM_READ,
                     UC_HOOK_MEM_READ_UNMAPPED, UC_HOOK_MEM_WRITE, UC_HOOK_MEM_WRITE_UNMAPPED, UC_MODE_32, Uc, UcError)
from unicorn.x86_const import (UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_EDX, UC_X86_REG_EIP, UC_X86_REG_ESP,
                               UC_X86_REG_FP0, UC_X86_REG_FPCW, UC_X86_REG_FPSW, UC_X86_REG_FS, UC_X86_REG_GDTR,
                               UC_X86_REG_DS, UC_X86_REG_ES, UC_X86_REG_GS, UC_X86_REG_SS)

import vector_lib as VL

PAGE = 0x1000
STACK_BASE, STACK_SIZE = 0x7F000000, 0x100000
SENTINEL = 0x7EFF0000          # return address: execution stops there
TEB = 0x7EFE0000
GDT = 0x7EFD0000
TRAP_BASE = 0x7D000000         # IAT slot i -> TRAP_BASE + 4 * i (unmapped)
IMAGE_END = 0x00845000


@dataclasses.dataclass
class ExeImage:
    data: bytes                  # IMAGE_BASE .. IMAGE_END, sections at their VAs, IAT slots patched with trap addresses
    imports: dict                # trap address -> "dll!name"
    sections: list               # (name, va, vsize)
    func_starts: dict            # va -> name (re/functions_ghidra.tsv)
    func_ranges: dict            # va -> [(lo, hi)] code ranges of the function (re/functions_ghidra.tsv `ranges`)

    def section_of(self, va: int):
        for n, s, z in self.sections:
            if s <= va < s + z:
                return n, va - s
        return None


@functools.lru_cache(maxsize=1)
def load_image(path: str = str(VL.EXE)) -> ExeImage:
    pe = pefile.PE(path)
    buf = bytearray(IMAGE_END - VL.IMAGE_BASE)
    hdr = pe.get_memory_mapped_image()[:pe.OPTIONAL_HEADER.SizeOfHeaders]
    buf[:len(hdr)] = hdr
    secs = []
    for s in pe.sections:
        va = VL.IMAGE_BASE + s.VirtualAddress
        raw = s.get_data()[:max(s.Misc_VirtualSize, 0)]
        buf[va - VL.IMAGE_BASE: va - VL.IMAGE_BASE + len(raw)] = raw
        secs.append((s.Name.rstrip(b"\0").decode(), va, s.Misc_VirtualSize))
    imports = {}
    i = 0
    for d in pe.DIRECTORY_ENTRY_IMPORT:
        for imp in d.imports:
            trap = TRAP_BASE + 4 * i
            i += 1
            off = imp.address - VL.IMAGE_BASE
            buf[off:off + 4] = trap.to_bytes(4, "little")
            imports[trap] = f"{d.dll.decode()}!{imp.name.decode() if imp.name else '#%d' % imp.ordinal}"
    starts, ranges = {}, {}
    tsv = VL.ROOT / "re" / "functions_ghidra.tsv"
    for line in tsv.read_text().splitlines()[1:]:
        f = line.split("\t")
        starts[int(f[0], 16)] = f[1]
        ranges[int(f[0], 16)] = [(int(a, 16), int(b, 16)) for a, b in (r.split("-") for r in f[5].split(";") if r)]
    return ExeImage(bytes(buf), imports, secs, starts, ranges)


def jcc_sites(img: ExeImage, entry: int) -> dict:
    """Conditional jumps inside a function's own ranges: jcc address -> (target, fall-through address)."""
    out = {}
    for lo, hi in img.func_ranges.get(entry, []):
        code = img.data[lo - VL.IMAGE_BASE:hi - VL.IMAGE_BASE]
        for i in _CS.disasm(code, lo):
            m = i.mnemonic
            if (m.startswith("j") and m != "jmp") or m in ("loop", "loope", "loopne"):
                try:
                    t = int(i.op_str, 16)
                except ValueError:
                    continue
                out[i.address] = (t, i.address + i.size)
    return out


_STATIC: dict = {}


def static_info(img: ExeImage, entry: int) -> tuple[list, bool]:
    """(static callee closure, x87 anywhere in it): direct `call rel32` / `jmp rel32` to a function start, followed
    through every range of every function reached. Indirect calls (vtables, pointers) are not followed, so a virtual
    callee's x87 is not seen; the flag is the function's own and its direct callees' code."""
    if entry in _STATIC:
        return _STATIC[entry]
    seen, todo, x87 = [], [entry], False
    done = set()
    while todo:
        f = todo.pop()
        if f in done:
            continue
        done.add(f)
        for lo, hi in img.func_ranges.get(f, []):
            code = img.data[lo - VL.IMAGE_BASE:hi - VL.IMAGE_BASE]
            for i in _CS.disasm(code, lo):
                if i.mnemonic.startswith("f"):
                    x87 = True
                if i.mnemonic in ("call", "jmp") and i.op_str.startswith("0x"):
                    t = int(i.op_str, 16)
                    if t in img.func_starts and t not in done:
                        if t != entry and t not in seen:
                            seen.append(t)
                        todo.append(t)
    _STATIC[entry] = (seen, x87)
    return _STATIC[entry]


@functools.lru_cache(maxsize=200000)
def last_insn(code: bytes, addr: int) -> int:
    last = addr
    for i in _CS.disasm(code, addr):
        last = i.address
    return last


def _gdt_entry(base, limit, access, flags):
    d = limit & 0xFFFF | (base & 0xFFFFFF) << 16 | (access & 0xFF) << 40 | ((limit >> 16) & 0xF) << 48
    d |= (flags & 0xF) << 52 | ((base >> 24) & 0xFF) << 56
    return d.to_bytes(8, "little")


_CS = Cs(CS_ARCH_X86, CS_MODE_32)


@functools.lru_cache(maxsize=200000)
def _block_has_x87(code: bytes, addr: int) -> bool:
    return any(i.mnemonic.startswith("f") for i in _CS.disasm(code, addr))


@dataclasses.dataclass
class CallResult:
    eax: int
    edx: int
    st0: float | None
    pop: int                     # bytes the callee removed from the stack beyond the return address (ret N)
    reads: list                  # [(addr, size)] outside the stack, in order
    writes: set                  # byte addresses written outside the stack
    mem: dict                    # byte address -> final value, for every written byte
    misses: list                 # unmapped accesses (address)
    fetch_misses: list           # unmapped instruction fetches (code outside the image: a DLL, a fixture stub)
    imports: list                # imports called, in order
    callees: list                # static callee closure of the entry (direct calls / jumps; see static_info)
    x87: bool                    # x87 code in that closure (static)
    err: str | None
    insns: int                   # basic blocks executed (a cost measure)
    edges: set                   # (last instruction of a block inside the entry function, next block start)
    accesses: int = 0            # hooked memory accesses (reads + writes of the image and blocks): the cost measure


def run_call(img: ExeImage, data: bytes, blocks: list[tuple[int, bytes]], abi: str, arg_types: list[str], arg_values: list,
             ret_type: str, fpcw: int, entry: int, budget: int = 50_000_000, reads_cap: int = 2_000_000,
             light: bool = False) -> CallResult:
    """blocks: [(base, bytes)]; arg_values: ints (pointer / integer types, already relocated) or floats."""
    uc = Uc(UC_ARCH_X86, UC_MODE_32)
    uc.mem_map(VL.IMAGE_BASE, IMAGE_END - VL.IMAGE_BASE)
    uc.mem_write(VL.IMAGE_BASE, img.data)
    if len(data) != VL.DATA_SIZE:
        raise ValueError("data image size")
    uc.mem_write(VL.DATA_VA, data)
    pages = set()
    for base, b in blocks:
        for p in range(base & ~(PAGE - 1), (base + max(len(b), 1) + PAGE - 1) & ~(PAGE - 1), PAGE):
            if VL.IMAGE_BASE <= p < IMAGE_END:
                raise ValueError(f"block at 0x{base:08x} overlaps the image")
            pages.add(p)
    for p in sorted(pages):
        uc.mem_map(p, PAGE)
    for base, b in blocks:
        uc.mem_write(base, b)
    uc.mem_map(STACK_BASE, STACK_SIZE)
    uc.mem_map(SENTINEL, PAGE)
    uc.mem_map(TEB, PAGE)
    uc.mem_map(GDT, PAGE)
    teb = struct.pack("<IIII", 0xFFFFFFFF, STACK_BASE + STACK_SIZE, STACK_BASE, 0) + b"\0" * 8 + struct.pack("<I", TEB)
    uc.mem_write(TEB, teb)
    # a full flat GDT: with only an fs entry, `ret` faulted on the null ss / ds descriptors (2026-10-18)
    uc.mem_write(GDT + 1 * 8, _gdt_entry(0, 0xFFFFF, 0xFB, 0xC))      # code, ring 3
    uc.mem_write(GDT + 2 * 8, _gdt_entry(0, 0xFFFFF, 0xF3, 0xC))      # data, ring 3
    uc.mem_write(GDT + 3 * 8, _gdt_entry(TEB, 0xFFF, 0xF3, 0x4))      # fs -> TEB
    uc.mem_write(GDT + 4 * 8, _gdt_entry(0, 0xFFFFF, 0x93, 0xC))      # ss, ring 0
    uc.reg_write(UC_X86_REG_GDTR, (0, GDT, PAGE - 1, 0))
    for reg, sel in ((UC_X86_REG_DS, 0x13), (UC_X86_REG_ES, 0x13), (UC_X86_REG_GS, 0x13), (UC_X86_REG_FS, 0x1B),
                     (UC_X86_REG_SS, 0x20)):
        uc.reg_write(reg, sel)
    uc.reg_write(UC_X86_REG_FPCW, fpcw)

    # stack frame: return address, then the arguments the ABI puts on the stack
    stack_args = list(zip(arg_types, arg_values))
    ecx = edx = 0
    if abi == "thiscall" and stack_args:
        ecx = stack_args.pop(0)[1]
    elif abi == "fastcall":
        if stack_args:
            ecx = stack_args.pop(0)[1]
        if stack_args:
            edx = stack_args.pop(0)[1]
    frame = struct.pack("<I", SENTINEL)
    for t, v in stack_args:
        if t == "double":
            frame += struct.pack("<d", v)
        elif t == "float":
            frame += struct.pack("<f", v)
        elif t in ("int64", "uint64"):
            frame += struct.pack("<Q", int(v) & (2 ** 64 - 1))
        else:
            frame += struct.pack("<I", int(v) & 0xFFFFFFFF)
    sp = STACK_BASE + STACK_SIZE - 0x1000
    uc.mem_write(sp, frame)
    uc.reg_write(UC_X86_REG_ESP, sp)
    uc.reg_write(UC_X86_REG_ECX, ecx & 0xFFFFFFFF)
    uc.reg_write(UC_X86_REG_EDX, edx & 0xFFFFFFFF)

    reads, writes, misses, imports, callees, seen, fetch_misses = set(), set(), [], [], [], set(), []
    state = {"blocks": 0, "x87": False, "last": None, "acc": 0}
    edges = set()
    own = img.func_ranges.get(entry) or [(entry, entry + 1)]
    in_own = lambda a: any(lo <= a < hi for lo, hi in own)
    lo_stack, hi_stack = STACK_BASE, STACK_BASE + STACK_SIZE

    # the memory hooks are bound to the image and the blocks (hook_add begin / end filters on the accessed address),
    # so the stack's reads and writes, most of a heavy function's accesses, never enter Python (editTerrainHeight's
    # first build ran 1,062 s, then Unicorn reported NOMEM)
    def on_read(u, access, addr, size, value, _):
        state["acc"] += 1
        if size == 4:
            reads.update((addr, addr + 1, addr + 2, addr + 3))
        else:
            reads.update(range(addr, addr + size))

    def on_write(u, access, addr, size, value, _):
        state["acc"] += 1
        if size == 4:
            writes.update((addr, addr + 1, addr + 2, addr + 3))
        else:
            writes.update(range(addr, addr + size))

    def on_unmapped(u, access, addr, size, value, _):
        if addr in img.imports:
            imports.append(img.imports[addr])
        elif access == UC_MEM_FETCH_UNMAPPED:
            fetch_misses.append(addr)
        misses.append(addr)
        return False

    # the block hook is bound to the function's own ranges (branch edges only): an unbound one cost 8.3 M Python calls
    # for one updateGolfers vector. The successor of the last own block is caught by its next own block or, when the
    # function returns / calls out, not needed (a jcc's both targets lie in its own ranges, except tail jumps).
    def on_block(u, addr, size, _):
        if state["last"] is not None:
            edges.add((state["last"], addr))
        state["blocks"] += 1
        state["last"] = last_insn(img.data[addr - VL.IMAGE_BASE:addr - VL.IMAGE_BASE + size], addr)

    # light: only the unmapped-access hook (the capture's page probe); no reads, writes, edges or block budget, an
    # instruction count limit instead, so heavy functions run at native emulation speed
    if not light:
        ranges = [(VL.IMAGE_BASE, IMAGE_END - 1)]
        for p in sorted(pages):
            if ranges and ranges[-1][1] + 1 == p and ranges[-1][0] != VL.IMAGE_BASE:
                ranges[-1] = (ranges[-1][0], p + PAGE - 1)
            else:
                ranges.append((p, p + PAGE - 1))
        for lo, hi in ranges:
            uc.hook_add(UC_HOOK_MEM_READ, on_read, None, lo, hi)
            uc.hook_add(UC_HOOK_MEM_WRITE, on_write, None, lo, hi)
        for lo, hi in own:
            uc.hook_add(UC_HOOK_BLOCK, on_block, None, lo, hi - 1)
    uc.hook_add(UC_HOOK_MEM_READ_UNMAPPED | UC_HOOK_MEM_WRITE_UNMAPPED | UC_HOOK_MEM_FETCH_UNMAPPED, on_unmapped)
    err = None
    try:
        uc.emu_start(entry, SENTINEL, count=budget * 20)     # instruction limit (infinite loops end as an error)
    except UcError as e:
        err = str(e)
    eip = uc.reg_read(UC_X86_REG_EIP)
    if eip != SENTINEL and err is None:
        err = f"stopped at 0x{eip:08x} (instruction budget {budget * 20})"
    if imports:
        err = f"import {imports[0]}"
    elif misses and err:
        err = f"{err} at 0x{misses[0]:08x}"
    st0 = None
    if ret_type in ("double", "float") and err is None:
        top = (uc.reg_read(UC_X86_REG_FPSW) >> 11) & 7
        mant, exp = uc.reg_read(UC_X86_REG_FP0 + top)
        st0 = VL.f80_to_float(mant, exp)
        if ret_type == "float":
            st0 = struct.unpack("<f", struct.pack("<f", st0))[0] if st0 == st0 and abs(st0) < 3.4e38 else st0
    reads = [(a, n) for a, n in VL.runs(reads)]
    scallees, sx87 = static_info(img, entry) if not light else ([], False)
    mem = {}
    for run_start, n in VL.runs(writes):
        try:
            b = uc.mem_read(run_start, n)
        except UcError:
            continue
        for k in range(n):
            mem[run_start + k] = b[k]
    return CallResult(uc.reg_read(UC_X86_REG_EAX), uc.reg_read(UC_X86_REG_EDX), st0,
                      uc.reg_read(UC_X86_REG_ESP) - sp - 4 if err is None else -1,
                      reads, writes, mem, misses, fetch_misses, imports, scallees, sx87, err, state["blocks"], edges,
                      state["acc"])
