"""Replay recorded real calls offline in Unicorn: ORIGINAL bytes vs our REIMPLEMENTATION, plus the
value the original returned live in the game.

    py -3.12 re/frida/replay.py TerrainDll_tileAt

For every recorded call, both bodies run from identical memory (the module images and the 4 KiB pages
captured around pointer inputs). Three-way check per call:
  live == emulated original   -> the emulator and the captured state reproduce the game (fidelity)
  emulated original == reimpl -> the reimplementation is equivalent on this real input
  side effects                -> every memory write of both runs is compared (address -> bytes)
Reads outside the captured state are reported, not hidden: they name exactly what the next recording
has to capture (that failure is information).

Writes log/diff/<addr>_<name>.replay.csv ending in VERDICT; GREEN needs all three checks on every call.
"""
from __future__ import annotations

import argparse
import csv
import json
import pathlib
import sys

from unicorn import (UC_ARCH_X86, UC_HOOK_MEM_READ_UNMAPPED, UC_HOOK_MEM_WRITE, UC_HOOK_MEM_FETCH_UNMAPPED,
                     UC_HOOK_MEM_WRITE_UNMAPPED, UC_MODE_32, Uc, UcError)
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_EDX, UC_X86_REG_EIP, UC_X86_REG_ESP

ROOT = pathlib.Path(__file__).resolve().parents[2]
STACK_BASE, STACK_SIZE = 0x7F000000, 0x10000
SENTINEL = 0x7EFF0000  # return address; mapped, execution stops on reaching it
PAGE = 0x1000


class Bundle:
    def __init__(self, name: str):
        self.dir = ROOT / "re" / "replay" / name
        self.meta = json.loads((self.dir / "meta.json").read_text())
        self.calls = [json.loads(l) for l in (self.dir / "calls.jsonl").read_text().splitlines()]
        self.images = {m: ((self.dir / "img" / f"{m}.bin").read_bytes(), v["base"]) for m, v in self.meta["images"].items()}

    def page(self, h: str) -> bytes:
        return (self.dir / "pages" / h).read_bytes()


STUB_BASE = 0x7E000000  # negative-control code page (tests only)


def run_one(b: Bundle, call: dict, entry: int, stub: bytes | None = None):
    uc = Uc(UC_ARCH_X86, UC_MODE_32)
    if stub:
        uc.mem_map(STUB_BASE, PAGE)
        uc.mem_write(STUB_BASE, stub)
    mapped = []
    for data, base in b.images.values():
        size = (len(data) + PAGE - 1) & ~(PAGE - 1)
        uc.mem_map(base, size)
        uc.mem_write(base, data)
        mapped.append((base, size))
    for addr, h in call["pages"]:
        if any(lo <= addr < lo + sz for lo, sz in mapped):
            uc.mem_write(addr, b.page(h))  # page inside a mapped image: overlay the live bytes
        else:
            uc.mem_map(addr, PAGE)
            uc.mem_write(addr, b.page(h))
            mapped.append((addr, PAGE))
    uc.mem_map(STACK_BASE, STACK_SIZE)
    uc.mem_map(SENTINEL, PAGE)
    sp = STACK_BASE + STACK_SIZE - 0x100
    args = call["args"]
    frame = [SENTINEL] + args
    for i, v in enumerate(frame):
        uc.mem_write(sp + 4 * i, (v & 0xFFFFFFFF).to_bytes(4, "little"))
    uc.reg_write(UC_X86_REG_ESP, sp)
    uc.reg_write(UC_X86_REG_ECX, call["ecx"])
    uc.reg_write(UC_X86_REG_EDX, call["edx"])
    writes, misses = {}, []

    def on_write(u, access, address, size, value, _):
        if STACK_BASE <= address < STACK_BASE + STACK_SIZE:
            return  # locals/frame layout legitimately differ between bodies
        writes[address] = value & ((1 << (8 * size)) - 1)

    def on_unmapped(u, access, address, size, value, _):
        misses.append(address)
        return False

    uc.hook_add(UC_HOOK_MEM_WRITE, on_write)
    uc.hook_add(UC_HOOK_MEM_READ_UNMAPPED | UC_HOOK_MEM_WRITE_UNMAPPED | UC_HOOK_MEM_FETCH_UNMAPPED, on_unmapped)
    err = None
    try:
        uc.emu_start(entry, SENTINEL, count=1_000_000)
    except UcError as e:
        err = str(e)
    eip = uc.reg_read(UC_X86_REG_EIP)
    esp = uc.reg_read(UC_X86_REG_ESP)
    return {"eax": uc.reg_read(UC_X86_REG_EAX), "writes": writes, "misses": misses,
            "err": err if eip != SENTINEL else None, "pop": esp - sp - 4}


def replay(name: str, out_dir: pathlib.Path | None = None, wrong_stub: bytes | None = None, limit: int | None = None) -> bool:
    """wrong_stub (tests only): x86 bytes run instead of the detour, to prove a wrong body reads RED."""
    if wrong_stub is not None and out_dir is None:
        raise ValueError("negative-control runs must not write into log/diff (evidence)")
    b = Bundle(name)
    m = b.meta
    out_dir = out_dir or ROOT / "log" / "diff"
    out_dir.mkdir(parents=True, exist_ok=True)
    path = out_dir / f"{m['addr']:08x}_{name}.replay.csv"
    bad = {"fidelity": 0, "equivalence": 0, "side_effects": 0, "stack": 0, "misses": 0}
    miss_examples = []
    with path.open("w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["call", "live", "original_emulated", "reimpl_emulated", "match"])
        for i, c in enumerate(b.calls[:limit]):
            o = run_one(b, c, m["target"])
            r = run_one(b, c, STUB_BASE, wrong_stub) if wrong_stub else run_one(b, c, m["detour"])
            fid = o["eax"] == c["live"] and not o["err"]
            eq = o["eax"] == r["eax"] and not r["err"]
            se = o["writes"] == r["writes"]
            st = o["pop"] == r["pop"]  # callee-cleaned bytes (ret N) must agree
            if o["misses"] or r["misses"]:
                bad["misses"] += 1
                if len(miss_examples) < 5:
                    miss_examples.append((i, [hex(x) for x in (o["misses"] + r["misses"])[:3]], hex(c["ecx"])))
            bad["fidelity"] += not fid
            bad["equivalence"] += not eq
            bad["side_effects"] += not se
            bad["stack"] += not st
            arg = " ".join([f"ecx={c['ecx']:#x}"] + [str(a if a < 0x80000000 else a - 0x100000000) for a in c["args"]])
            w.writerow([arg, f"{c['live']:#x}", f"{o['eax']:#x}", f"{r['eax']:#x}", fid and eq and se and st])
        wit = m["install_witness"]
        w.writerow(["install_witness", f"0x{wit['byte']:02x}", "0xe9", wit["ok"]])
        w.writerow(["meta", f"scenario={m['scenario']}", f"recorded={m['recorded_at']}",
                    f"calls_seen={m['calls_seen']} every={m['every']}"])
        n = len(b.calls[:limit])
        green = n > 0 and not any(bad.values())
        w.writerow(["VERDICT", "GREEN" if green else "RED", n, json.dumps(bad)])
    print(f"{name}: {'GREEN' if green else 'RED'}  {n} real calls  failures={bad}  -> {path}")
    for ex in miss_examples:
        print(f"  call {ex[0]}: reads outside captured state at {ex[1]} (ecx={ex[2]}) -> capture more")
    return green


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("name")
    a = ap.parse_args()
    return 0 if replay(a.name) else 1


if __name__ == "__main__":
    sys.exit(main())
