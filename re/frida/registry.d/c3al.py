# Batch c3al: jgld.dll (module-aware hooks; addr is the RVA, the module's VA is 0x10000000 + addr).
# Nine libpng 1.0.5 entry points jgld.dll links, plus four jgld class members. Every function is a leaf except
# png_set_sBIT (memcpy 0x1007f3a0), so each one gets at least ten vectors.
#   c3al_png     12 png_struct images (ps0..ps11), 6 png_info images (info0..info5), out words, sig_bit records
#   c3al_u32     16 four-byte buffers (b0..b15)
#   c3al_lists   12 LinkedList images (l0..l11) with distinct m_count
#   c3al_nodes   3 ListNode images (n0..n2) + a data pointer
#   c3al_matrix  3 matrices (m0..m2) + 4 vectors (v0..v3)

_PS = ["$ps%d" % i for i in range(12)]
_PNG_STATE = [("$info%d" % i, 0, 0x100) for i in range(6)]
_PS_STATE = [("$ps%d" % i, 0, 0x200) for i in range(12)]

HOOKS = {
    # Big-endian 32-bit load: 16 buffers (all-zero, each single byte position, 0xffffffff, 0x80000000, ...).
    "png_get_uint_32": dict(module="jgld.dll", addr=0x00079410, abi="default", ret="uint32", args=["pointer"],
                            fixture="c3al_u32", vectors=[("$b%d" % i,) for i in range(16)]),
    # transformations |= 1, unconditionally: 12 png_structs with 12 different transformations words.
    "png_set_bgr": dict(module="jgld.dll", addr=0x0006D600, abi="default", ret="void", args=["pointer"],
                        fixture="c3al_png", state=_PS_STATE, vectors=[(p,) for p in _PS]),
    # transformations |= 0x10 only when bit_depth (+0x127) is 16: ps0/2/4/6/8/11 take it, ps1/3/5/7/9/10 do not.
    "png_set_swap": dict(module="jgld.dll", addr=0x0006D630, abi="default", ret="void", args=["pointer"],
                         fixture="c3al_png", state=_PS_STATE, vectors=[(p,) for p in _PS]),
    # returns 7 and sets transformations |= 2 when interlaced (+0x123) is non-zero, else returns 1:
    # ps0/3/4/6/7/9/11 are interlaced, ps1/2/5/8/10 are not.
    "png_set_interlace_handling": dict(module="jgld.dll", addr=0x0006D750, abi="default", ret="int",
                                       args=["pointer"], fixture="c3al_png", state=_PS_STATE,
                                       vectors=[(p,) for p in _PS]),
    # four guards (png_ptr, info_ptr, valid bit 3, palette out) and the writing path; info1/3/4/5 have the bit
    # set, info0/2 do not.
    "png_get_PLTE": dict(module="jgld.dll", addr=0x0006EE50, abi="default", ret="uint32",
                         args=["pointer", "pointer", "pointer", "pointer"], fixture="c3al_png",
                         state=_PNG_STATE + [("$outp", 0, 8), ("$outn", 0, 8), ("$outp2", 0, 8), ("$outn2", 0, 8)],
                         vectors=[(0, "$info1", "$outp", "$outn"),
                                  ("$ps0", 0, "$outp", "$outn"),
                                  (0, 0, "$outp", "$outn"),
                                  ("$ps0", "$info0", "$outp", "$outn"),
                                  ("$ps1", "$info2", "$outp", "$outn"),
                                  ("$ps2", "$info1", 0, "$outn"),
                                  ("$ps0", "$info1", "$outp", "$outn"),
                                  ("$ps1", "$info3", "$outp", "$outn"),
                                  ("$ps2", "$info4", "$outp", "$outn"),
                                  ("$ps3", "$info5", "$outp", "$outn"),
                                  ("$ps0", "$info1", "$outp2", "$outn2"),
                                  ("$ps4", "$info3", "$outp2", "$outn")]),
    # info+0x7c = hist, valid |= 0x40, behind the two null guards.
    "png_set_hIST": dict(module="jgld.dll", addr=0x0007DC20, abi="default", ret="void",
                         args=["pointer", "pointer", "pointer"], fixture="c3al_png", state=_PNG_STATE,
                         vectors=[(0, "$info0", 0x11112222), ("$ps0", 0, 0x11112222), (0, 0, 0x11112222),
                                  ("$ps0", "$info0", 0x11112222), ("$ps1", "$info1", 0x33334444),
                                  ("$ps2", "$info2", 0), ("$ps3", "$info3", 0x7ffffffc),
                                  ("$ps4", "$info4", 0x55556666), ("$ps5", "$info5", 0x00000004),
                                  ("$ps6", "$info0", 0x77778888), ("$ps7", "$info1", 0x00000008),
                                  ("$ps8", "$info5", 0x99990000)]),
    # info+0x64/+0x68 = x/y offset, +0x6c = low byte of unit_type, valid |= 0x100.
    "png_set_oFFs": dict(module="jgld.dll", addr=0x0007DDB0, abi="default", ret="void",
                         args=["pointer", "pointer", "uint32", "uint32", "int"], fixture="c3al_png",
                         state=_PNG_STATE,
                         vectors=[(0, "$info0", 1, 2, 3), ("$ps0", 0, 1, 2, 3), (0, 0, 1, 2, 3),
                                  ("$ps0", "$info0", 0, 0, 0), ("$ps1", "$info1", 1, 2, 1),
                                  ("$ps2", "$info2", 0xffffffff, 0x7fffffff, 0xff),
                                  ("$ps3", "$info3", 0x80000000, 0, 0x100),
                                  ("$ps4", "$info4", 0x12345678, 0x9abcdef0, 2),
                                  ("$ps5", "$info5", 100, 200, 0x1ff),
                                  ("$ps6", "$info0", 0x0000ffff, 0xffff0000, -1),
                                  ("$ps7", "$info1", 7, 7, 0), ("$ps8", "$info5", 0x55555555, 0xaaaaaaaa, 0x80)]),
    # info+0x2c = low byte of intent, valid |= 0x800.
    "png_set_sRGB": dict(module="jgld.dll", addr=0x0007E0D0, abi="default", ret="void",
                         args=["pointer", "pointer", "int"], fixture="c3al_png", state=_PNG_STATE,
                         vectors=[(0, "$info0", 1), ("$ps0", 0, 1), (0, 0, 1),
                                  ("$ps0", "$info0", 0), ("$ps1", "$info1", 1), ("$ps2", "$info2", 2),
                                  ("$ps3", "$info3", 3), ("$ps4", "$info4", 0xff), ("$ps5", "$info5", 0x100),
                                  ("$ps6", "$info0", 0x1ff), ("$ps7", "$info1", -1), ("$ps8", "$info5", 0x7f)]),
    # memcpy(info+0x44, sig_bit, 5), valid |= 2; sb0 and sb1 hold different five-byte records.
    "png_set_sBIT": dict(module="jgld.dll", addr=0x0007E070, abi="default", ret="void",
                         args=["pointer", "pointer", "pointer"], fixture="c3al_png", state=_PNG_STATE,
                         vectors=[(0, "$info0", "$sb0"), ("$ps0", 0, "$sb0"), (0, 0, "$sb0"),
                                  ("$ps0", "$info0", "$sb0"), ("$ps1", "$info1", "$sb1"),
                                  ("$ps2", "$info2", "$sb0"), ("$ps3", "$info3", "$sb1"),
                                  ("$ps4", "$info4", "$sb0"), ("$ps5", "$info5", "$sb1"),
                                  ("$ps6", "$info0", "$sb1"), ("$ps7", "$info1", "$sb0"),
                                  ("$ps8", "$info5", "$sb0")]),
    # returns m_count (+0xc) of 12 lists: 0, 1, 2, 5, 7, 42, 0x100, 0x10000, 0x7fffffff, -1, INT_MIN, 3.
    "LinkedList::count": dict(module="jgld.dll", addr=0x00006D40, abi="thiscall", ret="int", args=["pointer"],
                              fixture="c3al_lists", vectors=[("$l%d" % i,) for i in range(12)]),
    # writes the ListNode vtable, data (+0xc), the low byte of flag (+0x10), 0 into next (+8) and prev (+4),
    # returns this; three nodes x four (data, flag) pairs.
    "ListNode::ctor": dict(module="jgld.dll", addr=0x00007370, abi="thiscall", ret="pointer",
                           args=["pointer", "pointer", "int"], fixture="c3al_nodes",
                           state=[("$n0", 0, 0x14), ("$n1", 0, 0x14), ("$n2", 0, 0x14)],
                           vectors=[("$n0", 0, 0), ("$n0", 0x11223344, 1), ("$n0", "$data", 0xff),
                                    ("$n0", 0x12345678, 0x5a), ("$n1", 0, 0x100), ("$n1", 0x7ffffff0, 2),
                                    ("$n1", "$data", 0x1ff), ("$n1", 0xdeadbee0, -1), ("$n2", 0x55, 0x7f),
                                    ("$n2", "$data", 0), ("$n2", 0x0f0f0f0c, 0x33), ("$n2", 0x7fff0000, 0x80)]),
    # copies three dwords from the argument into this+0x34..+0x3c; three matrices x four vectors.
    "Matrix::setTranslation": dict(module="jgld.dll", addr=0x000046F0, abi="thiscall", ret="void",
                                   args=["pointer", "pointer"], fixture="c3al_matrix",
                                   state=[("$m0", 0, 0x40), ("$m1", 0, 0x40), ("$m2", 0, 0x40)],
                                   vectors=[("$m%d" % m, "$v%d" % v) for m in range(3) for v in range(4)]),
    # (pixel << 3) & 0xf8.
    "blueOf": dict(module="jgld.dll", addr=0x0000E540, abi="default", ret="int", args=["int"],
                   vectors=[(v,) for v in (-0x80000000, -1000, -32, -17, -16, -3, -1, 0, 1, 2, 3, 7, 8, 15, 16,
                                           17, 30, 31, 32, 63, 0x55, 0xaa, 0x1f, 0x3e0, 0x7c00, 0x8000, 0xffff,
                                           12345, 0x12345678, 0x7fffffff)]),
}
