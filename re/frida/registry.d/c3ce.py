# Batch c3ce: the jgld.dll Surface methods reached only through a slot of the Surface vtable at RVA 0x11d0b0
# (module-aware hooks; addr is the RVA, VA = 0x10000000 + addr). Every Surface, Palette, pixel buffer, rectangle
# and lookup table the vectors touch is allocated by the fixture (re/frida/js/fixtures.d/c3ce.js) on jgld's own
# vtables; nothing live (the Display at 0x10128420, a screen surface, DirectDraw) is passed in or written.
#
# State regions, all inside the fixture's single block:
#   $o0..$o15  surface + 0x40, 0x28 bytes: the row stride (+0x40), the clip RECT (+0x44) and the bounds RECT
#              (+0x54). Surface::callBounds 0x0000a8d0 writes the clip rectangle through setClip 0x000083c0.
#   $w0..$w15  surface + 0x4b8, 0x20 bytes: the handed-out device context (+0x4b8), the stored one (+0x4bc), the
#              pixel base (+0x4c0), the two use counts (+0x4c4, +0x4c8), the handed-out base (+0x4cc) and the
#              spare dword (+0x4d0). The four acquire/release slots and 0x0000a5d0 write here.
#   $out       the eight 0x10-byte destination rectangles of 0x0000a930 and 0x0000a9d0.
#   $pix       the sixteen pixel buffers, added only for the four drawing dispatchers, whose evidence is the
#              pixels the primitive they picked wrote.
#
# No module global is a state region, and none of the seven primitives the dispatchers forward to writes one: a
# scan of the disassembly of fill8 0xf880, fill16 0xafe0, fill2_8 0xf9e0, line8 0xfb70, line16 0xb140,
# ditherRectC8 0x11f80 and ditherRect16 0xe580 finds no `mov [0x101.....], ...`. The only module global any of
# them touches is 0x1012873c, which line16 and ditherRect16 READ when the surface carries no Palette; every
# 16-bit surface used by these vectors carries one of the fixture's own, so that read never decides anything.

_NS = 16
_OBJ = [("$o%d" % i, 0, 0x28) for i in range(_NS)]
_WIN = [("$w%d" % i, 0, 0x20) for i in range(_NS)]
_OUT = [("$out", 0, 0x80)]
_STATE = _OBJ + _WIN + _OUT
_STATE_PIX = _STATE + [("$pix", 0, 0x4000)]

_F = "c3ce_surfaces"
_ALL = ["$s%d" % i for i in range(_NS)]
# The ten surfaces whose stored device context (+0x4bc) is NULL, so setClip never reaches its GDI block.
_NODC = ["$s1", "$s3", "$s5", "$s6", "$s7", "$s8", "$s9", "$s10", "$s12", "$s13", "$s14", "$s15"]

