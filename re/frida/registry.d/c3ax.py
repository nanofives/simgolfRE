# Batch c3ax: sixteen jgld.dll non-render functions (module-aware hooks; addr is the RVA, VA = 0x10000000 + addr).
# Every block the vectors touch is allocated by the fixtures (re/frida/js/fixtures.d/c3ax.js); nothing live in the
# running jgld.dll (Display, Surface, Palette, Font, the game's png_struct or inflate state) is passed in.
# Reimplementations and the instruction citations: shim/src/re/c3ax.cpp; branch tables: log/c3/c3ax_purpose.md.
#
# Leaf rule: Vector3::scale, intersect, equal, ListNode::dtor, png_do_bgr, png_do_unpack, png_do_gray_to_rgb,
# png_do_read_swap_alpha, png_do_read_invert_alpha, crc32 and adler32 have no callee of their own (only the debug
# stack check __chkesp 0x1007e780 for the first four), so each carries at least 14 vectors.
#
# Hooked callee: png_reset_crc (0x00078790) calls crc32 (0x0009ca70), which this batch also hooks, so its key is
# re-run with SIMGOLF_HOOKS_OFF=jgld.dll:9ca70.

# One state region per fixture block: every object a vector can touch sits inside it at a larger stride than its
# size, and the block is pre-filled with a per-offset pattern, so a store one byte too wide or a field left
# unwritten changes the block hash.
_JBLK = [("$jblk", 0, 0x600)]
_PBLK = [("$pblk", 0, 0x2400)]
_IBLK = [("$iblk", 0, 0xB00)]

# Row transformations: the header array and the row buffer of each function. The headers are in the state even for
# the three functions that never write one, which is how "it does not touch the header" is tested.
def _rows(stem, nheaders, rowsize):
    return [(f"${stem}0", 0, nheaders * 0xC), (f"${stem}row", 0, rowsize)]


_SCALARS = [0.1, 0.3333333432674408, 1e-7, 3e5, 1.0000001, 0.9999999, 16777215.0, -2.5e-5,
            0.30000001192092896, 1234.567, -33000.0, 0.0077, 0.0, -1.0, 2.0, 1e-30]

