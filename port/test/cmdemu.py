"""Emulator oracle for the phase 3.7b command effects (development only, port/test/test_commands.py).

Runs an ORIGINAL golf_clean.exe function from the user's file image under Unicorn (re/tools/vector_emu.py's process
layout: sections at their VAs, data image supplied by the caller, IAT slots pointing at trap addresses) with

  * DROPPED callees stubbed: every drawing / display / window-tree call the port drops is replaced by a stub that returns
    a fixed eax and writes nothing (`Emu.stub`), so the original and the port must agree on every remaining write;
  * the MODAL PUMP modelled exactly as the port models it (portsim/sim/commands.h, "MODAL BLOCKS"): frameTick483cd0, the
    point where jgld's pumps dispatch window messages, pops the next batch of scripted raw commands and runs the ORIGINAL
    root-window handlers on them (a trampoline of `push a1; push a0; call handler` per command), with GetAsyncKeyState
    and timeGetTime answered from the command (async_ctrl / async_shift / t). A batch is every command up to and
    including the first press (handlers 1, 4, 10, 11) or key (7, 8): the dispatch granularity of the port (U-0941).
    Without input left the pump raises NeedInput, as the port does.
  * imports: timeGetTime outside a handler returns a counter (the modal waits the port reaches all pass limit 0, so the
    value is never compared), PeekMessageA returns 0 (pumpMessages removes WM_MOUSEMOVE only), every other import stops the
    run with its name.

`run` returns the final data image, eax, an error string (fault, unstubbed import, budget, NeedInput) and the log of the
entered stubs. Nothing here is used by the default suite: tests that call it are marked SG_EMU_ORACLE=1, the default suite
checks pinned digests from the port.
"""
from __future__ import annotations

import dataclasses
import pathlib
import struct
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "re" / "tools"))
import vector_emu as VE  # noqa: E402
import vector_lib as VL  # noqa: E402

# the root-window handlers, by trace HANDLER id (re/model/trace_schema.md)
ROOT_HANDLERS = {1: 0x00465580, 2: 0x00465610, 3: 0x00465650, 4: 0x00465680, 5: 0x00465720, 6: 0x00465740,
                 7: 0x00465750, 8: 0x004657B0, 9: 0x004657D0, 10: 0x004657E0, 11: 0x00465840}
STOP_HANDLERS = (1, 4, 7, 8, 10, 11)     # a batch ends after a press or a key
FRAME_TICK = 0x00483CD0                  # frameTick483cd0: the dispatch point
# pumpMessages' callers whose pump is a DRAIN (commands.h, MODAL BLOCKS): the return address of their pumpMessages call,
# read at [esp + 0x20] when frameTick483cd0 is entered (pumpMessages: sub esp, 0x1c; its pushes are popped by then)
DRAIN_CALLERS = {0x0045C164: "pumpLight", 0x0045BFCF: "waitTicks"}
TRAMP = 0x7EF00000                       # trampoline page for the handler calls


@dataclasses.dataclass
class Raw:
    """one RootHandler command (portsim::Command with kind RootHandler)"""
    handler: int
    a0: int = 0
    a1: int = 0
    async_ctrl: int = 0
    async_shift: int = 0
    t: int = 0

    def args(self):
        return [self.handler, self.a0 & 0xFFFFFFFF, self.a1 & 0xFFFFFFFF, self.async_ctrl, self.async_shift, self.t]


def click(x, y, shift=0, ctrl=0, t=0):
    """a left click as the window procedure delivers it: onPoint (down), onPress1 (up: the click), onUnpoint"""
    return [Raw(2, x, y, t=t), Raw(1, x, y, ctrl, shift, t), Raw(3, 0, 0, t=t)]


def key(code, char=0, t=0):
    """a key: onChar (WM_KEYDOWN, code), onKey (WM_CHAR, char, code), onChar0 (WM_KEYUP)"""
    return [Raw(8, code, 0, t=t), Raw(7, char, code, t=t), Raw(9, 0, 0, t=t)]


class NeedInput(Exception):
    pass


@dataclasses.dataclass
class Result:
    eax: int | None
    image: bytes
    err: str
    log: list
    consumed: int


