"""S4.4: the port's x87 operations (portsim::x87, audio_mixer.x80batch) against the emulator's x87 (Unicorn, the oracle's
own FPU) on random and edge operands, per operation and control word.

    ~/bin/py -3.12 log/roadmap/S4/4_x80probe.py [--n 4000] [--seed 1] [--w32] [--soft]
--w32 runs the Win32 DLL (its x87 instructions; with --soft its software path). Prints mismatches per op / word.
"""
from __future__ import annotations

import argparse
import os
import pathlib
import random
import struct
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT))
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32  # noqa: E402
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_FPCW, UC_X86_REG_FPSW  # noqa: E402

OPS = {0: "add", 1: "sub", 2: "mul", 3: "div", 4: "fst64", 5: "fist16", 6: "fist32", 7: "fist64", 8: "fcom",
       9: "fld64", 10: "from_f64", 11: "fild64"}
WORDS = (0x027F, 0x0F7F, 0x037F, 0x007F, 0x0B7F, 0x077F, 0x067F, 0x0E7F)
CODE, DATA = 0x10000, 0x20000
A, B, R, CW = DATA, DATA + 0x10, DATA + 0x20, DATA + 0x30


def t10(m, se):
    return struct.pack("<QH", m, se)


class Emu:
    def __init__(self):
        uc = Uc(UC_ARCH_X86, UC_MODE_32)
        uc.mem_map(CODE, 0x1000)
        uc.mem_map(DATA, 0x1000)
        uc.mem_map(0x80000, 0x10000)
        self.uc = uc
        # (code bytes) per op: fninit; fldcw [CW]; operands; op; store
        fld_b = b"\xdb\x2d" + struct.pack("<I", B)          # fld tbyte [B]
        fld_a = b"\xdb\x2d" + struct.pack("<I", A)          # fld tbyte [A]
        fstp_r = b"\xdb\x3d" + struct.pack("<I", R)         # fstp tbyte [R]
        pre = b"\xdb\xe3" + b"\xd9\x2d" + struct.pack("<I", CW)   # fninit; fldcw [CW]
        self.code = {
            0: pre + fld_b + fld_a + b"\xd8\xc1" + fstp_r,        # fadd st(0), st(1)
            1: pre + fld_b + fld_a + b"\xd8\xe1" + fstp_r,        # fsub st(0), st(1)
            2: pre + fld_b + fld_a + b"\xd8\xc9" + fstp_r,        # fmul st(0), st(1)
            3: pre + fld_b + fld_a + b"\xd8\xf1" + fstp_r,        # fdiv st(0), st(1)
            4: pre + fld_a + b"\xdd\x1d" + struct.pack("<I", R),  # fstp qword [R]
            5: pre + fld_a + b"\xdf\x1d" + struct.pack("<I", R),  # fistp word [R]
            6: pre + fld_a + b"\xdb\x1d" + struct.pack("<I", R),  # fistp dword [R]
            7: pre + fld_a + b"\xdf\x3d" + struct.pack("<I", R),  # fistp qword [R]
            8: pre + fld_b + fld_a + b"\xde\xd9",                 # fcompp (status read after)
            9: pre + b"\xdd\x05" + struct.pack("<I", A) + fstp_r,  # fld qword [A]
            11: pre + b"\xdf\x2d" + struct.pack("<I", A) + fstp_r,  # fild qword [A]
        }

    def run(self, op, cw, a, b):
        uc = self.uc
        uc.mem_write(DATA, b"\0" * 0x40)
        if op in (9, 11):
            uc.mem_write(A, struct.pack("<Q", a[0]))
        else:
            uc.mem_write(A, t10(*a))
        uc.mem_write(B, t10(*b))
        uc.mem_write(CW, struct.pack("<H", cw))
        c = self.code[op]
        uc.mem_write(CODE, c)
        uc.reg_write(UC_X86_REG_ESP, 0x88000)
        uc.emu_start(CODE, CODE + len(c))
        if op == 8:
            return 0, uc.reg_read(UC_X86_REG_FPSW) & 0x4700
        r = bytes(uc.mem_read(R, 10))
        if op in (0, 1, 2, 3, 9, 11):
            return struct.unpack("<QH", r)
        n = {4: 8, 5: 2, 6: 4, 7: 8}[op]
        v = int.from_bytes(r[:n], "little", signed=op in (5, 6, 7))
        return v & 0xFFFFFFFFFFFFFFFF, 0


def rand_x80(rng: random.Random):
    k = rng.random()
    if k < 0.35:   # a double, as most operands are
        return dbl_x80(rand_double(rng))
    if k < 0.75:   # a random normal extended value in a moderate range
        return (rng.getrandbits(64) | (1 << 63), (rng.getrandbits(1) << 15) | rng.randint(16383 - 80, 16383 + 80))
    if k < 0.85:   # close exponents / equal magnitudes (cancellation)
        e = 16383 + rng.randint(-3, 3)
        m = (1 << 63) | rng.choice((0, 1, 2, 3, (1 << 63) - 1, rng.getrandbits(63)))
        return (m, (rng.getrandbits(1) << 15) | e)
    specials = [(0, 0), (0, 0x8000), (1 << 63, 0x7FFF), (1 << 63, 0xFFFF), (0xC000000000000000, 0xFFFF),
                (0xC000000000000001, 0x7FFF), (0xA000000000000000, 0x7FFF), (0xE000000000000000, 0xFFFF),
                ((1 << 64) - 1, 0x403E), (1 << 63, 0x401E), (1 << 63, 0x403E), ((1 << 63) | 1, 0x3FFE)]
    return rng.choice(specials)


