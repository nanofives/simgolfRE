"""The ORIGINAL sound.dll in Unicorn, fed by the fixture VM (step S4.1; re/analysis/data/sound_replay.md).

A path-1 A/B key of sound.dll (re/frida/registry.d, module="sound.dll") is replayed without the game:

  * the fixture VM (re/tools/fixvm/fixvm.js, Node) runs the key's fixture JS over a mock Frida API, in a 32-bit address space
    shared with this module page by page (FixVM below);
  * Machine runs native code in Unicorn: the user's original\\sound.dll relocated to a chosen base (its 5,769 HIGHLOW
    relocations applied, of 5,816 entries with 47 type-0 pads; ImageBase 0x10000000 is taken by jgld.dll in the game, so the live run had another base too), the IAT
    bound by the VM to fake system DLLs, a 1 MB stack and a fake TEB. A call into a NativeCallback, a fake DLL export or an
    Interceptor redirect stops Unicorn and is served by the VM; a NativeFunction the fixture JS calls is run here;
  * run_key() repeats diff_hook.js for one key: fixture, then per vector both arms from the same snapshot of the state
    regions (the second arm runs the original again, standing in for the reimplementation the live run called), the regions'
    hex hashed with the same FNV-1a, and the return value formatted as Frida printed it.

The S4.2 port runner can take Machine's place: anything with the same call / read / write / take_dirty / apply_pages /
set_hooks surface (see sound_replay.md, "Second executor") is driven by the same FixVM and KeyRun.

    from sound_emu import run_key
    res = run_key("Voice_mixStereo16", layout="A")
"""
from __future__ import annotations

import base64
import dataclasses
import functools
import importlib.util
import json
import os
import pathlib
import re
import shutil
import struct
import subprocess
import sys

import pefile
from unicorn import (UC_ARCH_X86, UC_HOOK_CODE, UC_HOOK_MEM_WRITE, UC_MODE_32, UC_PROT_ALL, Uc, UcError)
from unicorn.x86_const import (UC_X86_REG_DS, UC_X86_REG_EAX, UC_X86_REG_EBP, UC_X86_REG_EBX, UC_X86_REG_ECX,
                               UC_X86_REG_EDI, UC_X86_REG_EDX, UC_X86_REG_EFLAGS, UC_X86_REG_EIP, UC_X86_REG_ES,
                               UC_X86_REG_ESI, UC_X86_REG_ESP, UC_X86_REG_FPCW, UC_X86_REG_FS, UC_X86_REG_GDTR,
                               UC_X86_REG_GS, UC_X86_REG_SS)

ROOT = pathlib.Path(__file__).resolve().parents[2]
SOUND_DLL = ROOT / "original" / "sound.dll"
FIXVM_JS = ROOT / "re" / "tools" / "fixvm" / "fixvm.js"
JS_DIR = ROOT / "re" / "frida" / "js"
REG_DIR = ROOT / "re" / "frida" / "registry.d"
DIFF_DIR = ROOT / "log" / "diff"

PAGE = 0x1000
STACK_BASE, STACK_SIZE = 0x7F000000, 0x100000
STACK_INIT = STACK_BASE + STACK_SIZE - 0x2000
TEB = 0x7EFE0000
GDT = 0x7EFD0000
STUB = 0x6F000000                 # code page: return / float stubs
SCR = 0x6F001000                  # data page the stubs use
TRAP_LO, TRAP_HI = 0x7A000000, 0x7C000000   # fake DLL exports, NativeCallbacks, trampolines (fixvm.js / syslibs.js)
RET_TRAP = 0x7BFFF000             # return address of every native call this side makes
TID, MAIN_TID = 0x1A2C, 0x0D44    # the A/B thread's id in the VM (fs:[0x24]) and the game's first thread
FPCW_DEFAULT = 0x027F             # the rpc thread's control word (CLAUDE.md, x87 method of round 11)

# The TLS index the live process's TlsAlloc gave sound.dll's CRT (DllMain): 27 is the only value in 0..1099 for which
# _errno_a's import log hashes to its CSV (sound_census, 2026-10-24); _errno_b, __doserrno_a / _b then match too. The value
# is a fact of that boot's DLL load order, not of the fixture [U-1404].
LIVE_TLS_INDEX = 27

# Layouts A, B and C differ in every run address: the module base, the fixture heap, the emulated heap, and the gaps between
# fixture blocks. A value equal in all three layouts does not depend on where things are (sound_replay.md).
LAYOUTS = {
    "A": dict(base=0x04C00000, heap=0x30000000, emuheap=0x60000000, pad=0, tls=LIVE_TLS_INDEX),
    "B": dict(base=0x05E30000, heap=0x48000000, emuheap=0x64000000, pad=7, tls=LIVE_TLS_INDEX),
    "C": dict(base=0x07A50000, heap=0x52000000, emuheap=0x68000000, pad=13, tls=LIVE_TLS_INDEX),
}


def node_exe() -> str:
    return os.environ.get("SG_NODE") or shutil.which("node") or r"C:\Program Files\nodejs\node.exe"


# ---- the module image -------------------------------------------------------------------------------------------------
@dataclasses.dataclass
class Image:
    name: str
    base: int
    data: bytes                    # SizeOfImage bytes at `base`, relocations applied for `reloc_base`
    exports: dict                  # name -> rva
    sections: list                 # (rva, vsize, prot, name)
    entry: int                     # rva of the entry point
    reloc_base: int


