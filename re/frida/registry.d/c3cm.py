# Batch c3cm: sound.dll statically linked MSVC CRT (module-aware hooks; addr is the RVA, VA = 0x10000000 + RVA).
# Reimplementations in shim/src/re/c3cm.cpp, fixtures in re/frida/js/fixtures.d/c3cm.js.
# Every string, buffer and object a vector passes in is allocated by the fixture. Nothing here reaches the CRT
# heap, a lock, the per-thread data block, a file or the environment. Two entries are special and documented at
# their keys: cvtdate writes six module globals (all six are in its state region, so diff_hook restores them),
# and crtValidatePointer calls KERNEL32!IsBadWritePtr through the IAT slot the original calls.
#
# Vector counts: strchr 26, strstr 24, strcpy 16, doubleExponentField 20, iosResetVtable 12,
# crtValidatePointer 12, cvtdate 31. Every one of these functions has `callees none` in hooks.csv except strstr
# (0x10045686, the body of strchr six bytes past its entry) and strcpy (0x1004a081, the copy loop it shares with
# _strcat 0x1004a020), so all of them are given >= 10 vectors for the leaf rule.

_ALPHA = [("$s_alpha", c) for c in (0x61, 0x62, 0x63, 0x64, 0x65, 0x70, 0x7A, 0x00)]

