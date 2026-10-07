# Batch c3ao: fourteen jgld.dll functions (module-aware hooks; addr is the RVA, VA = 0x10000000 + addr).
# Every block the vectors touch is allocated by the fixtures (re/frida/js/fixtures.d/c3ao.js); nothing live in the
# running jgld.dll (Display, Surface, Palette, Font, the game's png_struct) is passed in or written.
# Reimplementations and the instruction citations: shim/src/re/c3ao.cpp.

# The four row transformations share the shape (row_info, row): twelve headers each, covering both sides of every
# guard and a zero-length loop.
_ROWS = 12

HOOKS = {
    # --- libpng row transformations (__cdecl, leaves: 12 vectors each). ------------------------------------
    # invert writes only the row buffer; the header is read-only, so one state region is enough.
    "png_do_invert": dict(module="jgld.dll", addr=0x0006D900, abi="default", ret="void",
                          args=["pointer", "pointer"], fixture="c3ao_rows",
                          state=[("$invrow", 0, 64)],
                          vectors=[(f"$inv{i}", "$invrow") for i in range(_ROWS)]),
    "png_do_swap": dict(module="jgld.dll", addr=0x0006D980, abi="default", ret="void",
                        args=["pointer", "pointer"], fixture="c3ao_rows",
                        state=[("$swprow", 0, 64)],
                        vectors=[(f"$swp{i}", "$swprow") for i in range(_ROWS)]),
    "png_do_packswap": dict(module="jgld.dll", addr=0x0006DA10, abi="default", ret="void",
                            args=["pointer", "pointer"], fixture="c3ao_rows",
                            state=[("$pksrow", 0, 64)],
                            vectors=[(f"$pks{i}", "$pksrow") for i in range(_ROWS)]),
    # chop rewrites bit_depth, pixel_depth and rowbytes of the header it is given, so the whole header block is
    # part of its state (12 headers of 0xc bytes from $chp0) as well as the row.
    "png_do_chop": dict(module="jgld.dll", addr=0x00072430, abi="default", ret="void",
                        args=["pointer", "pointer"], fixture="c3ao_rows",
                        state=[("$chp0", 0, _ROWS * 0xC), ("$chprow", 0, 64)],
                        vectors=[(f"$chp{i}", "$chprow") for i in range(_ROWS)]),

    # --- jgld surface helper (__cdecl, leaf). n odd / even / zero / negative; v with bits above 16 set, which
    #     the body masks away. The largest n writes 80 of the 160 bytes of $fw. ---------------------------------
    "fillWords": dict(module="jgld.dll", addr=0x0000AF30, abi="default", ret="void",
                      args=["pointer", "uint32", "int"], fixture="c3ao_fill",
                      state=[("$fw", 0, 160)],
                      vectors=[("$fw", v, n) for v, n in
                               ((0x0000, 0), (0x1234, 1), (0xABCD, 2), (0xFFFF, 3), (0x0001, 4), (0x8000, 7),
                                (0x5A5A, 8), (0x0001, 15), (0xDEAD, 16), (0x0F0F, 32), (0x7FFF, 40),
                                (0x1111, -1), (0x2222, -2), (0x3333, -3), (0xFFFF0042, 5))]),

    # --- Palette::toRGBQuads (__thiscall, leaf): 12 palettes, one 1 KB destination. ---------------------------
    "Palette::toRGBQuads": dict(module="jgld.dll", addr=0x0006AE50, abi="thiscall", ret="void",
                                args=["pointer", "pointer"], fixture="c3ao_pal",
                                state=[("$quads", 0, 0x400)],
                                vectors=[(f"$pal{i}", "$quads") for i in range(12)]),

    # --- png_sig_cmp (__cdecl, read-only): both sides of the four guards (num > 8, num < 1, start > 7,
    #     start + num > 8) and signatures that match, differ in the first byte, in the last byte and everywhere. -
    "png_sig_cmp": dict(module="jgld.dll", addr=0x000785F0, abi="default", ret="int",
                        args=["pointer", "uint32", "uint32"], fixture="c3ao_sig",
                        vectors=[("$sig_ok", 0, 8), ("$sig_ok", 0, 1), ("$sig_ok", 7, 1), ("$sig_ok", 0, 9),
                                 ("$sig_ok", 0, 0), ("$sig_ok", 8, 1), ("$sig_ok", 0xFFFFFFFF, 1),
                                 ("$sig_ok", 5, 8), ("$sig_ok", 0, 0x7FFFFFFF), ("$sig_ok", 7, 2),
                                 ("$sig_bad0", 0, 8), ("$sig_bad0", 1, 7), ("$sig_bad7", 0, 8),
                                 ("$sig_bad7", 0, 7), ("$sig_zero", 0, 8), ("$sig_zero", 4, 4),
                                 ("$sig_zero", 7, 1), ("$sig_ff", 0, 1), ("$sig_ff", 7, 2), ("$sig_ff", 3, 0)]),

    # --- png_calculate_crc (__cdecl): the 12 scratch png_structs cover ancillary/critical, both flag tests and
    #     both sides of the final need_crc test; the lengths vary the folded buffer. State: the block of structs
    #     ($crc0), whose running CRCs at +0x110 are the only field the function writes. ------------------------
    "png_calculate_crc": dict(module="jgld.dll", addr=0x000787D0, abi="default", ret="void",
                              args=["pointer", "pointer", "uint32"], fixture="c3ao_crc",
                              state=[("$crc0", 0, 12 * 0x140)],
                              vectors=[(f"$crc{i}", "$cdata", n) for i, n in enumerate(
                                  (16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16))]
                                      + [("$crc1", "$cdata", n) for n in (0, 1, 2, 7, 63, 64)]
                                      + [("$crc6", "$cdata", n) for n in (0, 1, 32, 64)]),

    # --- png_set_bKGD (__cdecl): three null combinations take the guard, eight sources exercise the copy. ------
    "png_set_bKGD": dict(module="jgld.dll", addr=0x0007DAD0, abi="default", ret="void",
                         args=["pointer", "pointer", "pointer"], fixture="c3ao_set",
                         state=[("$infoB", 0, 0x100)],
                         vectors=[("$pngB", "$infoB", f"$bk{i}") for i in range(8)]
                                 + [(0, "$infoB", "$bk0"), ("$pngB", 0, "$bk0"), (0, 0, "$bk0"),
                                    ("$pngB", "$infoB", "$bk3")]),

    # --- png_set_tIME (__cdecl): null png, null info, the 0x10000 mode bit set and clear (alone and inside a
    #     wider mask), six sources. ------------------------------------------------------------------------
    "png_set_tIME": dict(module="jgld.dll", addr=0x0007E400, abi="default", ret="void",
                         args=["pointer", "pointer", "pointer"], fixture="c3ao_set",
                         state=[("$infoT", 0, 0x100)],
                         vectors=[("$pngT0", "$infoT", f"$tm{i}") for i in range(6)]
                                 + [("$pngT3", "$infoT", "$tm1"), ("$pngT1", "$infoT", "$tm0"),
                                    ("$pngT2", "$infoT", "$tm2"), (0, "$infoT", "$tm0"),
                                    ("$pngT0", 0, "$tm0"), (0, 0, "$tm0")]),

    # --- png_set_tRNS (__cdecl): both sides of the two null guards, of `trans == 0`, of `trans_values == 0` and
    #     of the `num_trans == 0 -> 1` fix-up that only the trans_values path reaches. ------------------------
    "png_set_tRNS": dict(module="jgld.dll", addr=0x0007E470, abi="default", ret="void",
                         args=["pointer", "pointer", "pointer", "int", "pointer"], fixture="c3ao_set",
                         state=[("$infoR", 0, 0x100)],
                         vectors=[("$pngB", "$infoR", "$tr", 4, "$tv0"),
                                  ("$pngB", "$infoR", 0, 4, "$tv0"),
                                  ("$pngB", "$infoR", "$tr", 0, "$tv0"),
                                  ("$pngB", "$infoR", "$tr", 0, 0),
                                  ("$pngB", "$infoR", 0, 0, 0),
                                  ("$pngB", "$infoR", "$tr2", 256, "$tv1"),
                                  ("$pngB", "$infoR", "$tr", 0x1FFFF, "$tv2"),
                                  ("$pngB", "$infoR", "$tr2", -1, 0),
                                  ("$pngB", "$infoR", "$tr", 1, "$tv3"),
                                  ("$pngB", "$infoR", 0, 5, 0),
                                  (0, "$infoR", "$tr", 4, "$tv0"),
                                  ("$pngB", 0, "$tr", 4, "$tv0"),
                                  (0, 0, "$tr", 4, "$tv0")]),

    # --- The three unguarded stores (__cdecl, leaves: 12 vectors each, one scratch png_struct per vector inside
    #     one block, so the single state region shows which object changed). ---------------------------------
    "png_set_strip_alpha": dict(module="jgld.dll", addr=0x0006F360, abi="default", ret="void",
                                args=["pointer"], fixture="c3ao_simple",
                                state=[("$sa0", 0, 12 * 0x80)],
                                vectors=[(f"$sa{i}",) for i in range(12)]),
    "png_set_error_fn": dict(module="jgld.dll", addr=0x00079340, abi="default", ret="void",
                             args=["pointer", "pointer", "pointer", "pointer"], fixture="c3ao_simple",
                             state=[("$ef0", 0, 12 * 0x80)],
                             vectors=[(f"$ef{i}", p, e, w) for i, (p, e, w) in enumerate(
                                 ((0, 0, 0), (1, 2, 3), (0x7FFFFFFF, 0x80000000, 0xFFFFFFFF),
                                  (0x11111111, 0, 0x22222222), (0, 0x33333333, 0),
                                  (0xDEADBEEF, 0xCAFEBABE, 0x12345678), (4, 0, 0), (0, 0, 5),
                                  (0xAAAAAAAA, 0xAAAAAAAA, 0xAAAAAAAA), (0x55555555, 0x55555555, 0x55555555),
                                  (0x100, 0x200, 0x300), (0xFFFF0000, 0x0000FFFF, 0xF0F0F0F0)))]),
    "png_init_io": dict(module="jgld.dll", addr=0x00078AD0, abi="default", ret="void",
                        args=["pointer", "pointer"], fixture="c3ao_simple",
                        state=[("$io0", 0, 12 * 0x80)],
                        vectors=[(f"$io{i}", v) for i, v in enumerate(
                            (0, 1, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, 0x10000000, 0x400000, 0x2C,
                             0xDEAD0000, 0x55555555, 0xAAAAAAAA, 0x1234))]),
}