HOOKS = {
    # ---- acquireDC (slot +0x28): hands out [this+0x4bc] through [this+0x4b8] and bumps the count at +0x4c4.
    #      No branch. s0/s2/s4/s11 have a stored context, the other twelve have none, so the result is a pointer on
    #      four surfaces and NULL on ten; the count goes 0->1, 1->2, 3->4 and 5->6.
    "Surface::acquireDC": dict(
        module="jgld.dll", addr=0x00008640, abi="thiscall", ret="pointer", args=["pointer"],
        fixture=_F, state=_STATE, vectors=[(s,) for s in _ALL]),

    # ---- releaseDC (slot +0x2c): count -= n, and at <= 0 clear the handed-out context and the count.
    #      Both sides of the test at VA 0x100086ec are driven: n smaller than the count leaves it standing,
    #      n equal to or larger than it clears, and a negative n raises it.
    "Surface::releaseDC": dict(
        module="jgld.dll", addr=0x000086B0, abi="thiscall", ret="void", args=["pointer", "int"],
        fixture=_F, state=_STATE,
        vectors=[("$s2", 1),      # count 3 -> 2: greater than zero, nothing cleared
                 ("$s2", 2),      # count 3 -> 1
                 ("$s2", 3),      # count 3 -> 0: cleared
                 ("$s2", 5),      # count 3 -> -2: cleared
                 ("$s2", 0),      # count unchanged at 3
                 ("$s2", -1),     # count 3 -> 4
                 ("$s4", 1),      # count 1 -> 0: cleared
                 ("$s4", 0),      # count 1 -> 1
                 ("$s11", 2),     # count 5 -> 3
                 ("$s11", 5),     # count 5 -> 0: cleared
                 ("$s11", 6),     # count 5 -> -1: cleared
                 ("$s0", 0),      # count 0 -> 0: cleared (the zero case takes the clearing arm)
                 ("$s0", -1),     # count 0 -> 1: not cleared
                 ("$s1", 1)]),    # count 0 -> -1 on a surface with no stored context

    # ---- acquireDC2 (slot +0x10): hands out the pixel base [this+0x4c0] through [this+0x4cc] and bumps the
    #      count at +0x4c8 only when the base is not NULL (VA 0x10008769). s9 is the NULL-base surface.
    "Surface::acquireDC2": dict(
        module="jgld.dll", addr=0x00008730, abi="thiscall", ret="pointer", args=["pointer"],
        fixture=_F, state=_STATE, vectors=[(s,) for s in _ALL]),

    # ---- releaseDC2 (slot +0x24): the same shape as releaseDC on +0x4c8 / +0x4cc. s13 starts at 7, s11 at 4,
    #      s2 at 2, s4 at 1 and the rest at 0.
    "Surface::releaseDC2": dict(
        module="jgld.dll", addr=0x000087B0, abi="thiscall", ret="void", args=["pointer", "int"],
        fixture=_F, state=_STATE,
        vectors=[("$s13", 1),     # 7 -> 6
                 ("$s13", 6),     # 7 -> 1
                 ("$s13", 7),     # 7 -> 0: cleared
                 ("$s13", 9),     # 7 -> -2: cleared
                 ("$s13", 0),     # unchanged
                 ("$s13", -3),    # 7 -> 10
                 ("$s11", 2),     # 4 -> 2
                 ("$s11", 4),     # 4 -> 0: cleared
                 ("$s2", 1),      # 2 -> 1
                 ("$s2", 2),      # 2 -> 0: cleared
                 ("$s4", 1),      # 1 -> 0: cleared
                 ("$s0", 1),      # 0 -> -1: cleared, nothing to clear
                 ("$s0", -2),     # 0 -> 2: not cleared
                 ("$s9", 1)]),    # no base at all

    # ---- setField4d0 (slot +0xbc) and getField4d0 (slot +0xc0): the spare dword at +0x4d0. The setter always
    #      returns 0, so its evidence is $w<i>; the getter's is its return, which the fixture seeds differently
    #      on all sixteen surfaces.
    "Surface::setField4d0": dict(
        module="jgld.dll", addr=0x0000A5D0, abi="thiscall", ret="int", args=["pointer", "int"],
        fixture=_F, state=_STATE,
        vectors=[("$s0", 0), ("$s0", 1), ("$s0", -1), ("$s0", 0x7FFFFFFF), ("$s0", -0x80000000),
                 ("$s1", 0x12345678), ("$s2", 0xABCDEF), ("$s3", 2), ("$s4", 3), ("$s5", 4),
                 ("$s9", 5), ("$s11", 6), ("$s12", 7), ("$s13", 8)]),
    "Surface::getField4d0": dict(
        module="jgld.dll", addr=0x0000A620, abi="thiscall", ret="int", args=["pointer"],
        fixture=_F, state=_STATE, vectors=[(s,) for s in _ALL]),

    # ---- getClip (slot +0xc8) and getBounds (slot +0xd0): four dwords copied out of this+0x44 and this+0x54.
    #      The destination rectangles start filled with a marker, so a slot that wrote nothing would read RED.
    "Surface::getClip": dict(
        module="jgld.dll", addr=0x0000A930, abi="thiscall", ret="void", args=["pointer", "pointer"],
        fixture=_F, state=_STATE,
        vectors=[(s, "$out%d" % (i % 4)) for i, s in enumerate(_ALL)] +
                [("$s0", "$out4"), ("$s10", "$out5")]),
    "Surface::getBounds": dict(
        module="jgld.dll", addr=0x0000A9D0, abi="thiscall", ret="void", args=["pointer", "pointer"],
        fixture=_F, state=_STATE,
        vectors=[(s, "$out%d" % (4 + i % 4)) for i, s in enumerate(_ALL)] +
                [("$s13", "$out0"), ("$s12", "$out1")]),

    # ---- the three "address of a field" slots: +0xcc returns this+0x44, +0xd4 this+0x54, +0xe4 this+0x24.
    #      Sixteen surfaces 0x820 bytes apart give sixteen distinct results.
    "Surface::getClipRectPtr": dict(
        module="jgld.dll", addr=0x0000A990, abi="thiscall", ret="pointer", args=["pointer"],
        fixture=_F, state=_STATE, vectors=[(s,) for s in _ALL]),
    "Surface::getBoundsRectPtr": dict(
        module="jgld.dll", addr=0x0000AA30, abi="thiscall", ret="pointer", args=["pointer"],
        fixture=_F, state=_STATE, vectors=[(s,) for s in _ALL]),
    "Surface::getDepthPtr": dict(
        module="jgld.dll", addr=0x0000AB30, abi="thiscall", ret="pointer", args=["pointer"],
        fixture=_F, state=_STATE, vectors=[(s,) for s in _ALL]),

    # ---- width (+0xd8) and height (+0xdc): the bounds rectangle's right minus left and bottom minus top. The
    #      fourteen surfaces give widths 32, 32, 19, 32, 16, 32, 32, 10, 8, 32, 32, 32, 24, 0 and heights
    #      16, 16, 11, 16, 10, 16, 16, 8, 8, 16, 16, 16, 12, 0 (s12's bounds start at (2, 1), s13's is empty).
    "Surface::width": dict(
        module="jgld.dll", addr=0x0000AA70, abi="thiscall", ret="int", args=["pointer"],
        fixture=_F, state=_STATE, vectors=[(s,) for s in _ALL]),
    "Surface::height": dict(
        module="jgld.dll", addr=0x0000AAB0, abi="thiscall", ret="int", args=["pointer"],
        fixture=_F, state=_STATE, vectors=[(s,) for s in _ALL]),

    # ---- getField40 (+0xe0, the row stride) and getField7c (+0xe8, the Palette pointer). The strides are 32,
    #      32, 19, 32, 16, 32, 32, 10, 8, 32, 32, 32, 24, 32; the palettes are the fixture's two objects on nine
    #      and three surfaces and NULL on s9 and s13.
    "Surface::getField40": dict(
        module="jgld.dll", addr=0x0000AAF0, abi="thiscall", ret="int", args=["pointer"],
        fixture=_F, state=_STATE, vectors=[(s,) for s in _ALL]),
    "Surface::getField7c": dict(
        module="jgld.dll", addr=0x0000AB70, abi="thiscall", ret="pointer", args=["pointer"],
        fixture=_F, state=_STATE, vectors=[(s,) for s in _ALL]),

    # ---- pixelAddr (slot +0x0c): the address of a pixel. Calls width (+0xd8), height (+0xdc) and the base
    #      slot (+0x10), then switches on the depth at +0x24. The vectors cover: inside, x == width, x > width,
    #      y == height, y > height, a NULL base (s9), each of the four depths with a case (8 on s0/s2/s12,
    #      16 on s3/s4, 24 on s7, 32 on s8), a depth with no case (12 on s6), and negative coordinates, which
    #      the body does not test for and which give an address below the base.
    "Surface::pixelAddr": dict(
        module="jgld.dll", addr=0x00008830, abi="thiscall", ret="pointer",
        args=["pointer", "int", "int"], fixture=_F, state=_STATE,
        vectors=[("$s0", 0, 0), ("$s0", 1, 0), ("$s0", 0, 1), ("$s0", 31, 15), ("$s0", 5, 7),
                 ("$s0", 32, 0),      # x == width: NULL
                 ("$s0", 40, 0),      # x > width: NULL
                 ("$s0", 0, 16),      # y == height: NULL
                 ("$s0", 0, 20),      # y > height: NULL
                 ("$s0", -1, 0),      # negative x: below the base
                 ("$s0", 0, -1),      # negative y: below the base
                 ("$s2", 18, 10),     # stride 19
                 ("$s12", 23, 11),    # stride 24, bounds starting at (2, 1)
                 ("$s12", 24, 0),     # x == width (24) although the bounds reach 26: NULL
                 ("$s3", 31, 15),     # depth 16
                 ("$s3", 1, 1),
                 ("$s4", 15, 9),      # depth 16, stride 16
                 ("$s7", 9, 7),       # depth 24: x * 3
                 ("$s7", 1, 1),
                 ("$s8", 7, 7),       # depth 32: x * 4
                 ("$s8", 1, 1),
                 ("$s6", 5, 5),       # depth 12: the switch has no case
                 ("$s9", 5, 5),       # no base: NULL
                 ("$s13", 0, 0),      # width and height both 0: NULL
                 ("$s10", 31, 15),    # clip wider than the bounds: the bounds still decide
                 ("$s11", 20, 10),
                 ("$s14", 5, 3),      # depth 48: above the range the switch accepts
                 ("$s15", 5, 3)]),    # depth 4: the subtraction wraps below zero

    # ---- the two slots that forward to the base slot (+0x10) with no argument (slots +0x18 and +0x20). Their
    #      observable effect is acquireDC2's writes to +0x4c8 / +0x4cc; eax at the `ret` is what that slot
    #      returned.
    "Surface::fwdVfn18": dict(
        module="jgld.dll", addr=0x0000A750, abi="thiscall", ret="pointer", args=["pointer"],
        fixture=_F, state=_STATE, vectors=[(s,) for s in _ALL]),
    "Surface::fwdVfn20": dict(
        module="jgld.dll", addr=0x0000A7B0, abi="thiscall", ret="pointer", args=["pointer"],
        fixture=_F, state=_STATE, vectors=[(s,) for s in _ALL]),

    # ---- the two slots that forward their two arguments to slot +0x0c (slots +0x14 and +0x1c). Same vectors
    #      as pixelAddr, so every arm of that function is reached through them too.
    "Surface::call3a": dict(
        module="jgld.dll", addr=0x0000A810, abi="thiscall", ret="pointer",
        args=["pointer", "int", "int"], fixture=_F, state=_STATE,
        vectors=[("$s0", 0, 0), ("$s0", 5, 7), ("$s0", 31, 15), ("$s0", 32, 0), ("$s0", 0, 16),
                 ("$s0", -1, -1), ("$s2", 18, 10), ("$s3", 31, 15), ("$s4", 15, 9), ("$s7", 9, 7),
                 ("$s8", 7, 7), ("$s6", 5, 5), ("$s9", 5, 5), ("$s13", 0, 0),
                 ("$s14", 5, 3), ("$s15", 5, 3)]),
    "Surface::call3b": dict(
        module="jgld.dll", addr=0x0000A870, abi="thiscall", ret="pointer",
        args=["pointer", "int", "int"], fixture=_F, state=_STATE,
        vectors=[("$s0", 1, 1), ("$s0", 7, 5), ("$s0", 30, 14), ("$s0", 40, 0), ("$s0", 0, 20),
                 ("$s0", -2, -3), ("$s2", 0, 0), ("$s3", 1, 1), ("$s4", 0, 9), ("$s7", 1, 1),
                 ("$s8", 1, 1), ("$s6", 0, 0), ("$s9", 0, 0), ("$s12", 23, 11),
                 ("$s14", 1, 1), ("$s15", 1, 1)]),

    # ---- callBounds (slot +0xc4): hands the bounds rectangle to slot +0x34 (setClip 0x000083c0), which
    #      intersects it with itself into the clip rectangle at +0x44. Only the ten surfaces whose stored
    #      device context is NULL are used, so setClip's GDI block never runs (see the fixture header).
    #      s1, s11 and s10 have a clip rectangle that differs from the bounds, so the write is visible; s13's
    #      bounds are empty, so the intersection fails and setClip returns 1 instead of 0.
    "Surface::callBounds": dict(
        module="jgld.dll", addr=0x0000A8D0, abi="thiscall", ret="int", args=["pointer"],
        fixture=_F, state=_STATE, vectors=[(s,) for s in _NODC]),

    # ---- fillRect (slot +0x58): NULL rectangle -> 0x10; depth 8 -> fill8 0xf880; depth 16 -> fill16 0xafe0;
    #      any other depth -> 0x17. s10 is deliberately absent: its clip rectangle reaches past its own pixel
    #      slice, so a fill through it would write into the next surface's bytes.
    "Surface::fillRect": dict(
        module="jgld.dll", addr=0x00009320, abi="thiscall", ret="int",
        args=["pointer", "pointer", "int", "int"], fixture=_F, state=_STATE_PIX,
        vectors=[("$s0", "$r_full", 11, 0x41),      # depth 8, whole surface, a colour that occurs
                 ("$s0", "$r_in", 0x5A, 0x42),
                 ("$s0", "$r_1x1", 0xE4, 0x43),     # one pixel
                 ("$s0", "$r_neg", 0x30, 0x44),     # clipped at the top left
                 ("$s0", "$r_out", 0x50, 0x45),     # empty intersection
                 ("$s0", "$r_empty", 0x51, 0x46),
                 ("$s0", "$r_inv", 0x52, 0x47),
                 ("$s1", "$r_full", 0x20, 0x48),    # clipped to {4,2,20,12}
                 ("$s2", "$r_s19", 0x09, 0x49),     # stride 19
                 ("$s12", "$r_s12", 0x0C, 0x4A),    # bounds starting at (2, 1)
                 ("$s9", "$r_full", 0x25, 0x4B),    # no base
                 ("$s3", "$r_full", 0x11, 0x4C),    # depth 16 -> fill16
                 ("$s3", "$r_in", 0x12, 0x4D),
                 ("$s4", "$r_s16", 0x13, 0x4E),     # depth 16, RGB555
                 ("$s11", "$r_in", 0x14, 0x4F),
                 ("$s5", "$r_full", 0x15, 0x50),    # depth 16, format code 2
                 ("$s6", "$r_full", 0x16, 0x51),    # depth 12 -> 0x17
                 ("$s7", "$r_s24", 0x17, 0x52),     # depth 24 -> 0x17
                 ("$s8", "$r_s32", 0x18, 0x53),     # depth 32 -> 0x17
                 ("$s13", "$r_full", 0x19, 0x54),   # empty bounds
                 ("$s0", 0, 0x1A, 0x55),            # NULL rectangle -> 0x10
                 ("$s3", 0, 0x1B, 0x56),            # NULL rectangle on a 16-bit surface -> 0x10
                 ("$s6", 0, 0x1C, 0x57),
                 ("$s14", "$r_s16", 0x1D, 0x58),   # depth 48 -> 0x17
                 ("$s15", "$r_s16", 0x1E, 0x59)]),  # depth 4 -> 0x17

    # ---- fill2 (slot +0x5c): depth 8 -> fill2_8 0xf9e0 with (left, top, right, bottom, table); any other
    #      depth returns 0 without reading the rectangle at all, which is why the three vectors with a NULL
    #      rectangle use 16-bit, 24-bit and 12-bit surfaces.
    "Surface::fill2": dict(
        module="jgld.dll", addr=0x00009440, abi="thiscall", ret="int",
        args=["pointer", "pointer", "pointer"], fixture=_F, state=_STATE_PIX,
        vectors=[("$s0", "$r_full", "$lut"),
                 ("$s0", "$r_in", "$lut"),
                 ("$s0", "$r_1x1", "$lut"),
                 ("$s0", "$r_neg", "$lut"),         # clipped at the top left
                 ("$s0", "$r_out", "$lut"),         # empty intersection
                 ("$s0", "$r_empty", "$lut"),
                 ("$s0", "$r_inv", "$lut"),
                 ("$s0", "$r_full", 0),             # NULL table -> fill2_8 returns 0x10
                 ("$s1", "$r_full", "$lut"),        # clipped to {4,2,20,12}
                 ("$s2", "$r_s19", "$lut"),         # stride 19
                 ("$s12", "$r_s12", "$lut"),
                 ("$s9", "$r_full", "$lut"),        # no base
                 ("$s13", "$r_full", "$lut"),       # empty bounds
                 ("$s3", "$r_full", "$lut"),        # depth 16 -> 0, nothing drawn
                 ("$s3", 0, "$lut"),                # depth 16 and a NULL rectangle: never read
                 ("$s7", 0, "$lut"),                # depth 24 and a NULL rectangle
                 ("$s6", 0, 0),                     # depth 12, NULL rectangle and NULL table
                 ("$s8", "$r_s32", "$lut"),
                 ("$s14", "$r_s16", "$lut"),       # depth 48 -> 0
                 ("$s15", 0, "$lut")]),            # depth 4, NULL rectangle: never read

    # ---- drawLine (slot +0x64): depth 8 -> line8 0xfb70; depth 16 -> line16 0xb140; any other depth draws
    #      nothing. The return is always 0, so the evidence is $pix. The sixth argument is never read: the two
    #      vectors that differ only in it must give the same pixels, which is what makes that fact a result.
    "Surface::drawLine": dict(
        module="jgld.dll", addr=0x000094E0, abi="thiscall", ret="int",
        args=["pointer", "int", "int", "int", "int", "int", "int"], fixture=_F, state=_STATE_PIX,
        vectors=[("$s0", 0, 0, 31, 15, 0xA1, 0),      # depth 8, shallow diagonal
                 ("$s0", 0, 0, 31, 15, 0xA1, 0x7FFFFFFF),   # same line, other sixth argument
                 ("$s0", 31, 15, 0, 0, 0xA2, 0),      # the other way round
                 ("$s0", 0, 15, 31, 0, 0xA3, 0),      # the other diagonal
                 ("$s0", 4, 4, 4, 12, 0xA4, 0),       # vertical
                 ("$s0", 4, 6, 20, 6, 0xA5, 0),       # horizontal
                 ("$s0", 5, 5, 5, 5, 0xA6, 0),        # one point
                 ("$s0", -10, -10, 40, 40, 0xA7, 0),  # clipped on every edge
                 ("$s0", 100, 100, 120, 120, 0xA8, 0),  # entirely outside the clip rectangle
                 ("$s1", 0, 0, 31, 15, 0xA9, 0),      # clipped to {4,2,20,12}
                 ("$s2", 0, 0, 18, 10, 0xAA, 0),      # stride 19
                 ("$s12", 2, 1, 25, 12, 0xAB, 0),
                 ("$s9", 0, 0, 31, 15, 0xAC, 0),      # no base
                 ("$s3", 0, 0, 31, 15, 0x11, 0),      # depth 16 -> line16
                 ("$s3", 0, 15, 31, 0, 0x12, 0),
                 ("$s3", 8, 0, 8, 15, 0x80001234, 0),  # bit 31 set: the palette lookup is skipped
                 ("$s4", 0, 0, 15, 9, 0x13, 0),       # depth 16, RGB555
                 ("$s11", 6, 4, 18, 10, 0x14, 0),
                 ("$s5", 0, 0, 31, 15, 0x15, 0),      # depth 16, format code 2
                 ("$s6", 0, 0, 31, 15, 0x16, 0),      # depth 12: no arm
                 ("$s7", 0, 0, 9, 7, 0x17, 0),        # depth 24: no arm
                 ("$s8", 0, 0, 7, 7, 0x18, 0),        # depth 32: no arm
                 ("$s13", 0, 0, 5, 5, 0x19, 0),
                 ("$s14", 0, 0, 15, 7, 0x1A, 0),   # depth 48: no arm
                 ("$s15", 0, 0, 15, 7, 0x1B, 0)]),  # depth 4: no arm

    # ---- ditherRectDispatch (slot +0x4c): depth 8 -> ditherRectC8 0x11f80; depth 16 -> ditherRect16 0xe580;
    #      any other depth -> 0x17. A NULL rectangle is not tested here: the primitives take the bounds
    #      rectangle instead, which the last three vectors drive.
    "Surface::ditherRectDispatch": dict(
        module="jgld.dll", addr=0x00009C50, abi="thiscall", ret="int",
        args=["pointer", "pointer", "int"], fixture=_F, state=_STATE_PIX,
        vectors=[("$s0", "$r_full", 0x31),       # depth 8, even width and height
                 ("$s0", "$r_odd", 0x32),        # odd width and height
                 ("$s0", "$r_5x3", 0x33),
                 ("$s0", "$r_7x5", 0x34),
                 ("$s0", "$r_w32h15", 0x35),     # even width, odd height
                 ("$s0", "$r_1x1", 0x36),
                 ("$s0", "$r_out", 0x37),        # empty intersection
                 ("$s0", "$r_empty", 0x38),
                 ("$s0", "$r_inv", 0x39),
                 ("$s0", "$r_neg", 0x3A),        # clipped at the top left
                 ("$s1", "$r_full", 0x3B),       # clipped to {4,2,20,12}
                 ("$s2", "$r_s19", 0x3C),        # stride 19
                 ("$s12", "$r_s12", 0x3D),
                 ("$s9", "$r_full", 0x3E),       # no base
                 ("$s0", 0, 0x3F),               # NULL rectangle: the primitive takes the bounds
                 ("$s3", "$r_full", 0x21),       # depth 16 -> ditherRect16
                 ("$s3", "$r_in", 0x80004321),   # bit 31 set: the palette lookup is skipped
                 ("$s4", "$r_s16", 0x22),        # depth 16, RGB555
                 ("$s11", "$r_in", 0x23),
                 ("$s3", 0, 0x24),               # NULL rectangle on a 16-bit surface
                 ("$s5", "$r_full", 0x25),       # depth 16, format code 2
                 ("$s6", "$r_full", 0x26),       # depth 12 -> 0x17
                 ("$s7", "$r_s24", 0x27),        # depth 24 -> 0x17
                 ("$s8", "$r_s32", 0x28),        # depth 32 -> 0x17
                 ("$s13", "$r_full", 0x29),
                 ("$s14", "$r_s16", 0x2A),         # depth 48 -> 0x17
                 ("$s15", "$r_s16", 0x2B)]),       # depth 4 -> 0x17
}
