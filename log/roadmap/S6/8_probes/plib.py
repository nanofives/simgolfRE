"""S6.6 probe library: disassemble ranges of golf_clean.exe and run the ORIGINAL in Unicorn over the file image.

    import plib as P
    P.dis(0x4028f0, 0x402a00)                      # print capstone listing
    base = P.start_image('sandbox_basic', '00')     # a real GameState image (bytes, DATA_SIZE)
    r = P.emu(entry, args, data, abi='default', max_insns=20_000_000)
    r.status in ('completed', 'hang', 'fault', 'import')

No game run, no Frida. The image is the file image (no hooks), IAT slots trap, so a call into an import stops the run.
"""
from __future__ import annotations

import dataclasses
import pathlib
import struct
import sys

ROOT = pathlib.Path(__file__).resolve().parents[4]
sys.path.insert(0, str(ROOT / "re" / "tools"))
import vector_emu as E          # noqa: E402
import vector_lib as VL         # noqa: E402
from capstone import CS_ARCH_X86, CS_MODE_32, Cs   # noqa: E402
from unicorn import (UC_HOOK_MEM_FETCH_UNMAPPED, UC_HOOK_MEM_READ_UNMAPPED, UC_HOOK_MEM_WRITE_UNMAPPED, Uc, UcError)  # noqa: E402
from unicorn.x86_const import (UC_X86_REG_EAX, UC_X86_REG_EBP, UC_X86_REG_EBX, UC_X86_REG_ECX, UC_X86_REG_EDI,
                               UC_X86_REG_EDX, UC_X86_REG_EIP, UC_X86_REG_ESI, UC_X86_REG_ESP, UC_X86_REG_FPCW,
                               UC_X86_REG_FS, UC_X86_REG_GDTR, UC_X86_REG_DS, UC_X86_REG_ES, UC_X86_REG_GS,
                               UC_X86_REG_SS, UC_X86_REG_EFLAGS)  # noqa: E402

IMG = E.load_image()
CS = Cs(CS_ARCH_X86, CS_MODE_32)
CS.skipdata = True
D0, DN = VL.DATA_VA, VL.DATA_SIZE


def dis(lo: int, hi: int, out=None):
    code = IMG.data[lo - VL.IMAGE_BASE:hi - VL.IMAGE_BASE]
    for i in CS.disasm(code, lo):
        line = "%08x  %-28s %s %s" % (i.address, i.bytes.hex(), i.mnemonic, i.op_str)
        print(line, file=out)


def start_image(scenario: str = "sandbox_basic", k: str = "00") -> bytes:
    p = ROOT / "port" / "corpus" / "segments" / scenario / k / "start.data.bin"
    b = p.read_bytes()
    assert len(b) == DN, len(b)
    return b


def zero_image() -> bytes:
    return bytes(DN)


class Img:
    """mutable view of a GameState image by virtual address"""

    def __init__(self, b: bytes):
        self.b = bytearray(b)

    def r(self, va, n=4):
        return int.from_bytes(self.b[va - D0:va - D0 + n], "little")

    def w(self, va, v, n=4):
        self.b[va - D0:va - D0 + n] = (v & ((1 << (8 * n)) - 1)).to_bytes(n, "little")

    def wb(self, va, bs: bytes):
        self.b[va - D0:va - D0 + len(bs)] = bs

    def rb(self, va, n):
        return bytes(self.b[va - D0:va - D0 + n])

    def done(self) -> bytes:
        return bytes(self.b)


@dataclasses.dataclass
class Res:
    status: str                   # completed | hang | fault | import
    eip: int
    eax: int
    err: str | None
    insns: int
    regs: dict
    writes: dict                  # va -> (old, new) for every changed byte in the data image
    misses: list
    imports: list
    pop: int = 0
    stack_args: list = dataclasses.field(default_factory=list)