@functools.lru_cache(maxsize=4)
def _raw(path: str):
    """(image mapped at its ImageBase, HIGHLOW relocation rvas, sections, exports, entry rva, ImageBase): read once."""
    pe = pefile.PE(path)
    o = pe.OPTIONAL_HEADER
    data = bytearray(o.SizeOfImage)
    data[:o.SizeOfHeaders] = pe.__data__[:o.SizeOfHeaders]
    secs = []
    for s in pe.sections:
        raw = s.get_data()[:min(s.SizeOfRawData, max(s.Misc_VirtualSize, s.SizeOfRawData))]
        raw = raw[:max(s.Misc_VirtualSize, 0)] if s.Misc_VirtualSize else raw
        data[s.VirtualAddress:s.VirtualAddress + len(raw)] = raw
        ch = s.Characteristics
        prot = ("r" if ch & 0x40000000 else "-") + ("w" if ch & 0x80000000 else "-") + ("x" if ch & 0x20000000 else "-")
        secs.append((s.VirtualAddress, s.Misc_VirtualSize, prot, s.Name.rstrip(b"\0").decode()))
    relocs = []
    for blk in getattr(pe, "DIRECTORY_ENTRY_BASERELOC", []):
        for e in blk.entries:
            if e.type == 3:                  # IMAGE_REL_BASED_HIGHLOW
                relocs.append(e.rva)
            elif e.type != 0:
                raise ValueError(f"relocation type {e.type}")
    exps = {}
    if hasattr(pe, "DIRECTORY_ENTRY_EXPORT"):
        for e in pe.DIRECTORY_ENTRY_EXPORT.symbols:
            if e.name:
                exps[e.name.decode()] = e.address
    r = (bytes(data), tuple(relocs), tuple(secs), exps, o.AddressOfEntryPoint, o.ImageBase)
    pe.close()
    return r


@functools.lru_cache(maxsize=8)
def load_image(base: int, path: pathlib.Path = SOUND_DLL, reloc_base: int | None = None) -> Image:
    """The file mapped as the loader maps it, relocated for `reloc_base` (default `base`; a different value is the
    wrong-relocation control). IAT slots keep the file's values: the VM binds them."""
    data0, relocs, secs, exps, entry, ibase = _raw(str(path))
    reloc_base = base if reloc_base is None else reloc_base
    data = bytearray(data0)
    delta = (reloc_base - ibase) & 0xFFFFFFFF
    if delta:
        for o in relocs:
            struct.pack_into("<I", data, o, (struct.unpack_from("<I", data, o)[0] + delta) & 0xFFFFFFFF)
    return Image(path.name, base, bytes(data), dict(exps), list(secs), entry, reloc_base)


def reloc_count(path: pathlib.Path = SOUND_DLL) -> int:
    return len(_raw(str(path))[1])


# ---- errors -----------------------------------------------------------------------------------------------------------
class EmuError(Exception):
    """A vector that could not run to its end: kind is 'fault' | 'import' | 'fixture' | 'timeout' | 'link'."""

    def __init__(self, kind: str, msg: str, api: str | None = None):
        super().__init__(msg)
        self.kind, self.api = kind, api


# ---- the link to the fixture VM ---------------------------------------------------------------------------------------
class FixVM:
    """One Node process. request() sends a message with this side's dirty pages, then serves the VM's nested requests
    (native calls) until the reply arrives, applying the pages and hooks every VM message carries."""

    def __init__(self):
        self.p = subprocess.Popen([node_exe(), str(FIXVM_JS)], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                  stderr=subprocess.PIPE, cwd=str(ROOT))
        self.next_id = 1
        self.loaded = set()
        self.machine = None

    def close(self):
        try:
            self._send({"op": "quit", "id": 0})
        except OSError:
            pass
        try:
            self.p.wait(5)
        except subprocess.TimeoutExpired:
            self.p.kill()

    def __enter__(self):
        return self

    def __exit__(self, *a):
        self.close()

    def _send(self, msg):
        b = json.dumps(msg, separators=(",", ":")).encode()
        self.p.stdin.write(struct.pack("<I", len(b)) + b)
        self.p.stdin.flush()

    def _recv(self):
        h = self.p.stdout.read(4)
        if len(h) < 4:
            err = self.p.stderr.read().decode(errors="replace")
            raise EmuError("link", f"fixture VM exited: {err[-2000:]}")
        n = struct.unpack("<I", h)[0]
        b = self.p.stdout.read(n)
        return json.loads(b)

    def _absorb(self, m):
        if self.machine is not None:
            if m.get("pages"):
                self.machine.apply_pages(m["pages"])
            if "hooks" in m:
                self.machine.set_hooks(m["hooks"])

    def request(self, msg: dict) -> dict:
        msg["id"] = self.next_id
        self.next_id += 1
        if self.machine is not None:
            msg["pages"] = self.machine.take_dirty()
        self._send(msg)
        while True:
            m = self._recv()
            self._absorb(m)
            if m.get("op") == "reply":
                if m["id"] != msg["id"]:
                    raise EmuError("link", f"reply {m['id']} while waiting for {msg['id']}")
                return m
            self._serve(m)

    def _serve(self, m):
        r = {"op": "reply", "id": m["id"]}
        try:
            if m["op"] == "call":
                r.update(self.machine.native_call(m["addr"], m["abi"], m["args"], m["ret"]))
            elif m["op"] == "hostcall":
                r.update(_host_call(self.machine, m["api"], m["args"]))
            else:
                raise EmuError("link", f"unknown request {m['op']}")
        except EmuError as e:
            r["error"] = f"{e.kind}: {e}"
            if e.api:
                r["api"] = e.api
        r["pages"] = self.machine.take_dirty()
        self._send(r)

    def load(self, img: Image):
        k = (img.name.lower(), img.base, img.reloc_base)
        if k in self.loaded:
            return
        data = [s for s in img.sections if s[3] == ".data"]
        self.request({"op": "load", "name": img.name, "base": img.base, "image": base64.b64encode(img.data).decode(),
                      "exports": img.exports, "sections": [[r, v, p] for r, v, p, _ in img.sections],
                      "data": [img.base + data[0][0], data[0][1]] if data else None})
        self.loaded = {x for x in self.loaded if x[:2] != k[:2]} | {k}


# ---- the emulated machine ---------------------------------------------------------------------------------------------
def _gdt_entry(base, limit, access, flags):
    d = limit & 0xFFFF | (base & 0xFFFFFF) << 16 | (access & 0xFF) << 40 | ((limit >> 16) & 0xF) << 48
    d |= (flags & 0xF) << 52 | ((base >> 24) & 0xFF) << 56
    return d.to_bytes(8, "little")


