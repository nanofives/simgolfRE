# Batch c3bq: jgld.dll non-render (module-aware hooks; addr is the RVA, VA = 0x10000000 + addr).
# Every block the vectors touch is allocated by the fixtures (re/frida/js/fixtures.d/c3bq.js); nothing
# live in the running jgld.dll (Display, Surface, Palette, Font, the game's png_struct / png_info or
# its heap blocks) is passed in. Reimplementations and the instruction citations:
# shim/src/re/c3bq.cpp; branch tables: log/c3/c3bq_purpose.md.
#
# Hooked callees to switch off when re-running: NONE. png_read_transform_info has no callee at all;
# png_set_IHDR, png_get_IHDR and png_set_read_fn reach only png_warning 0x00079040 and the debug stack
# check __chkesp 0x0007e780, and neither carries an SG_HOOK in any batch; Palette::reset,
# Palette::apply and Matrix::loadIdentity have no callee.
#
# Leaf rule (>= 10 vectors): png_read_transform_info (32), Palette::reset (12), Palette::apply (14)
# and Matrix::loadIdentity (12) are leaves. The three libpng setters/getters are not.

# png_set_IHDR's nine scalar arguments, in the order the caller pushes them: width, height, bit_depth,
# color_type, interlace_type, compression_type, filter_type. A bit_depth of 0 would make the divisor at
# 0x1007dd5a zero and the original faults on the `div`, so none is passed.
_SI = [
    (16, 8, 8, 0, 0, 0, 0),             # 0  png_ptr null
    (16, 8, 8, 0, 0, 0, 0),             # 1  info_ptr null
    (16, 8, 8, 0, 0, 0, 0),             # 2  grey 8
    (16, 8, 8, 2, 0, 0, 0),             # 3  colour type 2
    (16, 8, 8, 3, 0, 0, 0),             # 4  colour type 3
    (16, 8, 8, 4, 0, 0, 0),             # 5  colour type 4
    (16, 8, 8, 6, 0, 0, 0),             # 6  colour type 6
    (16, 8, 8, 1, 0, 0, 0),             # 7  colour type 1
    (16, 8, 8, 5, 0, 0, 0),             # 8  colour type 5
    (16, 8, 8, 7, 0, 0, 0),             # 9  colour type 7
    (100, 50, 16, 6, 0, 0, 0),          # 10 depth 16, four channels
    (7, 3, 1, 0, 0, 0, 0),              # 11 depth 1
    (5, 5, 2, 3, 1, 0, 0),              # 12 depth 2, palette
    (9, 9, 4, 0, 1, 2, 3),              # 13 the three trailing types, each a different value
    (0x10000000, 1, 16, 6, 0, 0, 0),    # 14 one past 0x7fffffff / 8: the warning arm
    (0x0FFFFFFF, 1, 16, 6, 0, 0, 0),    # 15 exactly the quotient: the `jbe` boundary
    (0x7FFFFFFF, 1, 8, 0, 0, 0, 0),     # 16 one byte a pixel, the largest width that passes
    (0x80000000, 1, 8, 0, 0, 0, 0),     # 17 one byte a pixel, the first width that warns
    (0, 0, 8, 2, 0, 0, 0),              # 18 width 0
]

_SI_VECTORS = []
for _i, _v in enumerate(_SI):
    _png = 0 if _i == 0 else "$siPng"
    _info = 0 if _i == 1 else "$siI%d" % _i
    _SI_VECTORS.append((_png, _info) + _v)

# png_get_IHDR's seven out pointers, in argument order: width, height, bit_depth, color_type,
# interlace_type, compression_type, filter_type. "None" below means the vector passes NULL there.
_GI_NULL = {
    0: (), 1: (), 2: (0,), 3: (1,), 4: (2,), 5: (3,),
    6: (4, 5, 6),                        # the three optional pointers at once
    8: (4,), 9: (5,), 10: (6,),
}
_GI_VECTORS = []
for _i in range(20):
    _nulls = _GI_NULL.get(_i, ())
    _outs = tuple(0 if _j in _nulls else "$giO%d_%d" % (_i, _j) for _j in range(7))
    _png = 0 if _i == 0 else "$giPng"
    _info = 0 if _i == 1 else "$giI%d" % _i
    _GI_VECTORS.append((_png, _info) + _outs)

# png_set_read_fn: record i carries a non-null write_data_fn at +0x4c when i is odd, which is the arm
# that clears it and warns twice (0x10078cf3); the third argument alternates between NULL (the default
# reader is installed, 0x10078ce5) and a stub of ours.
_RF_VECTORS = [
    ("$rf0", "$rfIo", 0),
    ("$rf1", "$rfIo", "$rfFn"),
    ("$rf2", "$rfIo", "$rfFn"),
    ("$rf3", "$rfIo", 0),
    ("$rf4", 0, 0),
    ("$rf5", "$rfWarn", "$rfFn"),
    ("$rf6", "$rfIo", 0),
    ("$rf7", "$rfIo", "$rfFn"),
    ("$rf8", 0, "$rfFn"),
    ("$rf9", "$rfIo", 0),
    ("$rf10", "$rfIo", "$rfFn"),
    ("$rf11", 0, 0),
]

