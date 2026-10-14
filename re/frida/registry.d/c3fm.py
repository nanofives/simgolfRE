"""Batch c3fm (round 34): golf_clean.exe static CRT, openHookA/B/C, Window::passLocalRect / blitToTarget, DC_call90 /
DC_calla4, jgld.dll's debug CRT, jgld's dashed lines.

Reimplementations shim/src/re/c3fm.cpp, fixtures re/frida/js/fixtures.d/c3fm.js, purposes log/c3/c3fm_purpose.md.
Every fixture freezes the game's first thread and calls __freeze.negOn() after it (the negative-control switch).
String names ($A16, $HFOX ...) are the fixture's STR table; pointer results are reported relative to the fixture's
block ($obj) so the CSVs read as offsets.
"""

HOOKS = {}
_X = dict(module="golf_clean.exe")
_J = dict(module="jgld.dll")
FMLOG, VLOG = 0x1000, 0x1000
_STRBLK = ("$strblk", 0, 0x40 * 128)

# ===================================================================================================== group X: exe CRT
# _strchr 0x004a6170: aligned 16-character string at each byte position of the first and a later dword, misses, c == 0,
# the unaligned starts (A16_1/2/3, U1N, U2, U3, E1, E3), the c false positive (FPC + 'x', c 0x80 on Z*), the zero false
# positive (FPZ, H80), the zero exits of the aligned loop (Z0..Z3), the zero-before-c exits of the candidate block
# (ZB0..ZB3), high bytes (HB). Coverage per jcc: log/c3/c3fm_purpose.md (model log/c3/alt/c3fm_model.py).
_CHR = ([("$A16", c) for c in (0x61, 0x62, 0x63, 0x64, 0x65, 0x68, 0x6C, 0x70, 0x7A, 0x00)]
        + [("$A16_1", 0x62), ("$A16_1", 0x61), ("$A16_2", 0x63), ("$A16_2", 0x00), ("$A16_3", 0x64), ("$A16_3", 0x7A),
           ("$A16_5", 0x6B), ("$E", 0x61), ("$E", 0x00), ("$E1", 0x61), ("$E3", 0x00), ("$U1N", 0x71), ("$U1N", 0x72),
           ("$U2", 0x79), ("$U2", 0x7A), ("$U3", 0x73), ("$U3", 0x77), ("$U3", 0x00),
           ("$FPC", 0x78), ("$FPC", 0x41), ("$FPZ", 0x7A), ("$FPZ", 0x71),
           ("$Z0", 0x71), ("$Z1", 0x71), ("$Z2", 0x71), ("$Z3", 0x71), ("$Z3", 0x80), ("$Z0", 0x80), ("$Z1", 0x80),
           ("$ZB0", 0x71), ("$ZB1", 0x71), ("$ZB2", 0x71), ("$ZB3", 0x80),
           ("$HB", 0xFF), ("$HB", 0xC8), ("$HB", 0x80), ("$H80", 0x80), ("$H80", 0x00), ("$H80", 0x61),
           ("$A16", 0x161), ("$A16", -158)])
HOOKS["_strchr"] = dict(_X, addr=0x004A6170, abi="default", ret="pointer", args=["pointer", "int"],
                        fixture="c3fm_str", state=[_STRBLK], vectors=_CHR)

# _strstr 0x004a64f0: empty and one-character needles (the strchr tail 0x004a655e), first-character match at the start
# and inside the scan, a second-character mismatch, the needle-exhausted exits for length 2 / 3 / 4+, the even / odd
# position mismatches that restart the attempt, misses, an empty haystack.
_STRSTR = [("$HABCABCABD", "$E"), ("$HABCABCABD", "$NA"), ("$E", "$NAB"), ("$HABCABCABD", "$NAB"), ("$HXX", "$NAB"),
           ("$HAXBAB", "$NAB"), ("$HABCABCABD", "$NABC"), ("$HABCABCABD", "$NABD"), ("$HABCABCABD", "$NZZZ"),
           ("$HFOX", "$NLAZY"), ("$HFOX", "$NDOG"), ("$HABCDE", "$NABCDE"), ("$HABCDX", "$NABCD"),
           ("$HABCD", "$NABCDE"), ("$HAAAB", "$NAB"), ("$HAAAB", "$NAAB"), ("$HABCABD", "$NABD"),
           ("$HABCABCABD", "$NABCABD"), ("$E", "$E"), ("$HABCABCABD", "$NC"), ("$E", "$NA"), ("$HFOX", "$NZZZ"),
           ("$HABCXE", "$NABCDE"), ("$HXX", "$NABCDEF"), ("$HFOX_3", "$NDOG"), ("$A16", "$NBC"), ("$A16_1", "$NBC"),
           ("$HABCD", "$HABCD"), ("$FPC", "$NC")]