def _stubs() -> bytes:
    """STUB page: +0x00 fstp qword [SCR]; jmp RET_TRAP   +0x10 fstp dword [SCR]; jmp RET_TRAP
    +0x100 + 0x10 k: fld qword [SCR+8]; ret 4k      +0x400 + 0x10 k: fld dword [SCR+16]; ret 4k   (k = 0..47)"""
    b = bytearray(b"\xCC" * PAGE)

    def jmp(at, to):
        return b"\xE9" + struct.pack("<i", to - (STUB + at + 5))
    b[0:6] = b"\xDD\x1D" + struct.pack("<I", SCR)
    b[6:11] = jmp(6, RET_TRAP)
    b[0x10:0x16] = b"\xD9\x1D" + struct.pack("<I", SCR)
    b[0x16:0x1B] = jmp(0x16, RET_TRAP)
    for k in range(48):
        ret = b"\xC3" if k == 0 else b"\xC2" + struct.pack("<H", 4 * k)
        o = 0x100 + 0x10 * k
        b[o:o + 6] = b"\xDD\x05" + struct.pack("<I", SCR + 8)
        b[o + 6:o + 6 + len(ret)] = ret
        o = 0x400 + 0x10 * k
        b[o:o + 6] = b"\xD9\x05" + struct.pack("<I", SCR + 16)
        b[o + 6:o + 6 + len(ret)] = ret
    return bytes(b)


_WIDE = ("double", "int64", "uint64", "longlong", "ulonglong")