HOOKS = {
    # png_read_transform_info 0x000714c0: one scratch png_struct and one scratch png_info per vector,
    # one transformation bit (or one combination) each, so every bit test of the word at +0x70 is taken
    # and not taken, each guard of PNG_EXPAND, PNG_DITHER, PNG_16_TO_8, PNG_PACK, PNG_FILLER and
    # PNG_USER_TRANSFORM is decided by a different field, and the three channel arms are reached by a
    # colour type of their own. The png_struct block is a state region because the body must not write
    # to it.
    "png_read_transform_info": dict(module="jgld.dll", addr=0x000714C0, abi="default", ret="void",
                                    args=["pointer", "pointer"], fixture="c3bq_rti",
                                    state=[("$rtiPng", 0, 32 * 0x200), ("$rtiInfo", 0, 32 * 0x80)],
                                    vectors=[("$rtiP%d" % i, "$rtiI%d" % i) for i in range(32)]),

    # png_set_IHDR 0x0007dc70: the two null guards, the three channel arms and the alpha increment, and
    # both sides of the overflow test at 0x1007dd60 (vectors 14 and 17 warn through the handler the
    # fixture puts at png_struct+0x44; 15 and 16 are the exact boundary widths that do not).
    "png_set_IHDR": dict(module="jgld.dll", addr=0x0007DC70, abi="default", ret="void",
                         args=["pointer", "pointer", "uint32", "uint32", "int", "int", "int", "int", "int"],
                         fixture="c3bq_sihdr",
                         state=[("$siPng", 0, 0x200), ("$siInfo", 0, len(_SI) * 0x80)],
                         vectors=_SI_VECTORS),

    # png_get_IHDR 0x0006eae0: the six required pointers are null one at a time (vectors 0 to 5, each
    # returning 0), the three optional ones are null together and then one at a time, and the colour
    # types, the depth and the two boundary widths drive the channel arms and the overflow test. The
    # body reads its own results back out of the caller's buffers, so the out block is a state region.
    "png_get_IHDR": dict(module="jgld.dll", addr=0x0006EAE0, abi="default", ret="int",
                         args=["pointer", "pointer", "pointer", "pointer", "pointer", "pointer",
                               "pointer", "pointer", "pointer"],
                         fixture="c3bq_gihdr",
                         state=[("$giPng", 0, 0x200), ("$giInfo", 0, 20 * 0x80), ("$giOut", 0, 20 * 0x40)],
                         vectors=_GI_VECTORS),

    # png_set_read_fn 0x00078cb0: both sides of the read_data_fn test (0x10078cd5) and of the
    # write_data_fn test (0x10078cf3). The io_ptr argument is stored without any test, so it varies to
    # show the store.
    "png_set_read_fn": dict(module="jgld.dll", addr=0x00078CB0, abi="default", ret="void",
                            args=["pointer", "pointer", "pointer"], fixture="c3bq_readfn",
                            state=[("$rfBlk", 0, 12 * 0x200)], vectors=_RF_VECTORS),

    # Palette::reset 0x0006a380: twelve objects in one block, each with a different pair of serials, so
    # the two zero stores land in a different place in the region for every vector.
    "Palette::reset": dict(module="jgld.dll", addr=0x0006A380, abi="thiscall", ret="void",
                           args=["pointer"], fixture="c3bq_palette",
                           state=[("$palBlk", 0, 26 * 0x840)],
                           vectors=[("$palR%d" % i,) for i in range(12)]),

    # Palette::apply 0x0006a4a0: vector 0 has the two serials equal and takes the early exit at
    # 0x1006a4cf; the other thirteen each build both tables from a different set of 256 entries.
    "Palette::apply": dict(module="jgld.dll", addr=0x0006A4A0, abi="thiscall", ret="void",
                           args=["pointer"], fixture="c3bq_palette",
                           state=[("$palBlk", 0, 26 * 0x840)],
                           vectors=[("$palA%d" % i,) for i in range(14)]),

    # Matrix::loadIdentity 0x000045f0: twelve objects in one block; each vector zeroes sixteen cells of
    # its own object and sets four of them to 1.0f, leaving the dword at +0 (in the region) alone.
    "Matrix::loadIdentity": dict(module="jgld.dll", addr=0x000045F0, abi="thiscall", ret="void",
                                 args=["pointer"], fixture="c3bq_matrix",
                                 state=[("$matBlk", 0, 12 * 0x60)],
                                 vectors=[("$mat%d" % i,) for i in range(12)]),
}