def rand_double(rng: random.Random):
    k = rng.random()
    if k < 0.6:
        return struct.unpack("<Q", struct.pack("<d", rng.uniform(-70000, 70000) * rng.choice((1, 1e-3, 1e3, 1 / 3))))[0]
    if k < 0.8:
        return rng.getrandbits(64)
    return rng.choice([0, 1 << 63, 0x7FF0000000000000, 0xFFF0000000000000, 0x7FF8000000000000, 0x7FF0000000000001,
                       0x0000000000000001, 0x000FFFFFFFFFFFFF, 0x0010000000000000, 0x7FEFFFFFFFFFFFFF,
                       0x3FF0000000000000, 0x4000000000000000, 0x3FD7384EF2A605CE])


def dbl_x80(bits):
    s = bits >> 63
    ex = (bits >> 52) & 0x7FF
    f = bits & ((1 << 52) - 1)
    if ex == 0x7FF:
        return ((1 << 63) | (f << 11), (s << 15) | 0x7FFF)
    if ex == 0:
        if f == 0:
            return (0, s << 15)
        c = 64 - f.bit_length()
        return (f << c, (s << 15) | (16383 + 63 - 1074 - c))
    return ((1 << 63) | (f << 11), (s << 15) | (ex - 1023 + 16383))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--n", type=int, default=4000)
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--w32", action="store_true")
    ap.add_argument("--soft", action="store_true")
    a = ap.parse_args()
    rng = random.Random(a.seed)
    recs = []
    for _ in range(a.n):
        op = rng.choice([0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 11])
        cw = rng.choice(WORDS)
        if op in (9,):
            x = (rand_double(rng), 0)
        elif op == 11:
            x = (rng.choice([rng.getrandbits(64), rng.getrandbits(20), 0, 1 << 63, (1 << 64) - 1]), 0)
        elif op in (5, 6, 7) and rng.random() < 0.5:
            x = dbl_x80(struct.unpack("<Q", struct.pack("<d", rng.uniform(-3e9, 3e9) * rng.choice((1, 1e-4, 1e6, 1e12))))[0])
        else:
            x = rand_x80(rng)
        y = rand_x80(rng)
        if op in (0, 1) and rng.random() < 0.3:   # nearby magnitudes
            y = (x[0] ^ rng.getrandbits(rng.randint(1, 70)) & ((1 << 64) - 1) | (1 << 63), x[1] ^ rng.choice((0, 0x8000)))
        recs.append((op, cw, x, y))
    from port.test import sound_replay as S
    S.MIRROR = ROOT / "port" / "build" / "S4_4_mirror"
    if a.w32:
        lib = S.load_w32()
    else:
        lib = S.load_lib()
    st = lib.state()
    blob = b"".join(struct.pack("<II", op, cw) + t10(*x) + t10(*y) + b"\0" * 4 for op, cw, x, y in recs)
    st.files_put("$audio/x80in", blob)
    n = st.call("audio_mixer.x80batch", [1 if a.soft else 0]).eax
    out = st.files_read("$audio/x80out")
    st.close()
    assert n == len(recs), n
    emu = Emu()
    bad = {}
    tot = {}
    shown = 0
    for i, (op, cw, x, y) in enumerate(recs):
        v, s = struct.unpack_from("<QH", out, 16 * i)
        ev, es = emu.run(op, cw, x, y)
        key = (OPS[op], cw)
        tot[key] = tot.get(key, 0) + 1
        if (v, s) != (ev, es):
            bad[key] = bad.get(key, 0) + 1
            if shown < 25:
                shown += 1
                print(f"MISMATCH {OPS[op]} cw {cw:04x} a ({x[0]:016x},{x[1]:04x}) b ({y[0]:016x},{y[1]:04x}): "
                      f"port ({v:016x},{s:04x}) emu ({ev:016x},{es:04x})")
    ops_tot = {}
    for (o, w), c in tot.items():
        ops_tot.setdefault(o, [0, 0])[0] += c
        ops_tot[o][1] += bad.get((o, w), 0)
    for o, (c, b) in sorted(ops_tot.items()):
        print(f"[{o}] {c} records, {b} mismatches")
    from port.test.test_audio_mixer import is_snan_move
    other = sum(1 for (op, cw, x, y), o, e in zip(recs, [struct.unpack_from("<QH", out, 16 * i) for i in range(len(recs))],
                                                 [emu.run(*r) for r in recs]) if o != e and not is_snan_move(op, x))
    print(f"records {len(recs)} mismatches {sum(bad.values())} (not signaling-NaN moves: {other}) backend {'w32' if a.w32 else 'x64'}{' soft' if a.soft else ''}")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