HOOKS = {
    # 0x10045680: the byte-then-dword character scan. Vectors 1-8 run over one aligned 16-character string (the
    # four byte positions of a dword, a later dword, a miss and c == 0); 9-13 start the same string at offsets
    # 1, 2 and 3 so the alignment loop at 0x10045698 runs; 14 ends that loop on a zero byte (0x100456a1);
    # 15-16 are an empty string; 17 and 26 are the c false positive at 0x1004571e; 18 the zero false positive at
    # 0x100456f0; 19-22 the four "zero before c" exits of the candidate block (0x10045701, 0x10045709,
    # 0x10045714, 0x1004571c); 23 the plain zero-in-the-top-byte exit at 0x100456f2.
    "strchr": dict(module="sound.dll", addr=0x00045680, abi="default", ret="pointer", args=["pointer", "int"],
                   fixture="c3cm_chr",
                   vectors=_ALPHA + [("$s_alpha1", 0x62), ("$s_alpha1", 0x7A), ("$s_alpha2", 0x63),
                                     ("$s_alpha3", 0x64), ("$s_alpha3", 0x7A), ("$s_nul1", 0x61),
                                     ("$s_empty", 0x61), ("$s_empty", 0x00),
                                     ("$s_fp_c", 0x78), ("$s_fp_z", 0x7A),
                                     ("$s_z0", 0x71), ("$s_z1", 0x71), ("$s_z2", 0x71),
                                     ("$s_z3", 0x80), ("$s_z3", 0x71), ("$s_zz", 0x7A),
                                     ("$s_fp_c", 0x41), ("$s_fp_c", 0xF8)]),

    # 0x10042690: the substring search. Empty and one-character needles (the strchr tail jump at 0x100426fe),
    # first-character match at the start and inside the scan loop, a second-character mismatch (0x100426cf), the
    # two needle-exhausted exits (0x100426d9 for length 2, 0x100426e9 for length 3), the even-position mismatch
    # that restarts the outer loop (0x100426e2) and the odd-position one (0x100426f5).
    "strstr": dict(module="sound.dll", addr=0x00042690, abi="default", ret="pointer", args=["pointer", "pointer"],
                   fixture="c3cm_str",
                   vectors=[("$h_abcabcabd", "$n_empty"), ("$h_abcabcabd", "$n_a"), ("$h_empty", "$n_ab"),
                            ("$h_abcabcabd", "$n_ab"), ("$h_xx", "$n_ab"), ("$h_axbab", "$n_ab"),
                            ("$h_abcabcabd", "$n_abc"), ("$h_abcabcabd", "$n_abd"), ("$h_abcabcabd", "$n_zzz"),
                            ("$h_fox", "$n_lazydog"), ("$h_fox", "$n_dog"), ("$h_abcde", "$n_abcde"),
                            ("$h_abcdx", "$n_abcd"), ("$h_abcd", "$n_abcde"), ("$h_aaab", "$n_ab"),
                            ("$h_aaab", "$n_aab"), ("$h_abcabd", "$n_abd"), ("$h_abcabcabd", "$n_abcabd"),
                            ("$h_empty", "$n_empty"), ("$h_abcabcabd", "$n_c"), ("$h_empty", "$n_a"),
                            ("$h_fox", "$n_zzz"), ("$h_abcxe", "$n_abcde"), ("$h_xx", "$n_abcdef")]),

    # 0x1004a010: the copy. Sources of length 0, 4, 5, 6, 7, 15 and 20 cover the four terminal stores
    # (0x1004a0f8, 0x1004a0ef, 0x1004a0e2, 0x1004a0da) and the dword store at 0x1004a0a1; sources at odd
    # addresses run the byte prologue (0x1004a08d) once, three times and with an immediate zero (0x1004a092);
    # s_fp holds a 0x80 byte in the top position of its first dword, the false positive at 0x1004a0d8.
    # The four destinations share one 0x100-byte block, which is the state region, so every byte the copy writes
    # (and every byte after the terminator it must leave alone) is compared.
    "strcpy": dict(module="sound.dll", addr=0x0004A010, abi="default", ret="pointer", args=["pointer", "pointer"],
                   fixture="c3cm_cpy", state=[("$dstmem", 0, 0x100)],
                   vectors=[("$d0", "$c_15"), ("$d1", "$c_15"), ("$d0", "$c_4"), ("$d0", "$c_5"),
                            ("$d0", "$c_6"), ("$d0", "$c_7"), ("$d0", "$c_0"), ("$d1", "$c_u1"),
                            ("$d2", "$c_u1nul"), ("$d2", "$c_u3"), ("$d3", "$c_fp"), ("$d3", "$c_long"),
                            ("$d1", "$c_4"), ("$d2", "$c_5"), ("$d3", "$c_6"), ("$d2", "$c_long")]),

    # 0x10048268: the two stack dwords are the halves of a double; only the second is read. Ten high halves have
    # an exponent field of 0x7ff (the je at 0x10048276 returns the raw dword) and ten do not (the masked value).
    "doubleExponentField": dict(module="sound.dll", addr=0x00048268, abi="default", ret="uint32",
                                args=["uint32", "uint32"],
                                vectors=[(lo, hi) for lo, hi in
                                         ((0x00000000, 0x7FF00000), (0x00000001, 0x7FF00000),
                                          (0x00000000, 0x7FF80000), (0xFFFFFFFF, 0x7FFFFFFF),
                                          (0x00000000, 0xFFF00000), (0x12345678, 0xFFF12345),
                                          (0xAAAAAAAA, 0xFFFFFFFF), (0x00000000, 0x7FF00001),
                                          (0x00000000, 0xFFF80000), (0x00000000, 0x7FFFFFFF),
                                          (0x00000000, 0x00000000), (0x00000000, 0x80000000),
                                          (0x00000000, 0x3FF00000), (0x00000000, 0x40000000),
                                          (0x00000000, 0xC0590000), (0x00000000, 0x000FFFFF),
                                          (0x00000000, 0x7FEFFFFF), (0x00000000, 0x00100000),
                                          (0x9ABCDEF0, 0x12345678), (0x00000000, 0x7FE00000))]),

    # 0x10041aad: the virtual-base vtable store. Twelve objects whose virtual-base table holds a different offset
    # (0, 4, 8, 0xc, 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x80, 0xa0), so the stored address differs per vector;
    # the whole 0x100-byte block of each object is in the state region, including the bytes the store must not
    # touch. The return value is the offset eax carries out of 0x10041ab0.
    "iosResetVtable": dict(module="sound.dll", addr=0x00041AAD, abi="thiscall", ret="uint32", args=["pointer"],
                           fixture="c3cm_ios", state=[(f"$b{i}", 0, 0x100) for i in range(12)],
                           vectors=[(f"$o{i}",) for i in range(12)]),

    # 0x10049604: eight vectors pass a block the fixture allocated (the import reports it writable, so the
    # function returns 1) and four pass a null pointer, which IsBadWritePtr rejects without probing memory (any
    # other invalid address makes the import fault on purpose, and Frida's vectored exception handler takes that
    # access violation before the import's own handler does). The zero-length vector is the documented
    # IsBadWritePtr special case. No state: the function writes nothing.
    "crtValidatePointer": dict(module="sound.dll", addr=0x00049604, abi="default", ret="int",
                               args=["pointer", "uint32"], fixture="c3cm_valid",
                               vectors=[("$buf", 0), ("$buf", 1), ("$buf", 2), ("$buf", 4), ("$buf", 0x10),
                                        ("$buf", 0x100), ("$buf2", 1), ("$buf2", 0x20),
                                        (0, 1), (0, 4), (0, 0x10), (0, 0x1000)]),

    # 0x10051319: eleven integer arguments, no return value (`ret` at 0x10051458 leaves whatever eax held, which
    # is why ret is "void" and the evidence is the memory comparison). The six dwords it writes lie in
    # 0x10068900..0x1006891b and are the whole state region; diff_hook restores them after each arm, so the
    # running CRT sees them unchanged. The vectors cover both arms of every branch: datetype 1 and 0
    # (0x10051325), year with and without the low two bits set (0x10051337, 0x100513ba, 0x1005139b), week 5 and
    # not (0x10051395) with the result both past and before the next table entry (0x100513ad), the seven
    # weekdays against the computed remainder (0x10051377), trantype 1 and 0 (0x100513d3), and a millisecond
    # count that is negative (0x10051429) and one that reaches a full day (0x1005143f).
    "cvtdate": dict(module="sound.dll", addr=0x00051319, abi="default", ret="void",
                    args=["int"] * 11, fixture="c3cm_cvtdate", state=[("$dst", 0, 0x1C)],
                    vectors=[(1, 1, 100, 3, 1, 0, 0, 2, 0, 0, 0),
                             (1, 1, 100, 3, 5, 0, 0, 2, 0, 0, 0),
                             (1, 1, 101, 9, 5, 6, 0, 2, 0, 0, 0),
                             (1, 1, 102, 12, 2, 3, 0, 1, 30, 0, 0),
                             (1, 1, 103, 11, 5, 1, 0, 23, 59, 59, 999),
                             (0, 1, 100, 3, 1, 0, 0, 2, 0, 0, 0),
                             (0, 1, 100, 3, 5, 5, 0, 2, 0, 0, 0),
                             (0, 1, 101, 6, 3, 2, 0, 0, 0, -200000, 0),
                             (0, 1, 102, 6, 3, 2, 0, 48, 0, 0, 0),
                             (0, 1, 103, 2, 4, 4, 0, 12, 0, 0, 500),
                             (1, 0, 100, 2, 0, 0, 15, 3, 0, 0, 0),
                             (1, 0, 101, 2, 0, 0, 15, 3, 0, 0, 0),
                             (0, 0, 100, 11, 0, 0, 31, 2, 0, 0, 0),
                             (0, 0, 101, 12, 0, 0, 1, 0, 0, -86400, 0),
                             (0, 0, 102, 5, 0, 0, 10, 48, 0, 0, 0),
                             (1, 2, 103, 7, 0, 0, 20, 6, 30, 15, 250),
                             (0, 0, 100, 1, 0, 0, 1, 0, 0, 0, 0),
                             (0, 0, 103, 4, 0, 0, 29, 23, 59, 59, 999),
                             (1, 1, 100, 1, 5, 0, 0, 2, 0, 0, 0),
                             (1, 1, 100, 1, 5, 1, 0, 2, 0, 0, 0),
                             (1, 1, 100, 2, 5, 2, 0, 2, 0, 0, 0),
                             (1, 1, 100, 4, 5, 3, 0, 2, 0, 0, 0),
                             (1, 1, 101, 10, 5, 4, 0, 2, 0, 0, 0),
                             (1, 1, 102, 8, 5, 5, 0, 2, 0, 0, 0)]
                            + [(1, 1, 100, 6, 2, dw, 0, 2, 0, 0, 0) for dw in range(7)]),
}