HOOKS["_strstr"] = dict(_X, addr=0x004A64F0, abi="default", ret="pointer", args=["pointer", "pointer"],
                        fixture="c3fm_str", state=[_STRBLK], vectors=_STRSTR)

# _strtok 0x004a678b: state = the strings (tokens are cut in place) and the saved position at the A/B thread's
# __getptd block + 0x18. NULL s continues from the fixture's preset position (T6 ";;tok1;tok2", the terminator of
# "::", ",b" of "a,b").
_TOKST = [_STRBLK, ("$ptd", 0x18, 4)]
_TOK = [("$T0", "$D0"), ("$T0_4", "$D0"), ("$T1", "$D4"), ("$E", "$D0"), ("$T3", "$DE"), ("$T4", "$D2"), ("$T5", "$D0"),
        ("$T6", "$D3"), ("$T7", "$D3"), ("$T8", "$D4"), ("$A16", "$D5"), ("$L20", "$D4"), ("$HB", "$D2"),
        (0, "$D3"), (0, "$D4"), (0, "$DE"), (0, "$D0")]
HOOKS["_strtok"] = dict(_X, addr=0x004A678B, abi="default", ret="pointer", args=["pointer", "pointer"],
                        fixture="c3fm_tok", state=_TOKST, vectors=_TOK)
HOOKS["_strtok_end"] = dict(_X, addr=0x004A678B, abi="default", ret="pointer", args=["pointer", "pointer"],
                            fixture="c3fm_tok_end", state=_TOKST,
                            vectors=[(0, "$D3"), (0, "$D4"), (0, "$DE"), ("$T7", "$D4"), ("$T7", "$D3")])
HOOKS["_strtok_mid"] = dict(_X, addr=0x004A678B, abi="default", ret="pointer", args=["pointer", "pointer"],
                            fixture="c3fm_tok_mid", state=_TOKST,
                            vectors=[(0, "$D4"), (0, "$D3"), (0, "$DE"), (0, "$D5")])

# __itoa 0x004ad425 / _xtoa 0x004ad452: buffers B0..B7 (0xEE-filled, at alignment offsets 0..3) in $iblk.
_IBLK = [("$iblk", 0, 0x200)]
_ITOA = [(0, "$B0", 10), (5, "$B1", 10), (-5, "$B2", 10), (123456789, "$B3", 10), (-0x80000000, "$B4", 10),
         (0x7FFFFFFF, "$B5", 10), (-1, "$B6", 16), (255, "$B7", 16), (-255, "$B0", 16), (35, "$B1", 36),
         (1295, "$B2", 36), (7, "$B3", 2), (-7, "$B4", 2), (12345, "$B5", 8), (100, "$B6", 37), (5, "$B7", -3),
         (-1, "$B0", 0x10000), (0, "$B1", 1), (9, "$B2", 10), (10, "$B3", 10), (-10, "$B4", 10), (36, "$B5", 36)]
HOOKS["__itoa"] = dict(_X, addr=0x004AD425, abi="default", ret="pointer", args=["int", "pointer", "int"],
                       fixture="c3fm_itoa", state=_IBLK, vectors=_ITOA)
_XTOA = [(0, "$B0", 10, 0), (5, "$B1", 10, 1), (123, "$B2", 10, 0), (123, "$B3", 10, 1), (0xFFFFFFFF, "$B4", 16, 0),
         (0xFFFFFFFF, "$B5", 16, 1), (0x80000000, "$B6", 10, 1), (255, "$B7", 2, 0), (255, "$B0", 2, 7),
         (1295, "$B1", 36, 0), (100, "$B2", 37, 0), (5, "$B3", 0xFFFFFFFD, 0), (0xFFFFFFFF, "$B4", 0x10000, 0),
         (9, "$B5", 10, 0), (10, "$B6", 10, 0), (0x12345678, "$B7", 16, 0), (0, "$B0", 1, 0), (0, "$B1", 10, 1)]