class Emu:
    def __init__(self):
        self.img = VE.load_image()
        self.stubs = {}           # address -> (nargs, pops, eax or callable(args, ecx) -> eax, name)

    def ret_pop(self, addr) -> int:
        """the bytes the function at addr pops (its `ret N`; 0 for cdecl); every `ret` of the function must agree"""
        from capstone import CS_ARCH_X86, CS_MODE_32, Cs
        md = Cs(CS_ARCH_X86, CS_MODE_32)
        pops = set()
        for lo, hi in self.img.func_ranges.get(addr, []):
            for i in md.disasm(self.img.data[lo - VL.IMAGE_BASE:hi - VL.IMAGE_BASE], lo):
                if i.mnemonic == "ret":
                    pops.add(int(i.op_str, 16) if i.op_str else 0)
        if len(pops) != 1:
            raise ValueError(f"{addr:#x}: ret pops {sorted(pops)}")
        return pops.pop()

    def drop(self, addr, eax=0, name=""):
        """stub a dropped callee with its own calling convention (from its `ret N`)"""
        n = self.ret_pop(addr)
        return self.stub(addr, n // 4, eax, n > 0, name)

    def stub(self, addr, nargs, eax=0, pops=False, name=""):
        """replace the function at addr: returns eax (or eax(args, ecx)), pops its arguments when stdcall/thiscall"""
        self.stubs[addr] = (nargs, pops, eax, name or hex(addr))
        return self

    def run(self, entry, args, image: bytes, inputs=(), ecx=0, budget=40_000_000, blocks=(), watch=(), cover=None):
        from unicorn import UC_ARCH_X86, UC_HOOK_CODE, UC_MODE_32, Uc, UcError
        from unicorn import x86_const as X
        img = self.img
        uc = Uc(UC_ARCH_X86, UC_MODE_32)
        uc.mem_map(VL.IMAGE_BASE, VE.IMAGE_END - VL.IMAGE_BASE)
        uc.mem_write(VL.IMAGE_BASE, img.data)
        uc.mem_write(VL.DATA_VA, bytes(image))
        for base, b in blocks:
            uc.mem_map(base & ~0xFFF, (len(b) + (base & 0xFFF) + 0xFFF) & ~0xFFF)
            uc.mem_write(base, bytes(b))
        traps = sorted(img.imports)
        tlo, thi = traps[0] & ~0xFFF, (traps[-1] + 0x1000) & ~0xFFF
        uc.mem_map(tlo, thi - tlo)
        uc.mem_write(tlo, b"\xCC" * (thi - tlo))
        uc.mem_map(VE.STACK_BASE, VE.STACK_SIZE)
        uc.mem_map(VE.SENTINEL, 0x1000)
        uc.mem_map(VE.TEB, 0x1000)
        uc.mem_map(VE.GDT, 0x1000)
        uc.mem_map(TRAMP, 0x10000)
        uc.mem_write(VE.TEB, struct.pack("<IIII", 0xFFFFFFFF, VE.STACK_BASE + VE.STACK_SIZE, VE.STACK_BASE, 0) + b"\0" * 8
                     + struct.pack("<I", VE.TEB))
        uc.mem_write(VE.GDT + 8, VE._gdt_entry(0, 0xFFFFF, 0xFB, 0xC))
        uc.mem_write(VE.GDT + 16, VE._gdt_entry(0, 0xFFFFF, 0xF3, 0xC))
        uc.mem_write(VE.GDT + 24, VE._gdt_entry(VE.TEB, 0xFFF, 0xF3, 0x4))
        uc.mem_write(VE.GDT + 32, VE._gdt_entry(0, 0xFFFFF, 0x93, 0xC))
        uc.reg_write(X.UC_X86_REG_GDTR, (0, VE.GDT, 0xFFF, 0))
        for reg, sel in ((X.UC_X86_REG_DS, 0x13), (X.UC_X86_REG_ES, 0x13), (X.UC_X86_REG_GS, 0x13),
                         (X.UC_X86_REG_FS, 0x1B), (X.UC_X86_REG_SS, 0x20)):
            uc.reg_write(reg, sel)
        uc.reg_write(X.UC_X86_REG_FPCW, 0x027F)
        sp = VE.STACK_BASE + VE.STACK_SIZE - 0x1000
        uc.mem_write(sp, struct.pack("<I", VE.SENTINEL) + b"".join(struct.pack("<I", a & 0xFFFFFFFF) for a in args))
        uc.reg_write(X.UC_X86_REG_ESP, sp)
        uc.reg_write(X.UC_X86_REG_ECX, ecx & 0xFFFFFFFF)

        queue = list(inputs)
        state = {"pos": 0, "cur": None, "clock": 0x1000, "tramp": {}, "ntramp": 0}
        log, err = [], []

        def finish(eax, nargs, pops):
            esp = uc.reg_read(X.UC_X86_REG_ESP)
            ret = struct.unpack("<I", uc.mem_read(esp, 4))[0]
            uc.reg_write(X.UC_X86_REG_EAX, eax & 0xFFFFFFFF)
            uc.reg_write(X.UC_X86_REG_ESP, esp + 4 + (4 * nargs if pops else 0))
            uc.reg_write(X.UC_X86_REG_EIP, ret)

        def stack_args(n):
            esp = uc.reg_read(X.UC_X86_REG_ESP)
            return list(struct.unpack(f"<{n}I", uc.mem_read(esp + 4, 4 * n))) if n else []

        def dispatch():
            """frameTick483cd0: build a trampoline for the next batch and enter it; its final `ret` returns to the
            caller of frameTick (eax = 0)"""
            caller = struct.unpack("<I", uc.mem_read(uc.reg_read(X.UC_X86_REG_ESP) + 0x20, 4))[0]
            if caller in DRAIN_CALLERS:     # a drain: the scripted input never "has arrived" (QueueInput's rule)
                log.append(("drain", DRAIN_CALLERS[caller]))
                return finish(0, 0, False)
            if state["pos"] >= len(queue):
                raise NeedInput("modal wait with no input left")
            batch = []
            while state["pos"] < len(queue):
                c = queue[state["pos"]]
                state["pos"] += 1
                batch.append(c)
                if c.handler in STOP_HANDLERS:
                    break
            # a fresh address per dispatch: Unicorn caches translated blocks, so rewriting one address would run the
            # previous batch's code
            base = TRAMP + 0x100 * (state["ntramp"] % 0x100)
            state["ntramp"] += 1
            if state["ntramp"] % 0x100 == 0:
                uc.ctl_remove_cache(TRAMP, TRAMP + 0x10000)
            code, marks = bytearray(), {}
            for c in batch:
                marks[base + len(code)] = c
                code += b"\xB9" + b"\0\0\0\0"                           # mov ecx, 0 (the root window: unread)
                code += b"\x68" + struct.pack("<I", c.a1 & 0xFFFFFFFF)  # push a1
                code += b"\x68" + struct.pack("<I", c.a0 & 0xFFFFFFFF)  # push a0
                tgt = ROOT_HANDLERS[c.handler]
                at = base + len(code)
                code += b"\xE8" + struct.pack("<i", tgt - (at + 5))     # call handler (stdcall-like: ret 4 / ret 8)
                if c.handler in (8, 9):                                  # one-argument handlers pop 4: drop a1
                    code += b"\x83\xC4\x04"                              # add esp, 4
            marks[base + len(code)] = None                               # past the batch: no command is current
            code += b"\x31\xC0\xC3"                                      # xor eax, eax; ret
            assert len(code) <= 0x100, "batch too long for one trampoline slot"
            uc.mem_write(base, bytes(code))
            state["tramp"] = marks
            uc.reg_write(X.UC_X86_REG_EIP, base)

        def on_code(u, addr, size, _):
            try:
                if TRAMP <= addr < TRAMP + 0x10000:
                    if addr in state["tramp"]:
                        state["cur"] = state["tramp"][addr]
                    return
                if addr in img.imports:
                    name = img.imports[addr].split("!", 1)[1]
                    if name == "timeGetTime":
                        c = state["cur"]
                        if c is not None:
                            return finish(c.t, 0, True)
                        state["clock"] += 1
                        return finish(state["clock"], 0, True)
                    if name == "GetAsyncKeyState":
                        vk = stack_args(1)[0]
                        c = state["cur"]
                        v = 0 if c is None else (c.async_ctrl if vk == 0x11 else c.async_shift if vk == 0x10 else 0)
                        return finish(v, 1, True)
                    if name == "PeekMessageA":
                        return finish(0, 5, True)
                    raise RuntimeError(f"import {img.imports[addr]}")
                if addr == FRAME_TICK and FRAME_TICK not in self.stubs:
                    log.append(("frameTick", state["pos"]))
                    return dispatch()
                nargs, pops, eax, name = self.stubs[addr]
                a = stack_args(nargs)
                log.append((name, a))
                if isinstance(eax, tuple) and eax[0] == "cursor":   # View_toLocal(this, &x, &y): the scripted cursor
                    uc.mem_write(a[0], struct.pack("<i", eax[1][0]))
                    uc.mem_write(a[1], struct.pack("<i", eax[1][1]))
                    v = 0
                elif isinstance(eax, tuple) and eax[0] == "width":   # Text_put(this, text): sum of base + (c & mask)
                    s, p = b"", a[0]
                    while True:
                        ch = bytes(uc.mem_read(p, 1))
                        if ch == b"\0":
                            break
                        s, p = s + ch, p + 1
                    v = sum(eax[1] + (c & eax[2]) for c in s)
                    log.append(("Text_put", s))
                else:
                    v = eax(a, uc.reg_read(X.UC_X86_REG_ECX)) if callable(eax) else eax
                return finish(v, nargs, pops)
            except NeedInput as e:
                err.append(f"NeedInput: {e}")
                u.emu_stop()
            except Exception as e:  # noqa: BLE001 - any stub failure stops the run as a fault
                err.append(str(e))
                u.emu_stop()

        if watch:   # debugging: log (eip, address, size, value) of every write to the watched addresses
            from unicorn import UC_HOOK_MEM_WRITE

            def on_write(u, access, addr, size, value, _):
                if any(a <= addr < a + 4 for a in watch):
                    log.append(("write", hex(u.reg_read(X.UC_X86_REG_EIP)), hex(addr), size, hex(value)))
            uc.hook_add(UC_HOOK_MEM_WRITE, on_write)
        if cover is not None:   # branch coverage: cover[jcc] gets every address executed right after it
            sites = {}
            for f in cover.get("__functions__", [entry]):
                sites.update(VE.jcc_sites(img, f))
            prev = [None]

            def on_ins(u, addr, size, _):
                if prev[0] is not None:
                    cover.setdefault(prev[0], set()).add(addr)
                prev[0] = addr if addr in sites else None
            for f in cover.get("__functions__", [entry]):
                for lo, hi in img.func_ranges.get(f, []):
                    uc.hook_add(UC_HOOK_CODE, on_ins, None, lo, hi - 1)
            cover.setdefault("__sites__", {}).update(sites)
        uc.hook_add(UC_HOOK_CODE, on_code, None, tlo, thi - 1)
        uc.hook_add(UC_HOOK_CODE, on_code, None, TRAMP, TRAMP + 0xFFFF)
        for a in list(self.stubs) + [FRAME_TICK]:
            uc.hook_add(UC_HOOK_CODE, on_code, None, a, a)
        try:
            uc.emu_start(entry, VE.SENTINEL, count=budget)
        except UcError as e:
            err.append(f"{e} at eip {uc.reg_read(X.UC_X86_REG_EIP):#x}")
        if not err and uc.reg_read(X.UC_X86_REG_EIP) != VE.SENTINEL:
            err.append("instruction budget")
        final = bytes(uc.mem_read(VL.DATA_VA, VL.DATA_SIZE))
        return Result(None if err else uc.reg_read(X.UC_X86_REG_EAX), final, "; ".join(err), log, state["pos"])


def coverage_summary(cover) -> tuple[int, int, list]:
    """(jcc sites, sites with both sides reached, [(site, missing side)]) of a `cover` dict filled by Emu.run"""
    sites = cover.get("__sites__", {})
    both, missing = 0, []
    for a, (tgt, fall) in sorted(sites.items()):
        seen = cover.get(a, set())
        if tgt in seen and fall in seen:
            both += 1
        else:
            missing.append((hex(a), "taken" if tgt not in seen else "fall-through"))
    return len(sites), both, missing