HOOKS = {
    # ------------------------------------------------------------------ jgld: one x87 leaf, two Win32 wrappers,
    # ------------------------------------------------------------------ one destructor. Fixture c3ax_jgld.
    # Vector3::scale multiplies each of the three components by the scalar and rounds to float; the scalars are
    # values whose products are not exact in float, so a model that rounds elsewhere reads RED.
    "Vector3::scale": dict(module="jgld.dll", addr=0x00003F10, abi="thiscall", ret="void",
                           args=["pointer", "float"], fixture="c3ax_jgld", state=_JBLK,
                           vectors=[(f"$v{i}", _SCALARS[i]) for i in range(16)]
                                   + [("$v3", _SCALARS[0]), ("$v7", _SCALARS[2])]),
    # intersect: dst from the eight scratch rects, sources covering overlap, containment, edge contact, disjoint,
    # empty and inverted rectangles (IntersectRect returns 0 and clears dst for all of the last three).
    "intersect": dict(module="jgld.dll", addr=0x00008590, abi="default", ret="int",
                      args=["pointer", "pointer", "pointer"], fixture="c3ax_jgld", state=_JBLK,
                      vectors=[(f"$d{i % 8}", f"$r{i}", f"$r{(i + 1) % 16}") for i in range(16)]
                              + [("$d0", "$r0", "$r0"), ("$d1", "$r0", "$r5"), ("$d2", "$r9", "$r10"),
                                 ("$d3", "$r12", "$r13")]),
    # equal: reads only; the state region proves it writes nothing. Pairs that are identical, that differ in one
    # edge each, and that are both empty.
    "equal": dict(module="jgld.dll", addr=0x000085F0, abi="default", ret="int",
                  args=["pointer", "pointer"], fixture="c3ax_jgld", state=_JBLK,
                  vectors=[(f"$r{i}", f"$r{i}") for i in range(8)]
                          + [(f"$r{i}", f"$r{i + 1}") for i in range(8)]
                          + [("$r0", "$r2"), ("$r9", "$r11"), ("$r14", "$r14"), ("$r15", "$r14")]),
    # ListNode::dtor: 16 nodes inside the one block, each at a larger stride than the three words it writes.
    "ListNode::dtor": dict(module="jgld.dll", addr=0x00007450, abi="thiscall", ret="void",
                           args=["pointer"], fixture="c3ax_jgld", state=_JBLK,
                           vectors=[(f"$n{i}",) for i in range(16)]),

    # ------------------------------------------------------------------ libpng helpers. Fixture c3ax_png.
    # png_reset_crc stores crc32(0, NULL, 0) = 0 at +0x110 of each scratch png_struct; the twelve structs start
    # with twelve different values there, so the twelve vectors differ in both the before and the after state.
    "png_reset_crc": dict(module="jgld.dll", addr=0x00078790, abi="default", ret="void",
                          args=["pointer"], fixture="c3ax_png", state=_PBLK,
                          vectors=[(f"$pc{i}",) for i in range(12)]),
    # png_info_init zeroes 0xb8 bytes of each record; the records sit at a 0x100 stride inside the same block and
    # are pre-filled, so the bytes from +0xb8 to +0xff must survive.
    "png_info_init": dict(module="jgld.dll", addr=0x00078940, abi="default", ret="void",
                          args=["pointer"], fixture="c3ax_png", state=_PBLK,
                          vectors=[(f"$pi{i}",) for i in range(12)]),
    # png_memcpy_check / png_memset_check: lengths around the CRT's block boundaries (0, 1..8, 15..17, 31..33,
    # 64, 100, 255) and, for memset, values whose low byte is what memset uses (0x100, 0x1FF, negative).
    "png_memcpy_check": dict(module="jgld.dll", addr=0x00078F20, abi="default", ret="pointer",
                             args=["pointer", "pointer", "pointer", "uint32"], fixture="c3ax_png", state=_PBLK,
                             vectors=[("$pngp", "$mdst", "$msrc", n) for n in
                                      (0, 1, 2, 3, 4, 5, 7, 8, 9, 15, 16, 17, 31, 32, 33, 64, 100, 255)]
                                     + [("$pngp", "$mdst2", "$msrc2", n) for n in (1, 8, 64)]),
    "png_memset_check": dict(module="jgld.dll", addr=0x00078F80, abi="default", ret="pointer",
                             args=["pointer", "pointer", "int", "uint32"], fixture="c3ax_png", state=_PBLK,
                             vectors=[("$pngp", "$mdst", v, n) for v, n in
                                      ((0, 0), (0xFF, 1), (0x5A, 2), (1, 3), (0x7F, 4), (0x80, 7), (0xAB, 8),
                                       (0x100, 9), (0x1FF, 15), (-1, 16), (0x12, 17), (0xCD, 31), (3, 32),
                                       (0xE0, 33), (0x0F, 64), (0x33, 100), (0x99, 255))]
                                     + [("$pngp", "$mdst2", 0x44, n) for n in (1, 16, 64)]),

    # ------------------------------------------------------------------ libpng row transformations.
    # Fixture c3ax_rows. Each key's state is its own header array plus its own row buffer.
    "png_do_bgr": dict(module="jgld.dll", addr=0x0006E010, abi="default", ret="void",
                       args=["pointer", "pointer"], fixture="c3ax_rows", state=_rows("bgr", 15, 128),
                       vectors=[(f"$bgr{i}", "$bgrrow") for i in range(15)]),
    "png_do_unpack": dict(module="jgld.dll", addr=0x00071E80, abi="default", ret="void",
                          args=["pointer", "pointer"], fixture="c3ax_rows", state=_rows("unp", 15, 64),
                          vectors=[(f"$unp{i}", "$unprow") for i in range(15)]),
    "png_do_gray_to_rgb": dict(module="jgld.dll", addr=0x00073310, abi="default", ret="void",
                               args=["pointer", "pointer"], fixture="c3ax_rows", state=_rows("g2r", 15, 128),
                               vectors=[(f"$g2r{i}", "$g2rrow") for i in range(15)]),
    "png_do_read_swap_alpha": dict(module="jgld.dll", addr=0x000724F0, abi="default", ret="void",
                                   args=["pointer", "pointer"], fixture="c3ax_rows", state=_rows("swa", 14, 128),
                                   vectors=[(f"$swa{i}", "$swarow") for i in range(14)]),
    "png_do_read_invert_alpha": dict(module="jgld.dll", addr=0x00072860, abi="default", ret="void",
                                     args=["pointer", "pointer"], fixture="c3ax_rows", state=_rows("ina", 14, 128),
                                     vectors=[(f"$ina{i}", "$inarow") for i in range(14)]),

    # ------------------------------------------------------------------ zlib. Fixture c3ax_zlib.
    # crc32: a null buffer (three seeds), lengths below, at and above the eight-byte fold, lengths that are and
    # are not multiples of 8, misaligned starts, and seeds that exercise every byte of the running value.
    "crc32": dict(module="jgld.dll", addr=0x0009CA70, abi="default", ret="uint32",
                  args=["uint32", "pointer", "uint32"], fixture="c3ax_zlib",
                  vectors=[(0, 0, 0), (0xFFFFFFFF, 0, 16), (0x12345678, 0, 1)]
                          + [(0, "$data", n) for n in (0, 1, 2, 3, 7, 8, 9, 15, 16, 17, 64, 100, 1000, 4096)]
                          + [(0xFFFFFFFF, "$data", n) for n in (1, 8, 33)]
                          + [(0x12345678, "$data1", n) for n in (7, 8, 23)]
                          + [(0xDEADBEEF, "$data3", 64), (0x80000000, "$data7", 1), (1, "$data7", 8192)]),
    # adler32: a null buffer, length 0, lengths around the 16-byte group and around the 0x15b0 chunk (5551,
    # 5552, 5553) and two lengths that need two and three chunks, so the modulo at the end of a chunk runs more
    # than once; seeds with both halves non-zero.
    "adler32": dict(module="jgld.dll", addr=0x0009E370, abi="default", ret="uint32",
                    args=["uint32", "pointer", "uint32"], fixture="c3ax_zlib",
                    vectors=[(1, 0, 0), (0xFFFFFFFF, 0, 16), (0x12345678, 0, 1)]
                            + [(1, "$data", n) for n in (0, 1, 2, 15, 16, 17, 31, 32, 33, 100, 1000)]
                            + [(1, "$data", n) for n in (5551, 5552, 5553, 11104, 12000)]
                            + [(0x12345678, "$data", n) for n in (1, 16, 5553)]
                            + [(0xFFF0FFF0, "$data1", 64), (0x00010001, "$data3", 5552),
                               (0xABCD1234, "$data7", 7)]),
    # inflate_set_dictionary: four scratch states, each with its own window inside the same block; lengths 0 and
    # up to 255 bytes of the dictionary block, so the two pointer fields take different values per vector.
    "inflate_set_dictionary": dict(module="jgld.dll", addr=0x0009E310, abi="default", ret="void",
                                   args=["pointer", "pointer", "uint32"], fixture="c3ax_zlib", state=_IBLK,
                                   vectors=[("$st0", "$dict", 0), ("$st0", "$dict", 1), ("$st1", "$dict", 7),
                                            ("$st1", "$dict", 16), ("$st2", "$dict", 33), ("$st2", "$dict", 64),
                                            ("$st3", "$dict", 100), ("$st3", "$dict", 255),
                                            ("$st0", "$dict8", 8), ("$st1", "$dict8", 32),
                                            ("$st2", "$dict8", 128), ("$st3", "$dict8", 1)]),
}