HOOKS["_xtoa"] = dict(_X, addr=0x004AD452, abi="default", ret="uint32", args=["uint32", "pointer", "uint32", "int"],
                      fixture="c3fm_itoa", state=_IBLK, vectors=_XTOA)

# _fopen 0x004a614d: __fsopen stood in (records the strings and the share flag, answers from them).
_FOPEN = [("$HFOX", "$NA"), ("$NABC", "$NAB"), ("$T3", "$NC"), ("$E", "$NA"), ("$L20", "$U2"), ("$A16_1", "$NDOG"),
          (0, "$NA"), ("$NZZZ", 0), ("$HB", "$E")]
HOOKS["_fopen"] = dict(_X, addr=0x004A614D, abi="default", ret="uint32", args=["pointer", "pointer"],
                       fixture="c3fm_fopen", state=[_STRBLK, ("$fmlog", 0, FMLOG)], vectors=_FOPEN)

# _fclose 0x004a609f / _fgets 0x004a65ee: FILE records in $fblk; __lock_file / __unlock_file / __fclose_lk / __filbuf
# stood in (log in $fmlog). G0..G11 have the flag words 0x40, 0x41, 1, 2, 0x80, 0x83, 0, 0xc0, 0x140, 0x4000, 0x10,
# 0xffffffbf; F0..F7 the data / refill cases (fixture FILES).
_FILESST = [("$fblk", 0, 0x1000), ("$fmlog", 0, FMLOG)]
HOOKS["_fclose"] = dict(_X, addr=0x004A609F, abi="default", ret="int", args=["pointer"], fixture="c3fm_files",
                        state=_FILESST, vectors=[(f"$G{i}",) for i in range(12)] + [("$F0",), ("$F6",)])
_FGETS = [("$B0", 40, "$F0"), ("$B1", 5, "$F0"), ("$B2", 1, "$F0"), ("$B3", 0, "$F0"), ("$B0", -3, "$F0"),
          ("$B1", 40, "$F1"), ("$B2", 6, "$F1"), ("$B3", 40, "$F2"), ("$B0", 40, "$F3"), ("$B1", 3, "$F3"),
          ("$B2", 40, "$F4"), ("$B3", 40, "$F5"), ("$B0", 2, "$F2"), ("$B1", 10, "$F0"), ("$B2", 40, "$F6"),
          ("$B3", 40, "$F7"), ("$B0", 2, "$F7"), ("$B1", 0x7FFFFFFF, "$F3"), ("$B2", -0x80000000, "$F0")]
HOOKS["_fgets"] = dict(_X, addr=0x004A65EE, abi="default", ret="pointer", args=["pointer", "int", "pointer"],
                       fixture="c3fm_files", state=_FILESST, vectors=_FGETS)

# ============================================================================================== group O: openHook A/B/C
# O0 / O2 device pointer NULL, O1 / O3 set; the entry point a recorder (keys without suffix) or NULL (_null).
_OST = [("$oblk", 0, 0x200), ("$vlog", 0, VLOG)]
_OV = [("$O0", 0), ("$O0", 1), ("$O0", 7), ("$O0", 0x1234), ("$O0", -1), ("$O1", 5), ("$O2", 0x10), ("$O3", 7),
       ("$O2", 7), ("$O1", 0)]
for _k, _a, _f in (("openHookA", 0x004873F0, "c3fm_open_a"), ("openHookB", 0x004879B0, "c3fm_open_b"),
                   ("openHookC", 0x00487B90, "c3fm_open_c")):
    HOOKS[_k] = dict(_X, addr=_a, abi="thiscall", ret="int", args=["pointer", "int"], fixture=_f, state=_OST,
                     vectors=_OV)
    HOOKS[_k + "_null"] = dict(_X, addr=_a, abi="thiscall", ret="int", args=["pointer", "int"], fixture=_f + "_null",
                               state=_OST, vectors=_OV[:6])

# ================================================================================================= group W: windows
HOOKS["Window::passLocalRect"] = dict(_X, addr=0x0047B9A0, abi="thiscall", ret="int", args=["pointer"],
                                      fixture="c3fm_plr", state=[("$wblk", 0, 0x200 * 9), ("$fmlog", 0, FMLOG)],
                                      vectors=[(f"$P{i}",) for i in range(9)])

