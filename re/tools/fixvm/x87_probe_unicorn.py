"""Measure Unicorn's x87 against the Intel SDM on the points sound.dll's A/B evidence depends on (step S4.1 part d).

    ~/bin/py -3.12 port/test/x87_probe_unicorn.py

Prints one line per probe. Expected values are the Intel SDM's (vol. 1 ch. 8): the precision-control field (control word bits
8-9) rounds the significand of fadd / fsub / fmul / fdiv / fsqrt results to 24 / 53 / 64 bits; an inexact result sets PE
(status bit 5); fistp of a value out of range or of an empty register sets IE (bit 0), an empty register also SF (bit 6).
"""
from __future__ import annotations

import struct

from unicorn import UC_ARCH_X86, UC_MODE_32, Uc
from unicorn.x86_const import UC_X86_REG_ESP

CODE, DATA, STACK = 0x1000, 0x10000, 0x20000


def run(code: bytes, data: bytes = b"") -> bytes:
    uc = Uc(UC_ARCH_X86, UC_MODE_32)
    uc.mem_map(CODE, 0x1000)
    uc.mem_map(DATA, 0x1000)
    uc.mem_map(STACK, 0x1000)
    uc.mem_write(CODE, code + b"\xf4")
    uc.mem_write(DATA, data.ljust(0x100, b"\0"))
    uc.reg_write(UC_X86_REG_ESP, STACK + 0x800)
    uc.emu_start(CODE, CODE + len(code))
    return bytes(uc.mem_read(DATA, 0x200))


def m(off: int) -> bytes:
    return struct.pack("<I", DATA + off)


def fldcw(off):
    return b"\xd9\x2d" + m(off)


def probes():
    out = []
    one, three = struct.pack("<d", 1.0), struct.pack("<d", 3.0)
    for cw, bits in ((0x027F, 53), (0x037F, 64), (0x007F, 24)):
        # fninit; fldcw [cw]; fld qword [1.0]; fdiv qword [3.0]; fstp tbyte [0x40]; fnstsw [0x60]
        code = b"\xdb\xe3" + fldcw(0x00) + b"\xdd\x05" + m(0x08) + b"\xdc\x35" + m(0x10) + b"\xdb\x3d" + m(0x40) + b"\xdd\x3d" + m(0x60)
        r = run(code, struct.pack("<H", cw).ljust(8, b"\0") + one + three)
        mant = int.from_bytes(r[0x40:0x48], "little")
        low = mant & ((1 << (64 - bits)) - 1)
        sw = int.from_bytes(r[0x60:0x62], "little")
        out.append((f"1/3 at control word 0x{cw:04x} ({bits}-bit precision): significand 0x{mant:016x}",
                    "honours PC" if low == 0 else f"PC ignored ({64 - bits} low bits nonzero)", f"PE={(sw >> 5) & 1}"))
    # fistp of 0.5 under chop: integer 0, PE expected 1
    code = b"\xdb\xe3" + fldcw(0x00) + b"\xdd\x05" + m(0x08) + b"\xdf\x3d" + m(0x40) + b"\xdd\x3d" + m(0x60)
    r = run(code, struct.pack("<H", 0x0F7F).ljust(8, b"\0") + struct.pack("<d", 0.5))
    sw = int.from_bytes(r[0x60:0x62], "little")
    out.append(("fistp qword of 0.5 (chop)", f"result {struct.unpack('<q', r[0x40:0x48])[0]}", f"status 0x{sw:04x} PE={(sw >> 5) & 1} (SDM: 1)"))
    # fistp qword of 1e19: out of range, IE expected, indefinite stored
    code = b"\xdb\xe3" + fldcw(0x00) + b"\xdd\x05" + m(0x08) + b"\xdf\x3d" + m(0x40) + b"\xdd\x3d" + m(0x60)
    r = run(code, struct.pack("<H", 0x027F).ljust(8, b"\0") + struct.pack("<d", 1e19))
    sw = int.from_bytes(r[0x60:0x62], "little")
    out.append(("fistp qword of 1e19", f"result 0x{int.from_bytes(r[0x40:0x48], 'little'):016x}", f"status 0x{sw:04x} IE={sw & 1} (SDM: 1)"))
    # fistp of an empty register: IE and SF expected
    code = b"\xdb\xe3" + b"\xdf\x3d" + m(0x40) + b"\xdd\x3d" + m(0x60)
    r = run(code)
    sw = int.from_bytes(r[0x60:0x62], "little")
    out.append(("fistp qword of an empty st(0)", f"result 0x{int.from_bytes(r[0x40:0x48], 'little'):016x}",
                f"status 0x{sw:04x} IE={sw & 1} SF={(sw >> 6) & 1} (SDM: 1, 1)"))
    return out


if __name__ == "__main__":
    for p in probes():
        print(" | ".join(p))
