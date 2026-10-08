# Batch c3bg: nine jgld.dll non-render functions (module-aware hooks; addr is the RVA,
# VA = 0x10000000 + addr). Every block the vectors touch is allocated by the fixtures
# (re/frida/js/fixtures.d/c3bg.js); nothing live in the running jgld.dll (Display, Surface, Palette,
# Font, the game's png_struct / png_info, its inflate state or its heap blocks) is passed in.
# Reimplementations and the instruction citations: shim/src/re/c3bg.cpp; branch tables:
# log/c3/c3bg_purpose.md.
#
# Leaf rule: png_do_strip_filler, png_do_read_filler, png_do_gamma, png_do_expand,
# png_do_rgb_to_gray and inflate_fast have no callee at all (hooks.csv callees = none), so each
# carries 16 vectors or more. png_do_read_interlace and png_combine_row call the CRT memcpy
# 0x1007f3a0 and the debug stack check __chkesp 0x1007e780; png_set_sRGB_gAMA_and_cHRM calls
# png_set_cHRM, png_set_gAMA and png_set_sRGB.
#
# Hooked callees to switch off when re-running: png_set_sRGB_gAMA_and_cHRM only. Its three callees
# png_set_cHRM 0x0007db30, png_set_gAMA 0x0007dbd0 (both hooked by batch c3at) and png_set_sRGB
# 0x0007e0d0 (hooked by c3al) are all at C3 already, so that key is re-run with
# SIMGOLF_HOOKS_OFF=jgld.dll:7db30,jgld.dll:7dbd0,jgld.dll:7e0d0. No other function of this batch
# calls anything that carries an SG_HOOK: the rest reach only the CRT memcpy 0x1007f3a0, __chkesp
# 0x1007e780 and the read-only module tables png_pass_inc 0x10123244 and inflate_mask 0x101274a4.

# One state region per fixture block. Every read-only input the bodies index (the gamma tables, the
# transparency values, the Huffman trees) is its own region, which is how "the body does not write
# them" is tested; the pattern-filled blocks around each object catch a store one byte too wide.
_SF = [("$sfblk", 0, 16 * 0xC), ("$sfrowblk", 0, 0x200)]
_RF = [("$rfblk", 0, 16 * 0xC), ("$rfrowblk", 0, 0x400)]
_GM = [("$gmblk", 0, 18 * 0xC), ("$gmrowblk", 0, 0x100), ("$gmtab", 0, 256),
       ("$gm16", 0, 256 * 4), ("$gm16sub", 0, 8 * 512)]
_EX = [("$exblk", 0, 16 * 0xC), ("$exrowblk", 0, 0x200), ("$extvblk", 0, 8 * 0x10)]
_GR = [("$ghblk", 0, 16 * 0xC), ("$growblk", 0, 0x100), ("$gblk", 0, 7 * 0x280),
       ("$gt1", 0, 256), ("$gf1", 0, 256), ("$g16to", 0, 1024), ("$g16from", 0, 1024),
       ("$g16tosub", 0, 8 * 512), ("$g16fromsub", 0, 8 * 512)]
_IL = [("$ihblk", 0, 16 * 0xC), ("$irow", 0, 0x100)]
_CR = [("$cblk", 0, 13 * 0x140), ("$crbuf", 0, 0x80), ("$crow", 0, 0x80)]
_SG = [("$sblk", 0, 12 * 0x100), ("$spng", 0, 0x100)]
_ZL = [("$tblk", 0, 0x400), ("$zsblk", 0, 16 * 0x80), ("$zwblk", 0, 16 * (512 + 0x80))]