def emu(entry: int, args: list, data: bytes, abi: str = "default", max_insns: int = 20_000_000, blocks=(),
        init_regs: dict | None = None, hooks=None, ret_addr=None, stack_fill: int | None = None) -> Res:
    img = IMG
    from unicorn import UC_ARCH_X86, UC_MODE_32
    uc = Uc(UC_ARCH_X86, UC_MODE_32)
    uc.mem_map(VL.IMAGE_BASE, E.IMAGE_END - VL.IMAGE_BASE)
    uc.mem_write(VL.IMAGE_BASE, img.data)
    uc.mem_write(D0, data)
    for base, b in blocks:
        for p in range(base & ~0xFFF, (base + max(len(b), 1) + 0xFFF) & ~0xFFF, 0x1000):
            try:
                uc.mem_map(p, 0x1000)
            except UcError:
                pass
        uc.mem_write(base, b)
    uc.mem_map(E.STACK_BASE, E.STACK_SIZE)
    uc.mem_map(E.SENTINEL, 0x1000)
    uc.mem_map(E.TEB, 0x1000)
    uc.mem_map(E.GDT, 0x1000)
    teb = struct.pack("<IIII", 0xFFFFFFFF, E.STACK_BASE + E.STACK_SIZE, E.STACK_BASE, 0) + b"\0" * 8 + struct.pack("<I", E.TEB)
    uc.mem_write(E.TEB, teb)
    uc.mem_write(E.GDT + 8, E._gdt_entry(0, 0xFFFFF, 0xFB, 0xC))
    uc.mem_write(E.GDT + 16, E._gdt_entry(0, 0xFFFFF, 0xF3, 0xC))
    uc.mem_write(E.GDT + 24, E._gdt_entry(E.TEB, 0xFFF, 0xF3, 0x4))
    uc.mem_write(E.GDT + 32, E._gdt_entry(0, 0xFFFFF, 0x93, 0xC))
    uc.reg_write(UC_X86_REG_GDTR, (0, E.GDT, 0xFFF, 0))
    for reg, sel in ((UC_X86_REG_DS, 0x13), (UC_X86_REG_ES, 0x13), (UC_X86_REG_GS, 0x13), (UC_X86_REG_FS, 0x1B), (UC_X86_REG_SS, 0x20)):
        uc.reg_write(reg, sel)
    uc.reg_write(UC_X86_REG_FPCW, 0x027F)
    st = list(args)
    ecx = edx = 0
    if abi == "thiscall" and st:
        ecx = st.pop(0)
    elif abi == "fastcall":
        if st:
            ecx = st.pop(0)
        if st:
            edx = st.pop(0)
    ra = ret_addr if ret_addr is not None else E.SENTINEL
    frame = struct.pack("<I", ra) + b"".join(struct.pack("<I", int(v) & 0xFFFFFFFF) for v in st)
    sp = E.STACK_BASE + E.STACK_SIZE - 0x1000
    if stack_fill is not None:
        uc.mem_write(sp - 0x4000, bytes([stack_fill]) * 0x4000)
    uc.mem_write(sp, frame)
    uc.reg_write(UC_X86_REG_ESP, sp)
    uc.reg_write(UC_X86_REG_ECX, ecx)
    uc.reg_write(UC_X86_REG_EDX, edx)
    for k, v in (init_regs or {}).items():
        uc.reg_write(k, v)
    misses, imports = [], []

    def on_unmapped(u, access, addr, size, value, _):
        if addr in img.imports:
            imports.append(img.imports[addr])
        misses.append((access, addr, size))
        return False

    uc.hook_add(UC_HOOK_MEM_READ_UNMAPPED | UC_HOOK_MEM_WRITE_UNMAPPED | UC_HOOK_MEM_FETCH_UNMAPPED, on_unmapped)
    if hooks:
        for h in hooks:
            h(uc)
    err = None
    try:
        uc.emu_start(entry, E.SENTINEL, count=max_insns)
    except UcError as e:
        err = str(e)
    eip = uc.reg_read(UC_X86_REG_EIP)
    regs = {n: uc.reg_read(r) for n, r in (("eax", UC_X86_REG_EAX), ("ebx", UC_X86_REG_EBX), ("ecx", UC_X86_REG_ECX),
                                          ("edx", UC_X86_REG_EDX), ("esi", UC_X86_REG_ESI), ("edi", UC_X86_REG_EDI),
                                          ("ebp", UC_X86_REG_EBP), ("esp", UC_X86_REG_ESP), ("eflags", UC_X86_REG_EFLAGS))}
    if imports:
        status = "import"
    elif eip == E.SENTINEL and err is None:
        status = "completed"
    elif err is None:
        status = "hang"
    else:
        status = "fault"
    after = bytes(uc.mem_read(D0, DN))
    writes = {}
    if after != data:
        # byte diff, vectorised by chunks
        for off in range(0, DN, 4096):
            a, b = data[off:off + 4096], after[off:off + 4096]
            if a != b:
                for i in range(len(a)):
                    if a[i] != b[i]:
                        writes[D0 + off + i] = (a[i], b[i])
    pop = (regs["esp"] - sp - 4) if status == "completed" else -1
    return Res(status, eip, regs["eax"], err, max_insns, regs, writes, misses, imports, pop)


