# Batch c3bb: fourteen jgld.dll non-render functions (module-aware hooks; addr is the RVA,
# VA = 0x10000000 + addr). Every block the vectors touch is allocated by the fixtures
# (re/frida/js/fixtures.d/c3bb.js); nothing live in the running jgld.dll (Display, Surface, Palette,
# Font, the game's png_struct, inflate state or heap blocks) is passed in.
# Reimplementations and the instruction citations: shim/src/re/c3bb.cpp; branch tables:
# log/c3/c3bb_purpose.md.
#
# Leaf rule: Array_Rec14::count, Array_Rec94::count, Array_Rec14::reset, Array_Rec94::reset and
# initDepthInfo have no callee of their own, so each carries 16 vectors, and Surface::getField810
# carries 12. The remaining eight all call at least the CRT memcpy 0x1007f3a0 or the debug stack check
# __chkesp 0x1007e780.
#
# Hooked callees: NONE. The callees of this batch's functions are the CRT memcpy 0x1007f3a0, operator
# new 0x1007f0f0, operator delete 0x1007e6d0 (both only on the Array growth path, which no vector
# takes), png_warning 0x10079040 and __chkesp 0x1007e780, and none of them carries an SG_HOOK.
# inflate_flush's indirect check function is a stub of our own (fixture c3bb_zlib), not zlib's adler32
# 0x0009e370, precisely so that this key does not depend on batch c3ax's hook.

# One state region per fixture block: every object sits inside it at a larger stride than its size and
# the block is pre-filled with a per-offset pattern, so a store one byte too wide or a field left
# unwritten changes the block hash.
_SBLK = [("$sblk", 0, 0xA00)]
_FBLK = [("$fblk", 0, 12 * 0x820)]
_ABLK = [("$ablk", 0, 0x4000)]
_PBLK = [("$pblk", 0, 0x2000)]
_ZBLK = [("$zblk", 0, 0x1000)]

# Five dwords for a Rec14 and 0x25 for a Rec94, passed by value on the stack behind `this` in ecx.
_V14 = [[0x11110000 + i, 0x22220000 + i * 3, -i - 1, 0x7FFFFFFF - i, (i * 0x01010101) & 0xFFFFFFFF]
        for i in range(12)]
_V94 = [[(i * 0x100 + j * 7 + 0x13) & 0xFFFFFFFF for j in range(0x25)] for i in range(12)]