class Machine:
    """Unicorn over the VM's address space. Pages arrive from the VM (apply_pages); every page an emulated instruction
    or this class writes is dirty until take_dirty() hands it back."""

    def __init__(self, vm: FixVM, budget_s: float = 30.0):
        self.vm = vm
        self.uc = uc = Uc(UC_ARCH_X86, UC_MODE_32)
        self.mapped: set[int] = set()
        self.dirty: set[int] = set()
        self.redirects: dict[int, int] = {}
        self.tramps: dict[int, int] = {}
        self._rhooks: dict[int, int] = {}
        self.bypass: int | None = None
        self.depth = 0
        self.budget_s = budget_s
        self.insn_budget = 300_000_000    # per uninterrupted stretch of emulation (between two traps)
        self.calls = 0
        self.imports: list[str] = []
        for base, size in ((STACK_BASE, STACK_SIZE), (TEB, PAGE), (GDT, PAGE), (STUB, PAGE), (SCR, PAGE),
                           (RET_TRAP, PAGE)):
            self._map(base, size)
        uc.mem_write(STUB, _stubs())
        uc.mem_write(RET_TRAP, b"\xCC" * PAGE)
        teb = struct.pack("<IIII", 0xFFFFFFFF, STACK_BASE + STACK_SIZE, STACK_BASE, 0) + b"\0" * 8 + struct.pack("<I", TEB)
        teb = teb.ljust(0x24, b"\0") + struct.pack("<I", TID)
        uc.mem_write(TEB, teb)
        uc.mem_write(GDT + 1 * 8, _gdt_entry(0, 0xFFFFF, 0xFB, 0xC))
        uc.mem_write(GDT + 2 * 8, _gdt_entry(0, 0xFFFFF, 0xF3, 0xC))
        uc.mem_write(GDT + 3 * 8, _gdt_entry(TEB, 0xFFF, 0xF3, 0x4))
        uc.mem_write(GDT + 4 * 8, _gdt_entry(0, 0xFFFFF, 0x93, 0xC))
        uc.reg_write(UC_X86_REG_GDTR, (0, GDT, PAGE - 1, 0))
        for reg, sel in ((UC_X86_REG_DS, 0x13), (UC_X86_REG_ES, 0x13), (UC_X86_REG_GS, 0x13), (UC_X86_REG_FS, 0x1B),
                         (UC_X86_REG_SS, 0x20)):
            uc.reg_write(reg, sel)
        uc.reg_write(UC_X86_REG_FPCW, FPCW_DEFAULT)
        uc.reg_write(UC_X86_REG_ESP, STACK_INIT)
        self.dirty |= {TEB >> 12}
        uc.hook_add(UC_HOOK_MEM_WRITE, self._on_write)
        uc.hook_add(UC_HOOK_CODE, self._on_trap, None, TRAP_LO, TRAP_HI - 1)
        self._stop = None

    # the regions the VM must know as mapped (it holds zero pages for them)
    def regions(self):
        return [[STACK_BASE, STACK_SIZE], [TEB, PAGE], [GDT, PAGE], [STUB, PAGE], [SCR, PAGE], [RET_TRAP, PAGE]]

    def _map(self, base, size):
        self.uc.mem_map(base, size, UC_PROT_ALL)
        self.mapped.update(range(base >> 12, (base + size) >> 12))

    def _on_write(self, uc, access, addr, size, value, _):
        self.dirty.add(addr >> 12)
        self.dirty.add((addr + size - 1) >> 12)

    def _on_trap(self, uc, addr, size, _):
        self._stop = ("trap", addr)
        uc.emu_stop()

    def _on_redirect(self, uc, addr, size, _):
        if self.bypass == addr:
            self.bypass = None
            return
        self._stop = ("redirect", addr)
        uc.emu_stop()

    # ---- memory surface (shared with any second executor) ----
    def read(self, addr: int, n: int) -> bytes:
        return bytes(self.uc.mem_read(addr, n))

    def write(self, addr: int, data: bytes):
        self.uc.mem_write(addr, bytes(data))
        for p in range(addr >> 12, ((addr + max(len(data), 1) - 1) >> 12) + 1):
            self.dirty.add(p)

    def take_dirty(self):
        out = []
        for p in sorted(self.dirty):
            if p in self.mapped:
                out.append([p, base64.b64encode(bytes(self.uc.mem_read(p << 12, PAGE))).decode()])
        self.dirty.clear()
        return out

    def apply_pages(self, pages):
        flush = []
        for p, b64 in pages:
            if p not in self.mapped:
                self._map(p << 12, PAGE)
            else:
                flush.append(p)
            self.uc.mem_write(p << 12, base64.b64decode(b64))
        for p in flush:
            self.uc.ctl_remove_cache(p << 12, (p << 12) + PAGE)

    def set_hooks(self, hooks):
        new = {int(t): int(r) for t, r in hooks.get("redirects", [])}
        self.tramps = {int(a): int(t) for a, t in hooks.get("tramps", [])}
        for t in list(self._rhooks):
            if t not in new:
                self.uc.hook_del(self._rhooks.pop(t))
                self.uc.ctl_remove_cache(t, t + 1)
        for t in new:
            if t not in self._rhooks:
                self._rhooks[t] = self.uc.hook_add(UC_HOOK_CODE, self._on_redirect, None, t, t)
                self.uc.ctl_remove_cache(t & ~0xFFF, (t & ~0xFFF) + PAGE)
        self.redirects = new

    # ---- calls ----
    def native_call(self, addr: int, abi: str, args: list, ret: str) -> dict:
        """One call as Frida's NativeFunction makes it (libffi on x86): arguments by `abi`, return value in eax / edx, or
        st(0) stored with fstp at the thread's current control word."""
        uc = self.uc
        saved = {r: uc.reg_read(r) for r in (UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX, UC_X86_REG_EDX,
                                             UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP, UC_X86_REG_ESP,
                                             UC_X86_REG_EIP, UC_X86_REG_EFLAGS)}
        sp0 = saved[UC_X86_REG_ESP] if self.depth else STACK_INIT
        regs = []
        if abi == "thiscall":
            regs = ["ecx"]
        elif abi == "fastcall":
            regs = ["ecx", "edx"]
        ecx = edx = 0
        frame = b""
        for a in args:
            t, v = a["t"], a["v"]
            if t not in _WIDE and t != "float" and regs:
                r = regs.pop(0)
                if r == "ecx":
                    ecx = int(v) & 0xFFFFFFFF
                else:
                    edx = int(v) & 0xFFFFFFFF
                continue
            if t == "double":
                frame += struct.pack("<d", float(v))
            elif t == "float":
                frame += struct.pack("<f", float(v))
            elif t in _WIDE:
                frame += struct.pack("<Q", int(v) & (2 ** 64 - 1))
            else:
                frame += struct.pack("<I", int(v) & 0xFFFFFFFF)
        retaddr = STUB if ret == "double" else (STUB + 0x10 if ret == "float" else RET_TRAP)
        sp = ((sp0 - 0x400) & ~0xF) - len(frame) - 4
        self.write(sp, struct.pack("<I", retaddr) + frame)
        uc.reg_write(UC_X86_REG_ESP, sp)
        uc.reg_write(UC_X86_REG_ECX, ecx)
        uc.reg_write(UC_X86_REG_EDX, edx)
        self.depth += 1
        self.calls += 1
        try:
            self._run(addr)
        finally:
            self.depth -= 1
        out = {"eax": uc.reg_read(UC_X86_REG_EAX), "edx": uc.reg_read(UC_X86_REG_EDX)}
        if ret == "double":
            out["f"] = struct.unpack("<d", self.read(SCR, 8))[0]
            out["fbits"] = self.read(SCR, 8).hex()
        elif ret == "float":
            out["f"] = struct.unpack("<f", self.read(SCR, 4))[0]
            out["fbits"] = self.read(SCR, 4).hex()
        for r in (UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP, UC_X86_REG_ESP, UC_X86_REG_EIP,
                  UC_X86_REG_EFLAGS):
            uc.reg_write(r, saved[r])
        out["f"] = _json_float(out.get("f"))
        if out["f"] is None:
            out.pop("f")
        return out

    def _run(self, eip: int):
        import time
        uc = self.uc
        t_end = time.monotonic() + self.budget_s
        while True:
            self._stop = None
            left = t_end - time.monotonic()
            if left <= 0:
                raise EmuError("timeout", f"budget of {self.budget_s} s spent (eip 0x{eip:08x})")
            try:
                # an instruction count, not Unicorn's timeout: a timeout starts a timer thread that costs ~15 ms per start
                uc.emu_start(eip, 0xFFFFFFFF, count=self.insn_budget)
            except UcError as e:
                pc = uc.reg_read(UC_X86_REG_EIP)
                raise EmuError("fault", f"{e} at eip 0x{pc:08x}") from None
            if self._stop is None:
                pc = uc.reg_read(UC_X86_REG_EIP)
                raise EmuError("timeout", f"emulation stopped at 0x{pc:08x} without a trap (budget {self.insn_budget} "
                                          "instructions)")
            kind, addr = self._stop
            if kind == "redirect":
                eip = self.redirects[addr]
                continue
            if addr == RET_TRAP:
                return
            if addr in self.tramps:
                eip = self.tramps[addr]
                self.bypass = eip
                continue
            esp = uc.reg_read(UC_X86_REG_ESP)
            r = self.vm.request({"op": "trap", "addr": addr, "esp": esp, "ecx": uc.reg_read(UC_X86_REG_ECX),
                                 "edx": uc.reg_read(UC_X86_REG_EDX)})
            if "error" in r:
                if r.get("api"):
                    self.imports.append(r["api"])
                    raise EmuError("import", r["error"], r["api"])
                raise EmuError("fixture", f"trap 0x{addr:08x}: {r['error']}")
            if "eax" in r:
                uc.reg_write(UC_X86_REG_EAX, int(r["eax"]) & 0xFFFFFFFF)
            if "edx" in r:
                uc.reg_write(UC_X86_REG_EDX, int(r["edx"]) & 0xFFFFFFFF)
            pop = int(r.get("pop", 0))
            ret = struct.unpack("<I", self.read(esp, 4))[0]
            if "f" in r:
                # st(0) result of a NativeCallback: the stub loads it and returns with `ret pop` itself
                f = _from_json_float(r["f"])
                self.write(SCR + 8, struct.pack("<d", f))
                self.write(SCR + 16, _pack_f32(f))
                eip = STUB + 0x100 + 0x10 * (pop // 4)
                continue
            uc.reg_write(UC_X86_REG_ESP, esp + 4 + pop)
            eip = ret


def _pack_f32(f: float) -> bytes:
    """float -> 4 bytes rounded to nearest (an out-of-range value becomes +-Infinity, as a C cast does)."""
    try:
        return struct.pack("<f", f)
    except OverflowError:
        return struct.pack("<f", float("inf") if f > 0 else float("-inf"))


# ---- host calls: pure Win32 locale / code-page functions answered by this machine's Windows ---------------------------
def _k32():
    import ctypes
    return ctypes.WinDLL("kernel32", use_last_error=True)


def _alen(m, p):
    n = 0
    while m.read(p + n, 1) != b"\0":
        n += 1
    return n


def _wlen(m, p):
    n = 0
    while m.read(p + 2 * n, 2) != b"\0\0":
        n += 1
    return n


_HOST_SCALAR = {"GetACP": 0, "GetOEMCP": 0, "IsValidCodePage": 1, "IsProcessorFeaturePresent": 1}


def _host_call(m, api: str, a: list) -> dict:
    """Run one pure kernel32 function for the emulated code: inputs read from the emulated memory, the real function
    called through ctypes, outputs written back (their pages go to the VM as dirty pages)."""
    import ctypes
    k = _k32()
    S = lambda v: ctypes.c_int32(v & 0xFFFFFFFF).value     # noqa: E731
    f = getattr(k, api)
    ctypes.set_last_error(0)
    if api in _HOST_SCALAR:
        r = f(*a[:_HOST_SCALAR[api]])
    elif api == "GetCPInfo":
        buf = ctypes.create_string_buffer(20)
        r = f(a[0], buf)
        if r:
            m.write(a[1], buf.raw)
    elif api in ("GetStringTypeW", "GetStringTypeA"):
        wide = api.endswith("W")
        info, src, cch, out = (a[0], a[1], S(a[2]), a[3]) if wide else (a[1], a[2], S(a[3]), a[4])
        n = cch if cch >= 0 else (_wlen(m, src) if wide else _alen(m, src)) + 1
        sb = ctypes.create_string_buffer(m.read(src, 2 * n if wide else n), 2 * n if wide else n)
        ob = ctypes.create_string_buffer(2 * max(n, 1))
        r = f(info, sb, cch, ob) if wide else f(a[0], info, sb, cch, ob)
        if r:
            m.write(out, ob.raw[:2 * n])
    elif api in ("LCMapStringW", "LCMapStringA", "CompareStringW", "CompareStringA"):
        wide = api.endswith("W")
        u = 2 if wide else 1
        if api.startswith("LCMap"):
            lcid, flags, src, cch, dst, cchd = a[0], a[1], a[2], S(a[3]), a[4], S(a[5])
            n = cch if cch >= 0 else (_wlen(m, src) if wide else _alen(m, src)) + 1
            sb = ctypes.create_string_buffer(m.read(src, u * n), u * n)
            bytes_out = (flags & 0x400) != 0                      # LCMAP_SORTKEY counts bytes
            ou = 1 if bytes_out else u
            ob = ctypes.create_string_buffer(ou * max(cchd, 1))
            r = f(lcid, flags, sb, cch, ob if cchd else None, cchd)
            if r and cchd:
                m.write(dst, ob.raw[:ou * min(r, cchd)])
        else:
            lcid, flags, s1, c1, s2, c2 = a[0], a[1], a[2], S(a[3]), a[4], S(a[5])
            n1 = c1 if c1 >= 0 else (_wlen(m, s1) if wide else _alen(m, s1)) + 1
            n2 = c2 if c2 >= 0 else (_wlen(m, s2) if wide else _alen(m, s2)) + 1
            b1 = ctypes.create_string_buffer(m.read(s1, u * n1), u * n1)
            b2 = ctypes.create_string_buffer(m.read(s2, u * n2), u * n2)
            r = f(lcid, flags, b1, c1, b2, c2)
    elif api in ("GetLocaleInfoA", "GetLocaleInfoW"):
        u = 2 if api.endswith("W") else 1
        cch = S(a[3])
        ob = ctypes.create_string_buffer(u * max(cch, 1))
        r = f(a[0], a[1], ob if cch else None, cch)
        if r and cch:
            m.write(a[2], ob.raw[:u * min(r, cch)])
    elif api == "MultiByteToWideChar":
        cp, fl, src, cb, dst, cch = a[0], a[1], a[2], S(a[3]), a[4], S(a[5])
        n = cb if cb >= 0 else _alen(m, src) + 1
        sb = ctypes.create_string_buffer(m.read(src, n), max(n, 1))
        ob = ctypes.create_string_buffer(2 * max(cch, 1))
        r = f(cp, fl, sb, cb, ob if cch else None, cch)
        if r and cch:
            m.write(dst, ob.raw[:2 * min(r, cch)])
    elif api == "WideCharToMultiByte":
        cp, fl, src, cch, dst, cb, dch, used = a[0], a[1], a[2], S(a[3]), a[4], S(a[5]), a[6], a[7]
        n = cch if cch >= 0 else _wlen(m, src) + 1
        sb = ctypes.create_string_buffer(m.read(src, 2 * n), max(2 * n, 2))
        ob = ctypes.create_string_buffer(max(cb, 1))
        db = ctypes.create_string_buffer(m.read(dch, 1) + b"\0") if dch else None
        ub = ctypes.c_int32(0)
        r = f(cp, fl, sb, cch, ob if cb else None, cb, db, ctypes.byref(ub) if used else None)
        if r and cb:
            m.write(dst, ob.raw[:min(r, cb)])
        if used:
            m.write(used, struct.pack("<i", ub.value))
    else:
        raise EmuError("import", f"no host call for {api}", f"kernel32.dll!{api}")
    return {"eax": r & 0xFFFFFFFF, "lastError": ctypes.get_last_error()}


def _json_float(f):
    if f is None:
        return None
    if f != f:
        return "NaN"
    if f in (float("inf"), float("-inf")):
        return "Infinity" if f > 0 else "-Infinity"
    return f


def _from_json_float(f):
    if isinstance(f, str):
        return float({"NaN": "nan", "Infinity": "inf", "-Infinity": "-inf"}[f])
    return float(f)


# ---- formatting as Frida / JS printed it ------------------------------------------------------------------------------
def js_number(x: float) -> str:
    """Number.prototype.toString() of a double (ECMA-262 Number::toString, radix 10)."""
    if x != x:
        return "NaN"
    if x == 0:
        return "0"
    if x in (float("inf"), float("-inf")):
        return "Infinity" if x > 0 else "-Infinity"
    sign = "-" if x < 0 else ""
    r = repr(abs(x))
    m = re.fullmatch(r"(\d+)(?:\.(\d+))?(?:e([+-]\d+))?", r)
    ip, fp, ex = m.group(1), m.group(2) or "", int(m.group(3) or 0)
    digits = (ip + fp).lstrip("0")
    lead = len(ip + fp) - len((ip + fp).lstrip("0"))
    n = len(ip) + ex - lead               # value = 0.digits x 10^n
    digits = digits.rstrip("0") or "0"
    k = len(digits)
    if k <= n <= 21:
        s = digits + "0" * (n - k)
    elif 0 < n <= 21:
        s = digits[:n] + "." + digits[n:]
    elif -6 < n <= 0:
        s = "0." + "0" * (-n) + digits
    else:
        e = n - 1
        s = digits[0] + ("." + digits[1:] if k > 1 else "") + "e" + ("+" if e >= 0 else "-") + str(abs(e))
    return sign + s


def _js_layout(sign: str, digits: str, n: int) -> str:
    """ECMA-262 Number::toString layout of value = 0.digits x 10^n."""
    k = len(digits)
    if k <= n <= 21:
        s = digits + "0" * (n - k)
    elif 0 < n <= 21:
        s = digits[:n] + "." + digits[n:]
    elif -6 < n <= 0:
        s = "0." + "0" * (-n) + digits
    else:
        e = n - 1
        s = digits[0] + ("." + digits[1:] if k > 1 else "") + "e" + ("+" if e >= 0 else "-") + str(abs(e))
    return sign + s


def js_number_trunc17(x: float) -> str:
    """How the Frida runtime of the 2026-10-14 runs printed a double (measured on the c3fh CSVs, sound_replay.md): the
    exact decimal expansion, cut (not rounded) after 17 significant digits, or where it ends if it ends sooner; then the
    ECMA layout. 0.8 prints 0.80000000000000004, 1.1 prints 1.1000000000000000, 0.5 prints 0.5."""
    from decimal import Decimal
    if x != x:
        return "NaN"
    if x == 0:
        return "0"
    if x in (float("inf"), float("-inf")):
        return "Infinity" if x > 0 else "-Infinity"
    d = Decimal(abs(x))
    sign, digs, exp = d.as_tuple()
    s = "".join(map(str, digs)).lstrip("0")
    n = len(digs) + exp - (len(digs) - len("".join(map(str, digs)).lstrip("0")))   # value = 0.s x 10^n
    s = s[:17]
    return _js_layout("-" if x < 0 else "", s, n)


# The printing of a double changed with the Frida upgrade between the 2026-10-07 runs (shortest form, c3ar / c3au) and the
# 2026-10-14 runs (17 truncated digits, c3fh); no sound.dll CSV between them returns a double [U-1401].
TRUNC17_FROM = "2026-10-10"


def csv_double_style(spec: dict, key: str) -> str:
    p = csv_path(spec, key)
    if p.exists():
        for line in p.read_text().splitlines():
            if line.startswith("meta,"):
                return "trunc17" if line.rsplit(",", 1)[1] >= TRUNC17_FROM else "shortest"
    return "shortest"


def format_double(f: float, style: str) -> str:
    if f in (float("inf"), float("-inf")):          # Frida reads an x87 +-Infinity return as NaN (CLAUDE.md, round 12)
        f = float("nan")
    return js_number_trunc17(f) if style == "trunc17" else js_number(f)


def format_ret(ret: str, r: dict, fx: dict) -> str:
    """diff_hook.js norm(): what the CSV's `original` column holds for one call."""
    eax, edx = r["eax"] & 0xFFFFFFFF, r["edx"] & 0xFFFFFFFF
    if ret == "void":
        return "void"
    if ret == "pointer":
        if eax == 0:
            return "NULL"
        if "obj" in fx and "p" in fx["obj"]:
            return "obj+0x" + format((eax - fx["obj"]["p"]) & 0xFFFFFFFF, "x")
        return "0x" + format(eax, "x")
    if ret in ("double", "float"):
        f = _from_json_float(r["f"])
        if f in (float("inf"), float("-inf")):      # Frida reads an x87 +-Infinity return as NaN (CLAUDE.md, round 12)
            f = float("nan")
        return js_number(f)
    if ret in ("int", "int32", "long"):
        return str(eax - (1 << 32) if eax & 0x80000000 else eax)
    if ret in ("uint8", "uchar", "bool"):
        return str(eax & 0xFF)
    if ret in ("int8", "char"):
        v = eax & 0xFF
        return str(v - 256 if v & 0x80 else v)
    if ret in ("uint16", "ushort"):
        return str(eax & 0xFFFF)
    if ret in ("int16", "short"):
        v = eax & 0xFFFF
        return str(v - 65536 if v & 0x8000 else v)
    if ret in ("int64", "longlong"):
        v = (edx << 32) | eax
        return str(v - (1 << 64) if v >> 63 else v)
    if ret in ("uint64", "ulonglong"):
        return str((edx << 32) | eax)
    return str(eax)


def fnv(s: str) -> str:
    h = 0x811C9DC5
    for c in s.encode("latin1"):
        h ^= c
        h = (h * 0x01000193) & 0xFFFFFFFF
    return f"{h:08x}"


# ---- the registry -----------------------------------------------------------------------------------------------------
@functools.lru_cache(maxsize=1)
def registry() -> tuple[dict, dict]:
    """(HOOKS, batch of each key) through re/frida/hooks_registry.py's own loader."""
    sys.path.insert(0, str(ROOT / "re" / "frida"))
    import hooks_registry
    batch = {}
    for f in sorted(REG_DIR.glob("*.py"), key=lambda p: (p.stem not in hooks_registry.enabled_batches(), p.stem)):
        spec = importlib.util.spec_from_file_location(f"fixvm_reg_{f.stem}", f)
        mod = importlib.util.module_from_spec(spec)
        try:
            spec.loader.exec_module(mod)
        except Exception:
            continue
        for k in mod.HOOKS:
            batch.setdefault(k, f.stem)
    return hooks_registry.HOOKS, batch


def sound_keys() -> list[str]:
    hooks, _ = registry()
    return sorted(k for k, v in hooks.items() if (v.get("module") or "").lower() == "sound.dll")


@functools.lru_cache(maxsize=1)
def script_list() -> tuple[str, ...]:
    """diff_hook.py's script, in its order (re/frida/diff_hook.py `parts`): helpers, base fixtures, enabled fragments.
    freeze.js is replaced by the VM's own __freeze (marker '@freeze')."""
    sys.path.insert(0, str(ROOT / "re" / "frida"))
    import hooks_registry
    en = hooks_registry.enabled_batches()
    frags = [p for p in sorted((JS_DIR / "fixtures.d").glob("*.js")) if p.stem in en]
    names = ["keepalive.js", "heapstandin.js", "importstandin.js", "filestandin.js", "mmiostandin.js", "@freeze",
             "recdisplay.js", "uitree.js", "modal.js", "framemask.js", "diff_fixtures.js"]
    return tuple((n if n.startswith("@") else str(JS_DIR / n)) for n in names) + tuple(str(p) for p in frags)


def csv_path(spec: dict, key: str) -> pathlib.Path:
    name = key.replace("::", "_").replace("<", "(").replace(">", ")").replace("*", "P")
    m = (spec.get("module") or "golf_clean.exe")
    tag = "" if m.lower() == "golf_clean.exe" else m.rsplit(".", 1)[0] + "_"
    return DIFF_DIR / f"{spec['addr']:08x}_{tag}{name}.path1.csv"


# ---- one key ----------------------------------------------------------------------------------------------------------
@dataclasses.dataclass
class Row:
    vector: list
    orig: str | None = None
    state_hex: str | None = None   # regions after the first arm, hex joined by '|'
    state_orig: str | None = None  # fnv of that
    state_changed: bool | None = None
    error: str | None = None
    error_kind: str | None = None
    api: str | None = None
    re: str | None = None          # second arm's return text (the original again, or `second_arm`, e.g. the port)
    state_re_hex: str | None = None
    match: bool | None = None      # diff_hook's `same(row)`: returns equal and (stateful) regions equal


@dataclasses.dataclass
class KeyResult:
    key: str
    layout: str
    fixture_error: str | None
    rows: list
    used: list
    script_errors: list
    imports: list
    fx: dict
    regions: list


def resolve_arg(v, t, fx):
    if isinstance(v, str) and v.startswith("$"):
        e = fx.get(v[1:])
        if e is None:
            raise EmuError("fixture", f"fixture returned no {v}")
        v = e.get("p", e.get("n"))
        if v is None:
            raise EmuError("fixture", f"{v} is not a pointer or a number")
    if t in ("double", "float"):
        return {"t": t, "v": float(v)}
    if t in _WIDE:
        return {"t": t, "v": str(int(v))}
    return {"t": t, "v": int(v) & 0xFFFFFFFF}


class KeyRun:
    """diff_hook.js run() for one key over Machine + FixVM. arms=2 runs the original a second time where the live run
    called the reimplementation (both arms' writes outside the state regions persist into the next vector, as live)."""

    def __init__(self, vm: FixVM, key: str, layout: str = "A", arms: int = 2, init: bool = True,
                 reloc_base: int | None = None, budget_s: float = 30.0, fixture_patch=None, machine_factory=None,
                 double_style: str | None = None, second_arm=None):
        hooks, batch = registry()
        self.spec = dict(hooks[key])
        self.key, self.layout, self.arms, self.init = key, layout, arms, init
        self.lay = LAYOUTS[layout] if isinstance(layout, str) else layout
        self.vm = vm
        self.batch = batch.get(key)
        self.img = load_image(self.lay["base"], reloc_base=reloc_base)
        self.budget_s = budget_s
        self.fixture_patch = fixture_patch
        self.machine_factory = machine_factory or (lambda vm: Machine(vm, budget_s))
        self.double_style = double_style or csv_double_style(self.spec, key)
        # second_arm(run, args) -> {"eax", "edx"[, "f"]}: called where diff_hook.js called the reimplementation, with the
        # vector's raw arguments [{t, v}] on the same memory (run.m), between the same snapshot / restore points. None = the
        # original again. sound_replay.md, "Second executor".
        self.second_arm = second_arm

    def setup(self):
        vm = self.vm
        vm.machine = None
        vm.load(self.img)
        m = self.machine_factory(vm)
        vm.machine = m
        self.m = m
        b = vm.request({"op": "begin", "name": self.img.name, "base": self.img.base, "tid": TID, "mainTid": MAIN_TID,
                        "teb": TEB, "regions": m.regions(),
                        "layout": {"heap": self.lay["heap"], "emuheap": self.lay["emuheap"], "pad": self.lay["pad"],
                                   "tls": self.lay.get("tls", 0)},
                        "scripts": list(script_list())})
        if "error" in b:
            raise EmuError("link", "begin: " + b["error"])
        self.script_errors = b.get("errors", [])
        if self.init:
            # DllMain(hinst, DLL_PROCESS_ATTACH, 0): the CRT start-up the live module ran before any key
            m.native_call(self.img.base + self.img.entry, "stdcall",
                          [{"t": "pointer", "v": self.img.base}, {"t": "uint32", "v": 1}, {"t": "pointer", "v": 0}], "int")
            m.reg_reset()
        return m

    def run(self) -> KeyResult:
        spec = self.spec
        self.setup()
        fx, used, ferr = {}, [], None
        if spec.get("fixture"):
            r = self.vm.request({"op": "fixture", "name": spec["fixture"]})
            if "error" in r:
                ferr = r["error"] + (f" [{r['api']}]" if r.get("api") else "")
            fx, used = r.get("fx", {}), r.get("used", [])
        rows, regions = [], []
        if ferr is None and self.fixture_patch:
            self.fixture_patch(self, fx)
        if ferr is None:
            r = self.vm.request({"op": "regions", "state": [list(x) for x in spec.get("state", [])]})
            if "error" in r:
                ferr = r["error"]
            else:
                regions = [tuple(x) for x in r["regions"]]
        if ferr is None:
            r = self.vm.request({"op": "mkarm", "addr": self.img.base + spec["addr"], "ret": spec["ret"],
                                 "args": spec["args"], "abi": spec["abi"]})
            if "error" in r:
                ferr = r["error"]
        if ferr is None:
            for vec in spec["vectors"]:
                rows.append(self._vector(list(vec), regions))
        st = self.vm.request({"op": "state"})
        return KeyResult(self.key, self.layout if isinstance(self.layout, str) else "custom", ferr, rows, used,
                         self.script_errors, st.get("imports", []), fx, regions)

    def _snap(self, regions):
        return [self.m.read(a, n) for a, n in regions]

    def _restore(self, regions, snap):
        for (a, n), b in zip(regions, snap):
            self.m.write(a, b)

    def _req(self, msg):
        r = self.vm.request(msg)
        if "error" in r:
            e = r["error"]
            kind = "import" if r.get("api") else next((k for k in ("fault", "timeout", "link") if f"{k}:" in e), "fixture")
            raise EmuError(kind, e, r.get("api"))
        return r

    def _arm(self) -> str:
        r = self._req({"op": "arm"})
        if self.spec["ret"] in ("double", "float"):
            return format_double(_from_json_float(r["f"]), self.double_style)
        return r["norm"]

    def _vector(self, vec, regions) -> Row:
        row = Row(vec)
        before = None
        try:
            self._req({"op": "resolve", "vec": vec})
            before = self._snap(regions) if regions else None
            row.orig = self._arm()
            if regions:
                after = self._snap(regions)
                row.state_hex = "|".join(b.hex() for b in after)
                row.state_orig = fnv(row.state_hex)
                row.state_changed = after != before
                self._restore(regions, before)
            if self.arms > 1:
                if self.second_arm is None:
                    row.re = self._arm()
                else:
                    ca = self._req({"op": "curargs"})
                    r2 = self.second_arm(self, ca["args"])
                    fx = {"obj": {"p": ca["obj"]}} if ca.get("obj") is not None else {}
                    if self.spec["ret"] in ("double", "float"):
                        row.re = format_double(_from_json_float(r2["f"]), self.double_style)
                    else:
                        row.re = format_ret(self.spec["ret"], r2, fx)
                if regions:
                    after2 = self._snap(regions)
                    row.state_re_hex = "|".join(b.hex() for b in after2)
                    self._restore(regions, before)
                row.match = row.orig == row.re and (not regions or row.state_re_hex == row.state_hex)
        except EmuError as e:
            row.error, row.error_kind, row.api = str(e), e.kind, e.api
            if regions and before is not None:
                try:
                    self._restore(regions, before)
                except UcError:
                    pass
        return row


def _reg_reset(self):
    """Registers as a fresh rpc call finds them: esp at the stack's start, the others 0, FPU state unchanged."""
    uc = self.uc
    for r in (UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX, UC_X86_REG_EDX, UC_X86_REG_ESI, UC_X86_REG_EDI,
              UC_X86_REG_EBP):
        uc.reg_write(r, 0)
    uc.reg_write(UC_X86_REG_ESP, STACK_INIT)


Machine.reg_reset = _reg_reset


def run_key(key: str, layout: str = "A", vm: FixVM | None = None, **kw) -> KeyResult:
    own = vm is None
    vm = vm or FixVM()
    try:
        return KeyRun(vm, key, layout, **kw).run()
    finally:
        if own:
            vm.close()


# ---- the live evidence ------------------------------------------------------------------------------------------------
@dataclasses.dataclass
class CsvKey:
    rows: list                     # dicts: vector, original, state_original, state_changed
    regions: str | None
    distinct: int | None
    verdict: str | None
    error: str | None


def read_csv(spec: dict, key: str) -> CsvKey | None:
    import csv
    p = csv_path(spec, key)
    if not p.exists():
        return None
    rows, regions, distinct, verdict, error = [], None, None, None, None
    with p.open(newline="") as f:
        rd = csv.reader(f)
        head = next(rd)
        for r in rd:
            if not r:
                continue
            tag = r[0]
            if tag == "state_regions":
                regions = r[1]
            elif tag in ("install_witness", "meta"):
                pass
            elif tag == "distinct_results":
                distinct = int(r[1])
            elif tag == "VERDICT":
                verdict = r[1]
            elif tag == "error":
                error = " ".join(r[1:3])
            else:
                d = dict(zip(head, r))
                rows.append({"vector": d.get("vector"), "original": d.get("original"),
                             "state_original": d.get("state_original"),
                             "state_changed": None if d.get("state_changed") in (None, "") else d["state_changed"] == "True"})
    return CsvKey(rows, regions, distinct, verdict, error)


def vec_text(vec) -> str:
    return " ".join(map(str, vec))