_DCST = [("$dblk", 0, 0x400), ("$vlog", 0, VLOG)]
_DCV = [("$D0", "$T0", 1, 2, 3), ("$D0", "$T1", 4, 5, 6), ("$D0", "$T2", -1, -2, -3), ("$D2", "$T0", 0, 0, 0),
        ("$D1", "$T0", 1, 2, 3), ("$D1", 0, 1, 2, 3), ("$D0", 0, 7, 8, 9), ("$D2", 0, 0, 0, 0),
        ("$D0", "$T0", 0x7FFFFFFF, -0x80000000, 100), ("$D2", "$T2", 640, 480, 0x12345), ("$D0", "$T1", 9, 8, 7)]
HOOKS["DC_call90"] = dict(_X, addr=0x00475BA0, abi="thiscall", ret="int", args=["pointer", "pointer", "int", "int", "int"],
                          fixture="c3fm_dc", state=_DCST, vectors=_DCV)
HOOKS["DC_calla4"] = dict(_X, addr=0x00475BE0, abi="thiscall", ret="int", args=["pointer", "pointer", "int", "int", "int"],
                          fixture="c3fm_dc", state=_DCST, vectors=_DCV)

# Window::blitToTarget 0x00480dc0: nine windows (flags / +0xa4 / +0xa8 / +0xac combinations selecting the four DC calls
# and the blit) under five global settings; the stack argument is never read (0x5a5a passed).
_BTTST = [("$wblk", 0, 0x400 * 10), ("$vlog", 0, VLOG), ("$fmlog", 0, FMLOG)]
for _sfx in ("redirect", "redirect_ts0", "screen", "screen_ts0", "nodisp"):
    HOOKS["Window::blitToTarget_" + _sfx] = dict(_X, addr=0x00480DC0, abi="thiscall", ret="int",
                                                 args=["pointer", "int"], fixture="c3fm_btt_" + _sfx, state=_BTTST,
                                                 vectors=[(f"$W{i}", 0x5A5A) for i in range(9)])

# ============================================================================================== group J: jgld debug CRT
HOOKS["fopen"] = dict(_J, addr=0x0007FAA0, abi="default", ret="uint32", args=["pointer", "pointer"],
                      fixture="c3fm_jfopen", state=[_STRBLK, ("$fmlog", 0, FMLOG)], vectors=_FOPEN)
# NULL is not passed: the original reports it and then reads the flag through the NULL pointer (0x1007f88b).
HOOKS["fclose"] = dict(_J, addr=0x0007F850, abi="default", ret="int", args=["pointer"], fixture="c3fm_jfile",
                       state=[("$fblk", 0, 0x400), ("$fmlog", 0, FMLOG)], vectors=[(f"$G{i}",) for i in range(12)])
HOOKS["operator_new"] = dict(_J, addr=0x0007F0F0, abi="default", ret="uint32", args=["uint32"], fixture="c3fm_jnew",
                             state=[("$fmlog", 0, FMLOG)],
                             vectors=[(s,) for s in (0, 1, 4, 0x14C, 0x50, 100, 0x7FFFFFFF, 0xFFFFFFFF, 0x10000)])
_LEN = ["$A16", "$A16_1", "$A16_2", "$A16_3", "$E", "$E1", "$E3", "$U1N", "$U2", "$U3", "$L20", "$L20_1", "$L20_2",
        "$L20_3", "$L7", "$L5", "$L6", "$L80", "$LFF", "$FPZ", "$H80", "$HB", "$Z0", "$Z1", "$Z2", "$Z3", "$HFOX"]
HOOKS["strlen"] = dict(_J, addr=0x0007F6E0, abi="default", ret="uint32", args=["pointer"], fixture="c3fm_jstr",
                       state=[_STRBLK], vectors=[(s,) for s in _LEN])
