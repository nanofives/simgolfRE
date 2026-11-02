"""S11.1: which shade table each call of Sprite::drawG's three exe wrappers passes (U-1351, U-2001).

Scans golf_clean.exe's .text for `call rel32` to Surface_blit2 0x004740f0, Surface_drawScaled2 0x00474030 and
drawScaledSprite421b90 0x00421b90, decodes the instructions before each call back to the previous call / ret / jmp
and prints the pushes in source order. The shade table is Surface_blit2's 4th stack argument (a4), which
Surface_drawScaled2 passes on from its a4 and drawScaledSprite421b90 from its a4 (notes of the three functions).
Run: ~/bin/py -3.12 log/roadmap/S11/1_shade_sites.py
"""
import pathlib
import sys

import capstone
import pefile

ROOT = pathlib.Path(__file__).resolve().parents[3]
EXE = ROOT / "original" / "golf_clean.exe"
TARGETS = {0x004740F0: "Surface_blit2", 0x00474030: "Surface_drawScaled2", 0x00421B90: "drawScaledSprite421b90"}
TABLES = {0x00822C74: "g_halfBrightLut", 0x00824148: "g_color16Lut"}


def main() -> int:
    pe = pefile.PE(str(EXE), fast_load=True)
    base = pe.OPTIONAL_HEADER.ImageBase
    text = next(s for s in pe.sections if s.Name.rstrip(b"\0") == b".text")
    data = text.get_data()
    va0 = base + text.VirtualAddress
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    sites = []
    for i in range(len(data) - 5):
        if data[i] != 0xE8:
            continue
        rel = int.from_bytes(data[i + 1:i + 5], "little", signed=True)
        tgt = (va0 + i + 5 + rel) & 0xFFFFFFFF
        if tgt in TARGETS:
            sites.append((va0 + i, tgt))
    rows = []
    for site, tgt in sites:
        # decode forward from up to 0x60 bytes before the call, keep the run that lands exactly on the call
        best = None
        for back in range(0x60, 0, -1):
            start = site - back
            ins = list(md.disasm(data[start - va0:site - va0 + 5], start))
            if ins and ins[-1].address == site and ins[-1].mnemonic == "call":
                best = ins
                break
        if best is None:
            print(f"{site:#010x} -> {TARGETS[tgt]}: not decodable")
            continue
        # keep the instructions after the last call / ret / jmp before the site
        tail = []
        for x in reversed(best[:-1]):
            if x.mnemonic in ("call", "ret", "jmp"):
                break
            tail.append(x)
        tail.reverse()
        pushes = [x for x in tail if x.mnemonic == "push"]
        # cdecl/thiscall pushes go last argument first: a4 is the 4th push counted from the call
        a4 = pushes[-4] if len(pushes) >= 4 else None
        txt = "; ".join(f"{x.address:#x} {x.mnemonic} {x.op_str}" for x in tail)
        tag = "?"
        if a4 is not None:
            for va, name in TABLES.items():
                if f"{va:#x}" in a4.op_str:
                    tag = name
            # a register push: find the load into that register in the tail
            if tag == "?":
                reg = a4.op_str.strip()
                for x in reversed(tail[:tail.index(a4)]):
                    if x.mnemonic == "mov" and x.op_str.startswith(reg + ","):
                        for va, name in TABLES.items():
                            if f"{va:#x}" in x.op_str:
                                tag = f"{name} (via {reg} at {x.address:#x})"
                        if tag == "?":
                            tag = f"{reg} <- {x.op_str} at {x.address:#x}"
                        break
        rows.append((site, TARGETS[tgt], f"{a4.address:#x} push {a4.op_str}" if a4 else "-", tag, txt))
    for site, name, a4, tag, txt in rows:
        print(f"{site:#010x}\t{name}\ta4={a4}\t{tag}")
    print(f"sites {len(rows)}")
    print("\n# instruction tails")
    for site, name, a4, tag, txt in rows:
        print(f"{site:#010x} {name}: {txt}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