def show(r: Res, label: str = "", maxw: int = 12):
    print(f"[{label}] {r.status} eip=0x{r.eip:08x} eax=0x{r.eax:08x} err={r.err} imports={r.imports[:3]} "
          f"misses={[(a, hex(ad), s) for a, ad, s in r.misses[:3]]} writes={len(r.writes)}")
    items = sorted(r.writes.items())[:maxw]
    print("    first writes:", ", ".join(f"{a:#x}:{o:02x}->{n:02x}" for a, (o, n) in items))


_ADDR = None


def syms(lo: int, hi: int):
    """addresses.tsv rows overlapping [lo, hi): (va, size, symbol, c_type, count)"""
    global _ADDR
    if _ADDR is None:
        import csv
        _ADDR = []
        for r in csv.DictReader(open(ROOT / "re" / "model" / "addresses.tsv", encoding="utf-8"), delimiter="\t"):
            try:
                _ADDR.append((int(r["va"], 16), int(r["size"], 16), r["symbol"], r["c_type"], r["count"]))
            except ValueError:
                pass
        _ADDR.sort()
    return [(a, s, n, t, c) for a, s, n, t, c in _ADDR if a < hi and a + s > lo]


def pr_syms(lo, hi):
    for a, s, n, t, c in syms(lo, hi):
        print(f"  {a:#010x} +{s:#x} {n} {t} x{c}")


# ------------------------------------------------------------------------------------------------ hooks for probes
from unicorn import UC_HOOK_CODE, UC_HOOK_BLOCK  # noqa: E402

RANGE = 0x45c1e0          # Random::range(n), thiscall ecx = generator, `ret 4`


def script_random(values, log=None, default=0, fn=RANGE):
    """Hook: Random::range returns the next scripted value instead of drawing (values: list or callable(n, k)->int).
    Records (n, returned) in `log`. The generator word at 0x822d9c is not touched."""
    state = {"k": 0}

    def install(uc):
        def on_code(u, addr, size, _):
            esp = u.reg_read(UC_X86_REG_ESP)
            ret = int.from_bytes(u.mem_read(esp, 4), "little")
            n = int.from_bytes(u.mem_read(esp + 4, 4), "little")
            k = state["k"]
            state["k"] += 1
            if callable(values):
                v = values(n, k)
            else:
                v = values[k] if k < len(values) else default
            if log is not None:
                log.append((n, v))
            u.reg_write(UC_X86_REG_EAX, v & 0xFFFFFFFF)
            u.reg_write(UC_X86_REG_ESP, esp + 8)
            u.reg_write(UC_X86_REG_EIP, ret)
        uc.hook_add(UC_HOOK_CODE, on_code, None, fn, fn)
    return install


def visit(addrs, log):
    """Hook: record each address in `addrs` the first time it is executed (log: list)"""
    addrs = sorted(addrs)

    def install(uc):
        seen = set()

        def on_code(u, addr, size, _):
            if addr not in seen:
                seen.add(addr)
                log.append(addr)
        for a in addrs:
            uc.hook_add(UC_HOOK_CODE, on_code, None, a, a)
    return install


def stop_after(n_hits, addr, log=None):
    """Hook: stop emulation when `addr` has executed n_hits times (counts a loop iteration)"""
    def install(uc):
        st = {"n": 0}

        def on_code(u, a, size, _):
            st["n"] += 1
            if st["n"] >= n_hits:
                u.emu_stop()
        uc.hook_add(UC_HOOK_CODE, on_code, None, addr, addr)
    return install


_INSNS = None


def _all_insns():
    global _INSNS
    if _INSNS is None:
        import bisect
        _INSNS = []
        for f, rs in IMG.func_ranges.items():
            for lo, hi in rs:
                code = IMG.data[lo - VL.IMAGE_BASE:hi - VL.IMAGE_BASE]
                for i in CS.disasm(code, lo):
                    _INSNS.append((i.address, f, i.mnemonic, i.op_str))
        _INSNS.sort()
    return _INSNS


def refs(lo: int, hi: int, base_ok=True):
    """instructions whose text names an absolute address in [lo, hi): (addr, function va, function name, text)
    A displacement is read as an operand number `0x...` in the op string (registers + disp forms included)."""
    import re
    out = []
    pat = re.compile(r"0x[0-9a-f]+")
    for a, f, m, o in _all_insns():
        for t in pat.findall(o):
            v = int(t, 16)
            if lo <= v < hi:
                out.append((a, f, IMG.func_starts.get(f, "?"), f"{m} {o}"))
                break
    return out


def pr_refs(lo, hi, maxn=80):
    rs = refs(lo, hi)
    print(f"{len(rs)} refs in [{lo:#x},{hi:#x})")
    for a, f, n, t in rs[:maxn]:
        print(f"  {a:#010x} {n}({f:#x}) {t}")