_CAT = [("$C0", "$A16"), ("$C1", "$E"), ("$C2", "$U1N"), ("$C3", "$U2"), ("$C4", "$U3"), ("$C5", "$L7"), ("$C6", "$L5"),
        ("$C7", "$E1"), ("$C0", "$L6"), ("$C1", "$A16_1"), ("$C2", "$A16_2"), ("$C3", "$A16_3"), ("$C4", "$L80"),
        ("$C5", "$H80"), ("$C6", "$Z3"), ("$C7", "$E3"), ("$C1", "$Z2"), ("$C2", "$Z1"), ("$C3", "$Z0"),
        # C8: dst terminator at byte 3 of an aligned dword (0x1007f7bb taken); C9: a 0x80 top byte before it (the
        # false candidate, 0x1007f7bb not taken)
        ("$C8", "$A16"), ("$C8", "$U2"), ("$C9", "$L7"), ("$C9", "$E")]
HOOKS["strcat"] = dict(_J, addr=0x0007F770, abi="default", ret="pointer", args=["pointer", "pointer"],
                       fixture="c3fm_jstr", state=[_STRBLK, ("$cblk", 0, 0x280)], vectors=_CAT)
_MEM = [("$M0", "$M100", 0x40), ("$M1", "$M100", 0x40), ("$M3", "$M101", 7), ("$M100", "$M0", 0), ("$M2", "$M203", 33),
        ("$M200", "$M201", 0x80), ("$M201", "$M200", 0x80), ("$M100", "$M80", 0x100), ("$M80", "$M100", 0x100),
        ("$M103", "$M100", 0x41), ("$M101", "$M103", 0x22), ("$M40", "$M40", 0x20), ("$M300", "$M0", 3),
        ("$M5", "$M300", 1), ("$M10", "$M11", 2), ("$M13", "$M10", 0x29), ("$M22", "$M20", 0x5),
        ("$M41", "$M180", 0xBF), ("$M4", "$M8", 0x1F), ("$M8", "$M4", 0x1F)]
# forward with dst = 2 / 3 mod 4 and >= 8 dwords after the alignment bytes (0x1007f454 / 0x1007f46e not taken); the
# downward path (dst inside (src, src + n)) with its last dword aligned and < 8 dwords (0x1007f551 taken), n < 4
# unaligned (0x1007f576 taken), and the 1 / 2 / 3 alignment-byte bodies with < 8 and >= 8 dwords (0x1007f5a8,
# 0x1007f5d2, 0x1007f600); every tail length 0..3 of each table.
_MEM += [("$M2", "$M100", 0x40), ("$M3", "$M100", 0x41), ("$M2", "$M100", 0x43), ("$M3", "$M100", 0x42)]
_MEM += [(d, "$M100", n) for d in ("$M101", "$M102", "$M103")
         for n in (1, 2, 3, 5, 6, 7, 8, 9, 10, 11, 0x40, 0x41, 0x42, 0x43)]
# aligned forward copies: every slot of the < 8 dword table 0x1007f47c and the tails after `rep movsd`
_MEM += [("$M0", "$M100", n) for n in (8, 12, 16, 20, 24, 29, 0x41, 0x42, 0x43, 0x22)]
HOOKS["memcpy"] = dict(_J, addr=0x0007F3A0, abi="default", ret="pointer", args=["pointer", "pointer", "uint32"],
                       fixture="c3fm_jmem", state=[("$mblk", 0, 0x400)], vectors=_MEM)
HOOKS["abs"] = dict(_J, addr=0x0007F370, abi="default", ret="int", args=["int"], fixture="c3fm_plain",
                    vectors=[(v,) for v in (0, 1, -1, 2, -2, 100, -100, 0x7FFFFFFF, -0x7FFFFFFF, -0x80000000, 12345,
                                            -65536)])
# _setjmp3 0x0007fab8: (jmp_buf, count, extra...). count 0 reads the SEH head's try level, count 1 [third arg + 0xc],
# count 2 stores the fourth argument, count >= 3 copies min(count - 2, 6) extra dwords.
_SJ = [("$J0", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0), ("$J1", 1, "$R0", 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18),
       ("$J2", 2, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29),
       ("$J3", 3, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39),
       ("$J4", 7, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49),
       ("$J5", 8, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59),
       ("$J6", 9, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69),
       ("$J7", -1, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79),
       ("$J0", 1, "$R1", 1, 2, 3, 4, 5, 6, 7, 8), ("$J1", 4, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9),
       ("$J2", 0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9)]
