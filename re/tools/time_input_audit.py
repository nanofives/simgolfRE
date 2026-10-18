r"""Phase 2.3 (2026-10-18): every call site of a clock, timer, wait or input import in golf_clean.exe, jgld.dll,
sound.dll and Terrain.dll, offline (capstone over the Ghidra function ranges, names from re/names via xref.names).

    ~/bin/py -3.12 re/tools/time_input_audit.py                 # TSV on stdout
    ~/bin/py -3.12 re/tools/time_input_audit.py --out re/analysis/sim/time_input_sites.tsv

A site is an instruction whose memory operand is the import's IAT slot: `call [slot]` (kind call), `mov reg, [slot]`
(kind load; every later `call reg` in the same function before reg is written again is listed as kind callreg with the
load's address in `via`), or `jmp [slot]` (kind jmp: an import thunk; its callers are listed as kind thunkcall).
Columns: module api site kind via function_entry function_name. Sites outside every function are reported with entry -.
"""
from __future__ import annotations

import argparse
import pathlib
import sys

import capstone
import pefile

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "re" / "tools"))
import xref  # noqa: E402

APIS = {
    # clocks
    "timeGetTime", "QueryPerformanceCounter", "QueryPerformanceFrequency", "GetTickCount", "GetSystemTime",
    "GetLocalTime", "GetSystemTimeAsFileTime", "GetTimeZoneInformation",
    # timers and waits
    "SetTimer", "KillTimer", "timeSetEvent", "timeKillEvent", "timeBeginPeriod", "timeEndPeriod", "timeGetDevCaps",
    "Sleep", "WaitMessage", "WaitForSingleObject", "MsgWaitForMultipleObjects",
    # input
    "GetKeyboardState", "GetKeyState", "GetAsyncKeyState", "GetCursorPos", "ScreenToClient", "ClientToScreen",
    "PeekMessageA", "GetMessageA", "TranslateMessage", "DispatchMessageA", "PostMessageA", "PostThreadMessageA",
    "SetCapture", "ReleaseCapture", "MapVirtualKeyA", "ShowCursor", "SetCursor", "SetCursorPos", "ClipCursor",
    "keybd_event", "SetKeyboardState", "GetSystemMetrics", "GetDoubleClickTime", "GetMessageTime", "GetMessagePos",
}
MODULES = ["golf_clean.exe", "jgld.dll", "sound.dll", "Terrain.dll"]


def iat_slots(mod: str) -> dict[int, str]:
    pe = pefile.PE(str(ROOT / "original" / mod), fast_load=True)
    pe.parse_data_directories()
    out = {}
    for d in pe.DIRECTORY_ENTRY_IMPORT:
        for i in d.imports:
            n = i.name.decode() if i.name else f"#{i.ordinal}"
            if n in APIS:
                out[i.address] = n
    return out


def scan(mod: str) -> list[list[str]]:
    slots = iat_slots(mod)
    if not slots:
        return []
    pe, base = xref.load_pe(mod)
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    md.detail = True
    nm = xref.names(mod)
    fns = xref.functions(mod)
    rows, covered, thunks = [], [], {}

    def fname(va):
        n = nm.get(va)
        return n[1] if n else next((f[1] for f in fns if f[0] == va), "?")

    def walk(insns, entry):
        loaded = {}                      # reg -> (api, load site)
        for ins in insns:
            ops = ins.operands
            api = None
            for op in ops:
                if op.type == capstone.x86.X86_OP_MEM and not op.mem.base and not op.mem.index:
                    api = slots.get(op.mem.disp & 0xFFFFFFFF) or api
            if api and ins.mnemonic == "call":
                rows.append([mod, api, f"0x{ins.address:08x}", "call", "-", entry])
            elif api and ins.mnemonic == "jmp":
                rows.append([mod, api, f"0x{ins.address:08x}", "jmp", "-", entry])
                thunks[ins.address] = api
            elif api and ins.mnemonic == "mov" and ops[0].type == capstone.x86.X86_OP_REG:
                loaded[ops[0].reg] = (api, ins.address)
                rows.append([mod, api, f"0x{ins.address:08x}", "load", "-", entry])
            elif api:
                rows.append([mod, api, f"0x{ins.address:08x}", ins.mnemonic, "-", entry])
            if ins.mnemonic == "call" and ops and ops[0].type == capstone.x86.X86_OP_REG and ops[0].reg in loaded:
                a, at = loaded[ops[0].reg]
                rows.append([mod, a, f"0x{ins.address:08x}", "callreg", f"0x{at:08x}", entry])
            # a register written by anything else stops being the import pointer (call clobbers eax/ecx/edx)
            if ins.mnemonic == "call":
                for r in (capstone.x86.X86_REG_EAX, capstone.x86.X86_REG_ECX, capstone.x86.X86_REG_EDX):
                    loaded.pop(r, None)
            elif ops and ops[0].type == capstone.x86.X86_OP_REG and ins.mnemonic not in ("cmp", "test", "push") \
                    and not (api and ins.mnemonic == "mov"):
                loaded.pop(ops[0].reg, None)

    for va, _name, _size, rngs in fns:
        insns = []
        for lo, hi in rngs:
            try:
                insns += list(md.disasm(pe.get_data(lo - base, hi - lo), lo))
            except Exception:  # noqa: BLE001
                continue
            covered.append((lo, hi))
        walk(insns, va)
    # thunk callers (an import thunk is `jmp [slot]`): every `call thunk` in any function
    if thunks:
        for va, _name, _size, rngs in fns:
            for lo, hi in rngs:
                try:
                    for ins in md.disasm(pe.get_data(lo - base, hi - lo), lo):
                        if ins.mnemonic == "call" and ins.operands and ins.operands[0].type == capstone.x86.X86_OP_IMM:
                            t = ins.operands[0].imm & 0xFFFFFFFF
                            if t in thunks and not (lo <= t < hi):
                                rows.append([mod, thunks[t], f"0x{ins.address:08x}", "thunkcall", f"0x{t:08x}", va])
                except Exception:  # noqa: BLE001
                    continue
    out = []
    seen = set()
    for r in rows:
        k = (r[2], r[3])
        if k in seen:
            continue
        seen.add(k)
        e = r[5]
        out.append(r[:5] + ([f"0x{e:08x}", fname(e)] if e is not None else ["-", "-"]))
    return sorted(out, key=lambda r: (r[0], r[1], r[2]))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out")
    ap.add_argument("--module", action="append")
    a = ap.parse_args()
    lines = ["module\tapi\tsite\tkind\tvia\tfunction_entry\tfunction_name"]
    for mod in a.module or MODULES:
        lines += ["\t".join(r) for r in scan(mod)]
    text = "\n".join(lines) + "\n"
    if a.out:
        pathlib.Path(a.out).write_text(text)
        print(f"{len(lines) - 1} sites -> {a.out}")
    else:
        sys.stdout.write(text)


if __name__ == "__main__":
    main()