HOOKS = {
    # ---------------------------------------------------------------- libpng row transformations.
    # png_do_strip_filler: the two channel counts, the two depth halves and the two sides of the
    # 0x80 flag are eight bodies; vector 8 takes neither half, 9 to 13 run a loop zero times, 14
    # proves the depth test is "8 or anything else" and 15 that only bit 0x80 of the flag word is
    # read.
    "png_do_strip_filler": dict(module="jgld.dll", addr=0x0006DAC0, abi="default", ret="void",
                                args=["pointer", "pointer", "uint"], fixture="c3bg_rows", state=_SF,
                                vectors=[("$sf0", "$sfrow", 0x80), ("$sf1", "$sfrow", 0),
                                         ("$sf2", "$sfrow", 0x80), ("$sf3", "$sfrow", 0),
                                         ("$sf4", "$sfrow", 0x80), ("$sf5", "$sfrow", 0),
                                         ("$sf6", "$sfrow", 0x80), ("$sf7", "$sfrow", 0),
                                         ("$sf8", "$sfrow", 0x80), ("$sf9", "$sfrow", 0x80),
                                         ("$sf10", "$sfrow", 0x80), ("$sf11", "$sfrow", 0),
                                         ("$sf12", "$sfrow", 0), ("$sf13", "$sfrow", 0x80),
                                         ("$sf14", "$sfrow", 0x80), ("$sf15", "$sfrow", 0x7F)]),
    # png_do_read_filler: the same eight-body shape with the filler word split into two bytes; the
    # filler differs per vector so the inserted bytes differ too.
    "png_do_read_filler": dict(module="jgld.dll", addr=0x00072BD0, abi="default", ret="void",
                               args=["pointer", "pointer", "uint", "uint"], fixture="c3bg_rows",
                               state=_RF,
                               vectors=[("$rf0", "$rfrow", 0x1234, 0x80), ("$rf1", "$rfrow", 0x1234, 0),
                                        ("$rf2", "$rfrow", 0xABCD, 0x80), ("$rf3", "$rfrow", 0xABCD, 0),
                                        ("$rf4", "$rfrow", 0x00FF, 0x80), ("$rf5", "$rfrow", 0x00FF, 0),
                                        ("$rf6", "$rfrow", 0xFF00, 0x80), ("$rf7", "$rfrow", 0xFF00, 0),
                                        ("$rf8", "$rfrow", 0x1111, 0x80), ("$rf9", "$rfrow", 0x2222, 0x80),
                                        ("$rf10", "$rfrow", 0x3333, 0), ("$rf11", "$rfrow", 0x4455, 0x80),
                                        ("$rf12", "$rfrow", 0x6677, 0x80), ("$rf13", "$rfrow", 0x8899, 0),
                                        ("$rf14", "$rfrow", 0xAA00, 0), ("$rf15", "$rfrow", 0x1234, 0x7F)]),
    # png_do_gamma: the colour-type jump table (0, 2, 4, 6 have a body; 1, 3 and 5 jump to the exit
    # and 7 fails the range test), the four grey depths, and the two halves of the guard with each
    # table in turn set to NULL.
    "png_do_gamma": dict(module="jgld.dll", addr=0x00076760, abi="default", ret="void",
                         args=["pointer", "pointer", "pointer", "pointer", "int"],
                         fixture="c3bg_rows", state=_GM,
                         vectors=[("$gm0", "$gmrow", "$gmtab", "$gm16", 0),
                                  ("$gm1", "$gmrow", "$gmtab", "$gm16", 4),
                                  ("$gm2", "$gmrow", "$gmtab", "$gm16", 0),
                                  ("$gm3", "$gmrow", "$gmtab", "$gm16", 8),
                                  ("$gm4", "$gmrow", "$gmtab", "$gm16", 0),
                                  ("$gm5", "$gmrow", "$gmtab", "$gm16", 0),
                                  ("$gm6", "$gmrow", "$gmtab", "$gm16", 0),
                                  ("$gm7", "$gmrow", "$gmtab", "$gm16", 0),
                                  ("$gm8", "$gmrow", "$gmtab", "$gm16", 0),
                                  ("$gm9", "$gmrow", "$gmtab", "$gm16", 2),
                                  ("$gm10", "$gmrow", "$gmtab", "$gm16", 0),
                                  ("$gm11", "$gmrow", "$gmtab", "$gm16", 0),
                                  ("$gm12", "$gmrow", "$gmtab", "$gm16", 0),
                                  ("$gm13", "$gmrow", "$gmtab", "$gm16", 0),
                                  ("$gm14", "$gmrow", 0, "$gm16", 0),
                                  ("$gm15", "$gmrow", "$gmtab", 0, 4),
                                  ("$gm16", "$gmrow", 0, "$gm16", 1),
                                  ("$gm17", "$gmrow", "$gmtab", "$gm16", 0)]),
    # png_do_expand: the three sub-8 grey depths, the 8- and 16-bit tRNS stages of the grey arm and
    # the two of the RGB arm, each with a trans_value whose components the row does and does not
    # match, plus the three colour types that run nothing.
    "png_do_expand": dict(module="jgld.dll", addr=0x00077300, abi="default", ret="void",
                          args=["pointer", "pointer", "pointer"], fixture="c3bg_rows", state=_EX,
                          vectors=[("$ex0", "$exrow", "$tv0"), ("$ex1", "$exrow", "$tv1"),
                                   ("$ex2", "$exrow", "$tv2"), ("$ex3", "$exrow", "$tv3"),
                                   ("$ex4", "$exrow", "$tv3"), ("$ex5", "$exrow", "$tv4"),
                                   ("$ex6", "$exrow", 0), ("$ex7", "$exrow", 0),
                                   ("$ex8", "$exrow", "$tv3"), ("$ex9", "$exrow", "$tv0"),
                                   ("$ex10", "$exrow", "$tv5"), ("$ex11", "$exrow", 0),
                                   ("$ex12", "$exrow", "$tv0"), ("$ex13", "$exrow", "$tv0"),
                                   ("$ex14", "$exrow", "$tv0"), ("$ex15", "$exrow", "$tv5")]),
    # png_do_rgb_to_gray: eight bodies (colour type 2 or 6, depth 8 or 16, with or without the pair
    # of gamma tables of that depth), the colour-mask guard, a colour type that is neither 2 nor 6,
    # an empty row and the three "only one of the pair is set" records.
    "png_do_rgb_to_gray": dict(module="jgld.dll", addr=0x000736C0, abi="default", ret="int",
                               args=["pointer", "pointer", "pointer"], fixture="c3bg_gray", state=_GR,
                               vectors=[("$ps_nog", "$gh0", "$grow8"), ("$ps_g8", "$gh1", "$grow8"),
                                        ("$ps_nog", "$gh2", "$grow16"), ("$ps_g16", "$gh3", "$grow16"),
                                        ("$ps_nog", "$gh4", "$grow8"), ("$ps_g8", "$gh5", "$grow8"),
                                        ("$ps_nog", "$gh6", "$grow16"), ("$ps_g16", "$gh7", "$grow16"),
                                        ("$ps_nog", "$gh8", "$grow8"), ("$ps_nog", "$gh9", "$grow8"),
                                        ("$ps_nog", "$gh10", "$grow8"), ("$ps_half8", "$gh11", "$grow8"),
                                        ("$ps_half16", "$gh12", "$grow16"),
                                        ("$ps_half16b", "$gh13", "$grow16"),
                                        ("$ps_both", "$gh14", "$grow8"), ("$ps_both", "$gh15", "$grow16")]),
    # png_do_read_interlace: the three packed depths with both sides of the PACKSWAP flag, the
    # default body at four pixel sizes, every repeat count of png_pass_inc, an empty row and the two
    # null guards.
    "png_do_read_interlace": dict(module="jgld.dll", addr=0x0007C810, abi="default", ret="void",
                                  args=["pointer", "pointer", "int", "uint"], fixture="c3bg_png",
                                  state=_IL,
                                  vectors=[("$ih0", "$irow", 0, 0), ("$ih1", "$irow", 0, 0x10000),
                                           ("$ih2", "$irow", 2, 0), ("$ih3", "$irow", 2, 0x10000),
                                           ("$ih4", "$irow", 4, 0), ("$ih5", "$irow", 4, 0x10000),
                                           ("$ih6", "$irow", 1, 0), ("$ih7", "$irow", 3, 0),
                                           ("$ih8", "$irow", 5, 0x10000), ("$ih9", "$irow", 6, 0),
                                           ("$ih10", "$irow", 6, 0), ("$ih11", "$irow", 0, 0),
                                           ("$ih12", 0, 0, 0), (0, "$irow", 0, 0),
                                           ("$ih14", "$irow", 1, 0x10000), ("$ih15", "$irow", 0, 0x10000)]),
    # png_combine_row: the whole-row memcpy that mask 0xff takes, the three packed depths with both
    # sides of the PACKSWAP flag, the default body at four pixel sizes, a mask that selects nothing,
    # an empty row, a width of 16 so the packed bodies' bit walker wraps from 1 back to 0x80 and a
    # width of 10 so the default body's own walker wraps too.
    "png_combine_row": dict(module="jgld.dll", addr=0x0007C310, abi="default", ret="void",
                            args=["pointer", "pointer", "int"], fixture="c3bg_png", state=_CR,
                            vectors=[("$c0", "$crow", 0xFF), ("$c0", "$crow", 0x80),
                                     ("$c0", "$crow", 0x55), ("$c1", "$crow", 0xAA),
                                     ("$c2", "$crow", 0x80), ("$c3", "$crow", 0x55),
                                     ("$c4", "$crow", 0xF0), ("$c5", "$crow", 0x0F),
                                     ("$c6", "$crow", 0x55), ("$c6", "$crow", 0x00),
                                     ("$c7", "$crow", 0xAA), ("$c8", "$crow", 0x81),
                                     ("$c9", "$crow", 0xC0), ("$c10", "$crow", 0x55),
                                     ("$c11", "$crow", 0x81), ("$c9", "$crow", 0xFF),
                                     ("$c12", "$crow", 0x81)]),

    # ------------------------------------------------------------------- libpng's sRGB convenience
    # setter. Eleven png_info records with different starting `valid` words and eleven intents, then
    # the two null guards. Every write goes to the png_info; the png_struct is only tested.
    "png_set_sRGB_gAMA_and_cHRM": dict(module="jgld.dll", addr=0x0007E120, abi="default", ret="void",
                                       args=["pointer", "pointer", "int"], fixture="c3bg_set",
                                       state=_SG,
                                       vectors=[("$spng", "$si0", 0), ("$spng", "$si1", 1),
                                                ("$spng", "$si2", 2), ("$spng", "$si3", 3),
                                                ("$spng", "$si4", -1), ("$spng", "$si5", 0x7F),
                                                ("$spng", "$si6", 0x100), ("$spng", "$si7", 0x55),
                                                ("$spng", "$si8", 0), ("$spng", "$si9", 0x81),
                                                (0, "$si10", 1), ("$spng", 0, 1)]),

    # ------------------------------------------------------------------------- zlib's fast inflate.
    # Sixteen independent (state, stream, window, input) sets with Huffman trees built by the
    # fixture: a literal, a length with a direct copy, a copy that starts past the write pointer
    # with and without crossing the end of the window, both second levels, the end of block, both
    # error exits, the two later refill loops, a pre-filled bit buffer, a second pass of the outer
    # loop, and the branch where the write pointer is at or after the read pointer.
    "inflate_fast": dict(module="jgld.dll", addr=0x000A0340, abi="default", ret="int",
                         args=["uint", "uint", "pointer", "pointer", "pointer", "pointer"],
                         fixture="c3bg_zlib", state=_ZL,
                         vectors=[(2, 1, "$ztlA", "$ztdA", "$zs0", "$zz0"),
                                  (2, 1, "$ztlA", "$ztdA", "$zs1", "$zz1"),
                                  (2, 1, "$ztlA", "$ztdA", "$zs2", "$zz2"),
                                  (2, 1, "$ztlC", "$ztdA", "$zs3", "$zz3"),
                                  (2, 1, "$ztlA", "$ztdA", "$zs4", "$zz4"),
                                  (2, 1, "$ztlA", "$ztdA", "$zs5", "$zz5"),
                                  (2, 1, "$ztlA", "$ztdA", "$zs6", "$zz6"),
                                  (2, 1, "$ztlB", "$ztdA", "$zs7", "$zz7"),
                                  (2, 1, "$ztlA", "$ztdB", "$zs8", "$zz8"),
                                  (2, 1, "$ztlA", "$ztdC", "$zs9", "$zz9"),
                                  (2, 1, "$ztlD", "$ztdD", "$zs10", "$zz10"),
                                  (2, 1, "$ztlA", "$ztdA", "$zs11", "$zz11"),
                                  (2, 1, "$ztlA", "$ztdA", "$zs12", "$zz12"),
                                  (2, 1, "$ztlA", "$ztdA", "$zs13", "$zz13"),
                                  (2, 1, "$ztlA", "$ztdA", "$zs14", "$zz14"),
                                  (2, 1, "$ztlD", "$ztdA", "$zs15", "$zz15")]),
    # inflate_codes: one vector per mode of the state machine plus the branches inside the three that
    # have them. Each vector has its own inflate_codes_state, inflate_blocks_state, z_stream, window,
    # input and output buffer; the blocks state's check function is null so inflate_flush calls
    # nothing. The third argument is the caller's status, which vector 16 (the immediate leave) is
    # the only one to carry out of the body unchanged.
    "inflate_codes": dict(module="jgld.dll", addr=0x0009F370, abi="default", ret="int",
                          args=["pointer", "pointer", "int"], fixture="c3bg_codes",
                          state=[("$ctblk", 0, 0x200), ("$ccblk", 0, 18 * 0x40),
                                 ("$csblk", 0, 18 * 0x80), ("$cwblk", 0, 18 * (512 + 0x80)),
                                 ("$coblk", 0, 18 * 0x800)],
                          vectors=[("$cs0", "$cz0", 0), ("$cs1", "$cz1", 0),
                                   ("$cs2", "$cz2", 0), ("$cs3", "$cz3", 0),
                                   ("$cs4", "$cz4", 0), ("$cs5", "$cz5", 0),
                                   ("$cs6", "$cz6", 0), ("$cs7", "$cz7", 0),
                                   ("$cs8", "$cz8", 0), ("$cs9", "$cz9", 0),
                                   ("$cs10", "$cz10", 0), ("$cs11", "$cz11", 0),
                                   ("$cs12", "$cz12", 0), ("$cs13", "$cz13", 0),
                                   ("$cs14", "$cz14", 0), ("$cs15", "$cz15", 0),
                                   ("$cs16", "$cz16", -5), ("$cs17", "$cz17", 0)]),
}