HOOKS["_setjmp3"] = dict(_J, addr=0x0007FAB8, abi="default", ret="int",
                         args=["pointer", "int", "pointer"] + ["int"] * 8, fixture="c3fm_sj3",
                         state=[("$jblk", 0, 0x300)], vectors=_SJ)
# ================================================================================ group D: Surface::dashedLine16
# (surface, x1, y1, x2, y2, c1, c2, len1, len2, phase). S0 clip {4, 3, 40, 28}; S5 the whole 48 x 32 buffer.
_DLST = [("$sblk", 0, 0x5800), ("$vlog", 0, VLOG), ("$fmlog", 0, FMLOG)]
_C = (5, 0x2A, 3, 2, 0)                                      # c1, c2, len1, len2, phase
_DL = [
    # dispatch: vertical (0x1000c188), horizontal (0x1000c1bd)
    ("$S0", 10, 5, 10, 20) + _C, ("$S0", 5, 7, 30, 7) + _C, ("$S4", 3, 3, 3, 3) + _C,
    # format 2 returns after the format read; lockBits 0 returns after the swap
    ("$S2", 6, 5, 30, 12) + _C, ("$S4", 6, 5, 30, 12) + _C, ("$S4", 30, 12, 6, 5) + _C,
    # down, dx >= dy: inside; x1 / y1 before the clip (clip-in loop both arms); x2 past right; y leaving bottom inside
    # the loop; nothing left after the clip-in; the four guards
    ("$S0", 6, 5, 30, 12) + _C, ("$S0", 0, 4, 20, 10) + _C, ("$S0", 6, 0, 30, 9) + _C, ("$S0", 10, 5, 47, 15) + _C,
    ("$S0", 5, 20, 39, 31) + _C, ("$S0", 0, 0, 200, 4) + _C, ("$S0", 41, 5, 45, 10) + _C, ("$S0", 5, 29, 20, 31) + _C,
    ("$S0", 0, 5, 3, 6) + _C, ("$S0", 5, 0, 20, 2) + _C, ("$S0", 4, 3, 39, 27) + _C,
    # down, dx < dy
    ("$S0", 10, 4, 15, 25) + _C, ("$S0", 10, 0, 14, 20) + _C, ("$S0", 0, 4, 6, 27) + _C, ("$S0", 10, 5, 14, 31) + _C,
    ("$S0", 36, 4, 45, 27) + _C, ("$S0", 39, -50, 45, 100) + _C, ("$S0", 39, 3, 40, 27) + _C,
    # up, dx >= dy
    ("$S0", 6, 20, 30, 10) + _C, ("$S0", 0, 20, 30, 12) + _C, ("$S0", 6, 31, 39, 20) + _C, ("$S0", 10, 20, 47, 10) + _C,
    ("$S0", 5, 10, 39, 0) + _C, ("$S0", 0, 28, 200, 27) + _C, ("$S0", 41, 20, 45, 15) + _C, ("$S0", 5, 2, 20, 1) + _C,
    ("$S0", 0, 20, 3, 18) + _C, ("$S0", 5, 31, 20, 29) + _C,
    # up, dx < dy
    ("$S0", 10, 25, 15, 4) + _C, ("$S0", 10, 31, 14, 10) + _C, ("$S0", 0, 27, 6, 4) + _C, ("$S0", 10, 20, 14, 0) + _C,
    ("$S0", 36, 27, 45, 4) + _C, ("$S0", 36, 60, 45, 0) + _C,
    # x2 < x1: the swap of endpoints, runs, colours and the phase
    ("$S0", 30, 12, 6, 5, 5, 0x2A, 4, 1, 2), ("$S0", 15, 25, 10, 4, 7, 9, 2, 5, 6), ("$S0", 30, 10, 6, 20, 1, 2, 1, 3, 1),
    ("$S0", 14, 4, 10, 25, 0x11, 0x22, 6, 2, 9),
    # the phase cursor: phase >= len1, < len1, negative (signed idiv), a multiple of the period, one-pixel runs, len2 0
    ("$S5", 1, 1, 46, 9, 5, 0x2A, 3, 2, 4), ("$S5", 1, 2, 46, 10, 5, 0x2A, 3, 2, 1), ("$S5", 1, 3, 46, 11, 5, 0x2A, 3, 2, -4),
    ("$S5", 1, 4, 46, 12, 5, 0x2A, 3, 2, 10), ("$S5", 1, 5, 46, 13, 5, 0x2A, 1, 1, 0), ("$S5", 1, 6, 46, 14, 5, 0x2A, 4, 0, 1),
    ("$S5", 1, 7, 46, 30, 5, 0x2A, 7, 3, -13), ("$S5", 2, 31, 9, 0, 5, 0x2A, 2, 6, 5),
    # colours: bit 31 set (no table lookup; -1 masks to 0xffff and is still written), low byte of a wider value, format 1
    ("$S5", 1, 8, 46, 16, -1, 0x2A, 3, 2, 0), ("$S5", 1, 9, 46, 17, 5, -0x7FFFEDCC, 3, 2, 0),
    ("$S5", 1, 10, 46, 18, 0x105, 0x12A, 3, 2, 0), ("$S1", 6, 5, 30, 12) + _C, ("$S1", 10, 25, 15, 4, 0xFF, 0x80, 2, 2, 1),
    # format 1 with both colours' bit 31 set (0x1000c29f / 0x1000c2d1 taken)
    ("$S1", 6, 5, 30, 12, -1, -0x7FFFEDCC, 3, 2, 0),
    # the clip-in loop carrying y past the clip while x is still left of it: down (0x1000c4b1, 0x1000c69f) and up
    # (0x1000c8e4, 0x1000cad5)
    ("$S0", -100, 20, 40, 60) + _C, ("$S0", 0, 0, 6, 100) + _C, ("$S0", -100, 20, 40, -20) + _C, ("$S0", 0, 40, 6, -60) + _C,
    # a run boundary at the first pixel, in a c1 run (phase 2 -> cursor -1) and in a c2 run (phase 4 -> cursor 1), in
    # each octant
    ("$S0", 6, 5, 30, 12, 5, 0x2A, 3, 2, 2), ("$S0", 6, 5, 30, 12, 5, 0x2A, 3, 2, 4),
    ("$S0", 10, 4, 15, 25, 5, 0x2A, 3, 2, 2), ("$S0", 10, 4, 15, 25, 5, 0x2A, 3, 2, 4),
    ("$S0", 6, 20, 30, 10, 5, 0x2A, 3, 2, 2), ("$S0", 6, 20, 30, 10, 5, 0x2A, 3, 2, 4),
    ("$S0", 10, 25, 15, 4, 5, 0x2A, 3, 2, 2), ("$S0", 10, 25, 15, 4, 5, 0x2A, 3, 2, 4),
    # a c2 run of 2 at the first pixel (phase 3 -> cursor 2: no reload) in the two octants where no other vector has it
    ("$S0", 10, 4, 15, 25, 5, 0x2A, 3, 2, 3), ("$S0", 6, 20, 30, 10, 5, 0x2A, 3, 2, 3),
]
HOOKS["Surface::dashedLine16"] = dict(_J, addr=0x0000C140, abi="thiscall", ret="void",
                                      args=["pointer"] + ["int"] * 9, fixture="c3fm_dl16", state=_DLST, vectors=_DL)
# S3 has no palette of its own: the module global 0x1012873c (G with +4 = P1 / G0 with +4 = 0 / NULL)
_DLG = [("$S3", 6, 5, 30, 12) + _C, ("$S3", 10, 25, 15, 4, 0xFF, 0x80, 2, 2, 1), ("$S3", 30, 12, 6, 5, 9, -1, 4, 1, 2)]
for _sfx in ("gpal", "gnullpal", "gnone"):
    HOOKS["Surface::dashedLine16_" + _sfx] = dict(_J, addr=0x0000C140, abi="thiscall", ret="void",
                                                  args=["pointer"] + ["int"] * 9, fixture="c3fm_dl16_" + _sfx,
                                                  state=_DLST, vectors=_DLG)

# the A/B thread's SEH head fs:[0] set to -1 by the fixture for this key (0x1007faea not taken: +0x1c = -1, return)
HOOKS["_setjmp3_nohead"] = dict(_J, addr=0x0007FAB8, abi="default", ret="int",
                                args=["pointer", "int", "pointer"] + ["int"] * 8, fixture="c3fm_sj3_nohead",
                                state=[("$jblk", 0, 0x300)], vectors=_SJ[:6])