HOOKS = {
    # ------------------------------------------------------------------ jgld's one-field accessors.
    # Fixture c3bb_small: sixteen objects per key inside one pattern-filled block, each with a
    # different count at +0xc, so sixteen vectors give sixteen distinct results (count) or sixteen
    # different block hashes (reset).
    "Array_Rec14::count": dict(module="jgld.dll", addr=0x00066A80, abi="thiscall", ret="int",
                               args=["pointer"], fixture="c3bb_small", state=_SBLK,
                               vectors=[(f"$c14_{i}",) for i in range(16)]),
    "Array_Rec94::count": dict(module="jgld.dll", addr=0x00066B50, abi="thiscall", ret="int",
                               args=["pointer"], fixture="c3bb_small", state=_SBLK,
                               vectors=[(f"$c94_{i}",) for i in range(16)]),
    # reset leaves `this` in eax without any instruction setting a return value, so ret is "void" and
    # the comparison is on memory only.
    "Array_Rec14::reset": dict(module="jgld.dll", addr=0x00066AC0, abi="thiscall", ret="void",
                               args=["pointer"], fixture="c3bb_small", state=_SBLK,
                               vectors=[(f"$r14_{i}",) for i in range(16)]),
    "Array_Rec94::reset": dict(module="jgld.dll", addr=0x00066B90, abi="thiscall", ret="void",
                               args=["pointer"], fixture="c3bb_small", state=_SBLK,
                               vectors=[(f"$r94_{i}",) for i in range(16)]),
    # initDepthInfo reloads `this` into eax after its store, so the pointer IS the return value.
    "initDepthInfo": dict(module="jgld.dll", addr=0x000092E0, abi="thiscall", ret="pointer",
                          args=["pointer"], fixture="c3bb_small", state=_SBLK,
                          vectors=[(f"$dep_{i}",) for i in range(16)]),
    # Surface::getField810: twelve scratch surfaces of 0x814 bytes at a 0x820 stride; the twelve values
    # at +0x810 include 0, both signed extremes and -1.
    "Surface::getField810": dict(module="jgld.dll", addr=0x0000AEF0, abi="thiscall", ret="int",
                                 args=["pointer"], fixture="c3bb_surfaces", state=_FBLK,
                                 vectors=[(f"$sf_{i}",) for i in range(12)]),

    # ------------------------------------------------------------------ Array<T>::add, room branch.
    # Every object has count < cap, so the growth branch (operator new / operator delete on the game's
    # heap) is never taken; the element is a different 0x14- or 0x94-byte value per vector and lands at
    # a different slot, so the block hash differs for all twelve.
    "Array_Rec14::add": dict(module="jgld.dll", addr=0x000681A0, abi="thiscall", ret="int",
                             args=["pointer"] + ["int"] * 5, fixture="c3bb_arrays", state=_ABLK,
                             vectors=[tuple([f"$a14_{i}"] + _V14[i]) for i in range(12)]),
    "Array_Rec94::add": dict(module="jgld.dll", addr=0x00068400, abi="thiscall", ret="int",
                             args=["pointer"] + ["int"] * 0x25, fixture="c3bb_arrays", state=_ABLK,
                             vectors=[tuple([f"$a94_{i}"] + _V94[i]) for i in range(12)]),

    # ------------------------------------------------------------------ libpng row transformations.
    # Fixture c3bb_rows. Each key's state is its own header array, its own row buffer and the read-only
    # tables it indexes (which is how "it does not write them" is tested).
    # png_do_unshift: sixteen (header, sig_bits) pairs covering the palette guard, the colour and alpha
    # branches, a wholly clamped shift set, a mixed one, bit depths 2, 4, 8 and 16, two empty loops,
    # a depth inside the switch range with no case (5) and one that wraps past it (1).
    "png_do_unshift": dict(module="jgld.dll", addr=0x000720C0, abi="default", ret="void",
                           args=["pointer", "pointer", "pointer"], fixture="c3bb_rows",
                           state=[("$uns0", 0, 16 * 0xC), ("$sig0", 0, 16 * 8), ("$unsrow", 0, 64)],
                           vectors=[(f"$uns{i}", "$unsrow", f"$sig{i}") for i in range(16)]),
    # png_do_expand_palette: the num_trans column is the fifth argument, so a vector carries its own
    # (header, trans, num_trans) combination; 0 for trans selects the three-byte body.
    "png_do_expand_palette": dict(module="jgld.dll", addr=0x00076EC0, abi="default", ret="void",
                                  args=["pointer", "pointer", "pointer", "pointer", "int"],
                                  fixture="c3bb_rows",
                                  state=[("$exp0", 0, 16 * 0xC), ("$exprow", 0, 128), ("$pal", 0, 768),
                                         ("$trans", 0, 256)],
                                  vectors=[("$exp0", "$exprow", "$pal", 0, 0),
                                           ("$exp1", "$exprow", "$pal", 0, 0),
                                           ("$exp2", "$exprow", "$pal", "$trans", 4),
                                           ("$exp3", "$exprow", "$pal", "$trans", 0),
                                           ("$exp4", "$exprow", "$pal", "$trans", 256),
                                           ("$exp5", "$exprow", "$pal", 0, 0),
                                           ("$exp6", "$exprow", "$pal", "$trans", 3),
                                           ("$exp7", "$exprow", "$pal", 0, 0),
                                           ("$exp8", "$exprow", "$pal", "$trans", 10),
                                           ("$exp9", "$exprow", "$pal", "$trans", 5),
                                           ("$exp10", "$exprow", "$pal", 0, 0),
                                           ("$exp11", "$exprow", "$pal", "$trans", 4),
                                           ("$exp12", "$exprow", "$pal", 0, 0),
                                           ("$exp13", "$exprow", "$pal", "$trans", 2),
                                           ("$exp14", "$exprow", "$pal", 0, 0),
                                           ("$exp15", "$exprow", "$pal", "$trans", 128)]),
    # png_do_dither: headers 3, 8 and 11 are run with the lookup their body needs set to NULL, which is
    # the only way to take the second test of each of the three guards.
    "png_do_dither": dict(module="jgld.dll", addr=0x00077A90, abi="default", ret="void",
                          args=["pointer", "pointer", "pointer", "pointer"], fixture="c3bb_rows",
                          state=[("$dit0", 0, 16 * 0xC), ("$ditrow", 0, 128), ("$plut", 0, 0x8000),
                                 ("$dlut", 0, 256)],
                          vectors=[("$dit0", "$ditrow", "$plut", "$dlut"),
                                   ("$dit1", "$ditrow", "$plut", "$dlut"),
                                   ("$dit2", "$ditrow", "$plut", "$dlut"),
                                   ("$dit3", "$ditrow", 0, "$dlut"),
                                   ("$dit4", "$ditrow", "$plut", "$dlut"),
                                   ("$dit5", "$ditrow", "$plut", "$dlut"),
                                   ("$dit6", "$ditrow", "$plut", "$dlut"),
                                   ("$dit7", "$ditrow", "$plut", "$dlut"),
                                   ("$dit8", "$ditrow", 0, "$dlut"),
                                   ("$dit9", "$ditrow", "$plut", "$dlut"),
                                   ("$dit10", "$ditrow", "$plut", "$dlut"),
                                   ("$dit11", "$ditrow", "$plut", 0),
                                   ("$dit12", "$ditrow", "$plut", "$dlut"),
                                   ("$dit13", "$ditrow", "$plut", "$dlut"),
                                   ("$dit14", "$ditrow", "$plut", "$dlut"),
                                   ("$dit15", "$ditrow", "$plut", "$dlut")]),

    # ------------------------------------------------------------------ libpng error and filter code.
    # png_format_buffer: twelve chunk names covering the three escape ranges and their four closed
    # boundaries, each first with a null message (the buffer is terminated in place) and then eight of
    # them with a message (':', ' ' and 0x40 copied bytes).
    "png_format_buffer": dict(module="jgld.dll", addr=0x00079100, abi="default", ret="void",
                              args=["pointer", "pointer", "pointer"], fixture="c3bb_png", state=_PBLK,
                              vectors=[(f"$ps{i}", f"$ob{i}", 0) for i in range(12)]
                                      + [("$ps0", "$ob12", "$msg0"), ("$ps1", "$ob13", "$msg1"),
                                         ("$ps2", "$ob14", "$msg2"), ("$ps3", "$ob15", "$msg3"),
                                         ("$ps6", "$ob16", "$msg0"), ("$ps7", "$ob17", "$msg1"),
                                         ("$ps10", "$ob18", "$msg2"), ("$ps11", "$ob19", "$msg3")]),
    # png_read_filter_row: the five filter numbers of the jump table, three values that take the
    # default (5, 7 and -1, the last through the UNSIGNED range test), and headers whose rowbytes make
    # a loop body run zero times (fh5 rowbytes 1, fh6 rowbytes 0).
    "png_read_filter_row": dict(module="jgld.dll", addr=0x0007CE80, abi="default", ret="void",
                                args=["pointer", "pointer", "pointer", "pointer", "int"],
                                fixture="c3bb_png",
                                state=_PBLK + [("$fh0", 0, 8 * 0xC), ("$fbuf", 0, 0x100)],
                                vectors=[("$fpng", "$fh0", "$frow", "$fprev", 0),
                                         ("$fpng", "$fh1", "$frow2", "$fprev2", 0),
                                         ("$fpng", "$fh0", "$frow", "$fprev", 1),
                                         ("$fpng", "$fh1", "$frow", "$fprev", 1),
                                         ("$fpng", "$fh2", "$frow2", "$fprev", 1),
                                         ("$fpng", "$fh3", "$frow", "$fprev", 1),
                                         ("$fpng", "$fh5", "$frow", "$fprev", 1),
                                         ("$fpng", "$fh0", "$frow", "$fprev", 2),
                                         ("$fpng", "$fh2", "$frow2", "$fprev2", 2),
                                         ("$fpng", "$fh6", "$frow", "$fprev", 2),
                                         ("$fpng", "$fh0", "$frow", "$fprev", 3),
                                         ("$fpng", "$fh1", "$frow2", "$fprev", 3),
                                         ("$fpng", "$fh2", "$frow", "$fprev2", 3),
                                         ("$fpng", "$fh5", "$frow", "$fprev", 3),
                                         ("$fpng", "$fh0", "$frow", "$fprev", 4),
                                         ("$fpng", "$fh1", "$frow2", "$fprev", 4),
                                         ("$fpng", "$fh2", "$frow", "$fprev2", 4),
                                         ("$fpng", "$fh3", "$frow2", "$fprev2", 4),
                                         ("$fpng", "$fh4", "$frow2", "$fprev2", 4),
                                         ("$fpng", "$fh7", "$frow", "$fprev", 4),
                                         ("$fpng", "$fh5", "$frow", "$fprev", 4),
                                         ("$fpng", "$fh0", "$frow", "$fprev", 5),
                                         ("$fpng", "$fh1", "$frow2", "$fprev2", 7),
                                         ("$fpng", "$fh0", "$frow", "$fprev", -1)]),

    # ------------------------------------------------------------------ zlib. Fixture c3bb_zlib.
    # inflate_flush: ten states x six streams x the status values -5 (the one the body rewrites), -3,
    # 0, 1 and 2, chosen so that each of the two blocks sees a clamped and an unclamped length, a zero
    # and a non-zero length, a null and a non-null check function, and so that the wrap is taken with
    # the write pointer both at and away from the end of the window.
    "inflate_flush": dict(module="jgld.dll", addr=0x000A0130, abi="default", ret="int",
                          args=["pointer", "pointer", "int"], fixture="c3bb_zlib", state=_ZBLK,
                          vectors=[("$s0", "$z0", 0), ("$s0", "$z0", -5), ("$s0", "$z1", -5),
                                   ("$s0", "$z2", -5), ("$s0", "$z3", 2),
                                   ("$s1", "$z0", 0), ("$s1", "$z1", -5), ("$s1", "$z3", -5),
                                   ("$s2", "$z0", 1), ("$s2", "$z5", 0),
                                   ("$s3", "$z0", -5), ("$s4", "$z0", -5),
                                   ("$s5", "$z0", 0), ("$s5", "$z2", -5),
                                   ("$s6", "$z0", -3), ("$s6", "$z5", 0),
                                   ("$s7", "$z4", -5), ("$s7", "$z1", 0),
                                   ("$s8", "$z0", -5), ("$s8", "$z3", 1),
                                   ("$s9", "$z4", -5), ("$s9", "$z1", 0)]),
}
