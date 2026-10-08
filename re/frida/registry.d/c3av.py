# Batch c3av: nine 8-bit drawing primitives of jgld.dll's Surface (module-aware hooks; addr is the RVA,
# VA = 0x10000000 + addr). Every surface, pixel buffer, rectangle and lookup table the vectors touch is allocated by
# the fixture (re/frida/js/fixtures.d/c3av.js) on jgld's own Surface vtable; nothing live (the Display at
# 0x10128420, a screen surface, DirectDraw) is passed in or written.
#
# State: $pix is the single block holding all eight pixel buffers, so one region says which surface changed;
# $l0..$l7 are the per-surface lock windows (surface+0x4c0: base, use count, current base) so a missing
# slot-+0x24 release reads RED.
#
# The module global at VA 0x1012847c, in which fillRectC8 0x11dd0 parks its row skip between setting it up
# (0x10011eb2) and re-reading it once per row (0x10011ee6), is deliberately NOT a state region: it belongs to the
# running game. Measured at the menu with log/c3/alt/c3av_probe.py (two boots, 5 s and 10 s): the game's main
# thread enters fillRectC8 8_000 times a second and the global flips between 0 and 420 about 43 times a second,
# with no A/B running. Snapshotting it would compare a value the game rewrites between the two arms. What the
# skip is set to is still verified, indirectly and deterministically, by the multi-row vectors: a wrong skip puts
# the second and later rows at the wrong offsets in $pix.

_PIX = ("$pix", 0, 0x2800)
_LOCKS = [(f"$l{i}", 0, 0x14) for i in range(8)]
_STATE = [_PIX] + _LOCKS
_STATE_SKIP = _STATE

_F = "c3av_surfaces"
# clear8 and fillRectC8 are the two the running game calls itself: they run with its render thread held still.
_FZ = "c3av_surfaces_frozen"

HOOKS = {
    # ---- hline / vline: one straight run of a byte, clipped to the clip rectangle of slot +0xcc. s1's clip
    #      {4,2,20,12} sits inside the bounds {0,0,32,16}, so every clamp and every early return is reachable;
    #      s4 (no pixel base), s6 (depth 12) and s7 (clip wider than the surface) make the address come back NULL.
    "Surface::hline": dict(
        module="jgld.dll", addr=0x000103F0, abi="thiscall", ret="void",
        args=["pointer", "int", "int", "int", "int"], fixture=_F, state=_STATE,
        vectors=[("$s1", 6, 18, 5, 0xA1),     # inside the clip rectangle
                 ("$s1", 6, 18, 1, 0xA2),     # y < top
                 ("$s1", 6, 18, 12, 0xA3),    # y >= bottom
                 ("$s1", 9, 9, 6, 0xA4),      # x1 == x2
                 ("$s1", 18, 6, 7, 0xA5),     # x1 > x2: the three-xor swap
                 ("$s1", 22, 28, 8, 0xA6),    # x1 >= right
                 ("$s1", 0, 3, 9, 0xA7),      # x2 < left
                 ("$s1", 0, 15, 10, 0xA8),    # x1 raised to left
                 ("$s1", 10, 31, 11, 0xA9),   # x2 lowered to right - 1
                 ("$s1", -5, 40, 4, 0xAA),    # both clamps
                 ("$s4", 6, 18, 5, 0xAB),     # NULL pixel address (no base)
                 ("$s7", 10, 40, 20, 0xAC),   # inside the wide clip, past the surface height
                 ("$s6", 0, 20, 5, 0xAD),     # depth 12: the address switch has no case
                 ("$s0", 0, 31, 0, 0xAE),     # first row, full width
                 ("$s0", 0, 31, 15, 0xAF),    # last row
                 ("$s2", 0, 18, 5, 0xB0),     # stride 19
                 ("$s5", 0, 15, 9, 0xB1),     # depth 16: the address scales by two
                 ("$s3", 0, 31, 8, 0xB2)]),   # clip inset by one on every edge
    "Surface::vline": dict(
        module="jgld.dll", addr=0x00010620, abi="thiscall", ret="void",
        args=["pointer", "int", "int", "int", "int"], fixture=_F, state=_STATE,
        vectors=[("$s1", 10, 4, 10, 0xC1),
                 ("$s1", 2, 4, 10, 0xC2),     # x < left
                 ("$s1", 25, 4, 10, 0xC3),    # x >= right
                 ("$s1", 10, 6, 6, 0xC4),     # y1 == y2
                 ("$s1", 11, 10, 4, 0xC5),    # y1 > y2: swap
                 ("$s1", 12, 13, 20, 0xC6),   # y1 >= bottom
                 ("$s1", 13, -5, 1, 0xC7),    # y2 < top
                 ("$s1", 14, 0, 8, 0xC8),     # y1 raised to top
                 ("$s1", 15, 4, 20, 0xC9),    # y2 lowered to bottom - 1
                 ("$s1", 16, -3, 30, 0xCA),   # both clamps
                 ("$s4", 10, 4, 10, 0xCB),    # NULL pixel address
                 ("$s7", 40, 0, 10, 0xCC),    # inside the wide clip, past the surface width
                 ("$s6", 10, 0, 10, 0xCD),    # depth 12
                 ("$s0", 0, 0, 15, 0xCE),
                 ("$s0", 31, 0, 15, 0xCF),
                 ("$s2", 18, 0, 10, 0xD0),    # stride 19
                 ("$s5", 15, 0, 9, 0xD1),     # depth 16
                 ("$s3", 16, 0, 15, 0xD2)]),

    # ---- clear8: the whole surface. s0/s2/s5/s6 have clip == bounds and take the `rep stosd` path (s2's width 19
    #      exercises the (w & 3) -> +1 dword rounding); s1/s3/s8 differ and hand the work to slot +0x44, which
    #      reaches fillRectC8.
    #      s4 is excluded on purpose: the fast path writes through the base with no null test.
    #      s7 is excluded too: its clip {0,0,64,40} is wider than its stride 32, so the slot-+0x44 path would park
    #      a NEGATIVE row skip (-32) in the global at 0x1012847c, and the game's render thread — inside
    #      fillRectC8 8_000 times a second — would then walk its own cursor BACKWARDS out of its drawing buffer.
    #      s8 gives the same "clip wider than the bounds" shape with stride 64, so its skip is 0. Every skip these
    #      vectors park is in [0, 31], under the 420 the game parks itself, so a game row can only come out short.
    "Surface::clear8": dict(
        module="jgld.dll", addr=0x00011C40, abi="thiscall", ret="void", args=["pointer", "int"],
        fixture=_FZ, state=_STATE_SKIP,
        vectors=[("$s0", 0x11), ("$s0", 0x00), ("$s0", 0xFF),
                 ("$s2", 0x22), ("$s2", 0x7F),      # width 19: w & 3 = 3
                 ("$s5", 0x33), ("$s6", 0x44),
                 ("$s1", 0x55), ("$s1", 0xAA),      # clip != bounds: slot +0x44
                 ("$s3", 0x66), ("$s8", 0x77),      # clip wider than the bounds, skip 0
                 ("$s0", 0x1234)]),                 # only the low byte is replicated

    # ---- fillRectC8: solid rectangle. The rectangle widths cover every (w >> 2, w & 3) combination; r_out /
    #      r_empty / r_inv make IntersectRect fail; a null rectangle goes to clear8.
    "Surface::fillRectC8": dict(
        module="jgld.dll", addr=0x00011DD0, abi="thiscall", ret="int", args=["pointer", "pointer", "int"],
        fixture=_FZ, state=_STATE_SKIP,
        vectors=[("$s0", "$r_full", 0x11),     # w 32: 8 dwords, 0 bytes
                 ("$s0", "$r_in", 0x12),       # w 16
                 ("$s0", "$r_odd", 0x13),      # w 11: 2 dwords, 3 bytes
                 ("$s0", "$r_even", 0x14),     # w 12: 3 dwords, 0 bytes
                 ("$s0", "$r_1x1", 0x15),      # w 1: 0 dwords, 1 byte
                 ("$s0", "$r_5x3", 0x16),      # w 5: 1 dword, 1 byte
                 ("$s0", "$r_7x5", 0x17),      # w 7: 1 dword, 3 bytes
                 ("$s0", "$r_out", 0x18),      # empty intersection
                 ("$s0", "$r_empty", 0x19),    # zero area
                 ("$s0", "$r_inv", 0x1A),      # inverted rectangle
                 ("$s0", "$r_neg", 0x1B),      # clipped at the top-left
                 ("$s1", "$r_full", 0x1C),     # clipped to {4,2,20,12}
                 ("$s2", "$r_s19", 0x1D),      # stride 19
                 ("$s5", "$r_s16", 0x1E),      # depth 16
                 ("$s6", "$r_full", 0x1F),     # depth 12: NULL address
                 ("$s4", "$r_full", 0x20),     # no base: NULL address
                 ("$s7", "$r_nullx", 0x21),    # inside the wide clip, off the surface
                 ("$s0", 0, 0x22),             # null rectangle -> clear8 (fast path)
                 ("$s1", 0, 0x23),             # null rectangle -> clear8 (slot +0x44 path)
                 ("$s3", "$r_w32h15", 0x24)]),

    # ---- ditherRectC8: every second pixel, parity alternating per row. The four (w parity, h parity) pairs are
    #      covered by r_full (even, even), r_odd (odd, odd), r_w32h15 (even, odd) and r_7x5 (odd, odd with h 5);
    #      a null rectangle takes the bounds rectangle of slot +0xd4.
    "Surface::ditherRectC8": dict(
        module="jgld.dll", addr=0x00011F80, abi="thiscall", ret="int", args=["pointer", "pointer", "int"],
        fixture=_F, state=_STATE,
        vectors=[("$s0", "$r_full", 0x31),     # w 32 even, h 16 even
                 ("$s0", "$r_odd", 0x32),      # w 11 odd, h 9 odd
                 ("$s0", "$r_even", 0x33),     # w 12 even, h 8 even
                 ("$s0", "$r_5x3", 0x34),      # w 5 odd, h 3 odd
                 ("$s0", "$r_7x5", 0x35),      # w 7 odd, h 5 odd
                 ("$s0", "$r_w32h15", 0x36),   # w 32 even, h 15 odd
                 ("$s0", "$r_1x1", 0x37),      # w 1, h 1: w >> 1 is 0 and the odd row writes nothing at all
                 ("$s0", "$r_1x4", 0x3F),      # w 1, h 4: w >> 1 is 0, the even rows write one byte each
                 ("$s0", "$r_in", 0x38),       # w 16 even, h 10 even
                 ("$s0", "$r_out", 0x39),      # empty intersection
                 ("$s0", "$r_empty", 0x3A),
                 ("$s0", "$r_inv", 0x3B),
                 ("$s0", "$r_neg", 0x3C),      # clipped at the top-left
                 ("$s1", "$r_full", 0x3D),
                 ("$s2", "$r_s19", 0x3E),      # stride 19, width 19 odd
                 ("$s5", "$r_s16", 0x3F),      # depth 16
                 ("$s6", "$r_full", 0x40),     # depth 12: NULL address
                 ("$s4", "$r_full", 0x41),     # no base: NULL address
                 ("$s7", "$r_nullx", 0x42),    # off the surface
                 ("$s0", 0, 0x43),             # null rectangle -> bounds
                 ("$s1", 0, 0x44),             # null rectangle -> bounds, then clipped
                 ("$s3", 0, 0x45)]),

    # ---- fill8: replace one byte value with another inside a rectangle. The surface is pre-filled with the
    #      pattern (i * 37 + 11) & 0xff, which is a permutation of 0..255 over any 256 consecutive bytes, so every
    #      `from` below matches some pixels and misses others: the per-pixel compare takes both sides.
    "Surface::fill8": dict(
        module="jgld.dll", addr=0x0000F880, abi="thiscall", ret="int",
        args=["pointer", "int", "int", "int", "int", "int", "int"], fixture=_F, state=_STATE,
        vectors=[("$s0", 0, 0, 32, 16, 11, 0x90),
                 ("$s0", 0, 0, 32, 16, 0x5A, 0x91),
                 ("$s0", 4, 2, 10, 6, 0x0C, 0x92),
                 ("$s0", 4, 2, 10, 6, 0xFF, 0x93),       # no pixel matches: the compare's miss side only
                 ("$s0", -5, -5, 10, 10, 0x30, 0x94),    # clipped at the top-left
                 ("$s0", 28, 12, 10, 10, 0x37, 0x95),    # clipped at the bottom-right
                 ("$s0", 100, 100, 5, 5, 0x50, 0x96),    # empty intersection -> 0
                 ("$s0", 0, 0, 0, 0, 0x60, 0x97),        # zero size -> 0
                 ("$s0", 0, 0, -4, -4, 0x61, 0x98),      # negative size -> 0
                 ("$s1", 0, 0, 32, 16, 0x20, 0x99),      # clipped to {4,2,20,12}
                 ("$s2", 0, 0, 19, 11, 0x09, 0x9A),      # stride 19
                 ("$s3", 1, 1, 30, 14, 0x22, 0x9B),
                 ("$s5", 0, 0, 16, 10, 0x23, 0x9C),      # depth 16
                 ("$s6", 0, 0, 32, 16, 0x24, 0x9D),      # depth 12: NULL -> 7
                 ("$s4", 0, 0, 32, 16, 0x25, 0x9E),      # no base: NULL -> 7
                 ("$s7", 33, 2, 5, 5, 0x26, 0x9F),       # off the surface: NULL -> 7
                 ("$s0", 5, 5, 1, 1, 0xE4, 0xA0),        # one pixel, and it matches
                 ("$s0", 0, 0, 32, 16, 0x0B, 0x0B)]),    # from == to: matches, writes the same byte back

    # ---- fill2_8: map every pixel through a 256-byte table. $lut is a permutation, $lut2 folds 256 values onto
    #      32, so the two give different surfaces from the same rectangle. A null table returns 0x10 before
    #      anything else runs; this is the only primitive here that takes the pixel address from slot +0x0c.
    "Surface::fill2_8": dict(
        module="jgld.dll", addr=0x0000F9E0, abi="thiscall", ret="int",
        args=["pointer", "int", "int", "int", "int", "pointer"], fixture=_F, state=_STATE,
        vectors=[("$s0", 0, 0, 32, 16, "$lut"),
                 ("$s0", 0, 0, 32, 16, "$lut2"),
                 ("$s0", 4, 2, 20, 12, "$lut"),
                 ("$s0", -5, -5, 8, 6, "$lut2"),         # clipped at the top-left
                 ("$s0", 20, 10, 40, 30, "$lut"),        # clipped at the bottom-right
                 ("$s0", 5, 5, 6, 6, "$lut"),            # one pixel
                 ("$s0", 0, 0, 32, 16, 0),               # null table -> 0x10
                 ("$s1", 0, 0, 32, 16, 0),               # null table -> 0x10, other surface
                 ("$s0", 100, 100, 110, 110, "$lut"),    # empty intersection -> 0
                 ("$s0", 10, 10, 10, 10, "$lut"),        # zero area -> 0
                 ("$s0", 20, 20, 5, 5, "$lut"),          # inverted -> 0
                 ("$s1", 0, 0, 32, 16, "$lut"),
                 ("$s2", 0, 0, 19, 11, "$lut2"),         # stride 19
                 ("$s3", 1, 1, 31, 15, "$lut"),
                 ("$s5", 0, 0, 16, 10, "$lut"),          # depth 16
                 ("$s6", 0, 0, 32, 16, "$lut"),          # depth 12: NULL -> 7
                 ("$s4", 0, 0, 32, 16, "$lut"),          # no base: NULL -> 7
                 ("$s7", 33, 2, 40, 8, "$lut")]),        # off the surface: NULL -> 7

    # ---- the two dashed primitives: len1 pixels of c1 then len2 of c2, from a phase. -1 for a colour leaves that
    #      half of the pattern untouched and selects one of the three inline loops; both -1 returns at once. The
    #      swap also swaps the two colours, so a vector with x1 > x2 and c1 == -1 runs the c2 == -1 loop.
    "Surface::dashedHLine8": dict(
        module="jgld.dll", addr=0x000114E0, abi="thiscall", ret="void",
        args=["pointer", "int", "int", "int", "int", "int", "int", "int", "int"], fixture=_F, state=_STATE,
        vectors=[("$s1", 4, 19, 3, 0x11, 0x22, 3, 2, 0),     # both colours, phase < len1
                 ("$s1", 4, 19, 4, 0x11, 0x22, 3, 2, 4),     # phase >= len1
                 ("$s1", 4, 19, 5, -1, 0x22, 3, 2, 1),       # c1 == -1 loop
                 ("$s1", 4, 19, 6, 0x11, -1, 3, 2, 1),       # c2 == -1 loop
                 ("$s1", 19, 4, 7, 0x11, 0x22, 3, 2, 1),     # swap
                 ("$s1", 19, 4, 8, -1, 0x22, 3, 2, 1),       # swap turns c1 == -1 into the c2 == -1 loop
                 ("$s1", 4, 19, 1, 0x11, 0x22, 3, 2, 0),     # y < top
                 ("$s1", 4, 19, 12, 0x11, 0x22, 3, 2, 0),    # y >= bottom
                 ("$s1", 9, 9, 5, 0x11, 0x22, 3, 2, 0),      # x1 == x2
                 ("$s1", 4, 19, 5, -1, -1, 3, 2, 0),         # both colours -1
                 ("$s1", 22, 28, 5, 0x11, 0x22, 3, 2, 0),    # x1 >= right
                 ("$s1", 0, 3, 5, 0x11, 0x22, 3, 2, 0),      # x2 < left
                 ("$s1", 0, 31, 9, 0x11, 0x22, 4, 4, 2),     # both clamps
                 ("$s4", 4, 19, 5, 0x11, 0x22, 3, 2, 0),     # NULL pixel address
                 ("$s6", 4, 19, 5, 0x11, 0x22, 3, 2, 0),     # depth 12
                 ("$s0", 0, 31, 0, 0x33, 0x44, 1, 1, 0),     # single-pixel runs
                 ("$s0", 0, 31, 2, 0x33, 0x44, 5, 3, 7),
                 ("$s0", 0, 31, 4, 0x33, 0x44, 5, 3, -3),    # negative phase (signed idiv)
                 ("$s2", 0, 18, 5, 0x55, 0x66, 2, 3, 4),     # stride 19
                 ("$s5", 0, 15, 3, 0x77, 0x88, 2, 2, 1),     # depth 16
                 ("$s0", 0, 31, 6, 0x33, 0x44, 7, 1, 0),
                 ("$s0", 0, 31, 8, -1, 0x44, 2, 6, 3)]),     # c1 == -1 with phase >= len1
    "Surface::dashedVLine8": dict(
        module="jgld.dll", addr=0x00011880, abi="thiscall", ret="void",
        args=["pointer", "int", "int", "int", "int", "int", "int", "int", "int"], fixture=_F, state=_STATE,
        vectors=[("$s1", 6, 2, 11, 0x11, 0x22, 3, 2, 0),
                 ("$s1", 7, 2, 11, 0x11, 0x22, 3, 2, 4),     # phase >= len1
                 ("$s1", 8, 2, 11, -1, 0x22, 3, 2, 1),       # c1 == -1 loop
                 ("$s1", 9, 2, 11, 0x11, -1, 3, 2, 1),       # c2 == -1 loop
                 ("$s1", 10, 11, 2, 0x11, 0x22, 3, 2, 1),    # swap
                 ("$s1", 11, 11, 2, -1, 0x22, 3, 2, 1),      # swap turns c1 == -1 into the c2 == -1 loop
                 ("$s1", 2, 2, 11, 0x11, 0x22, 3, 2, 0),     # x < left
                 ("$s1", 25, 2, 11, 0x11, 0x22, 3, 2, 0),    # x >= right
                 ("$s1", 12, 5, 5, 0x11, 0x22, 3, 2, 0),     # y1 == y2
                 ("$s1", 12, 2, 11, -1, -1, 3, 2, 0),        # both colours -1
                 ("$s1", 13, 13, 20, 0x11, 0x22, 3, 2, 0),   # y1 >= bottom
                 ("$s1", 14, -5, 1, 0x11, 0x22, 3, 2, 0),    # y2 < top
                 ("$s1", 15, -4, 20, 0x11, 0x22, 4, 4, 2),   # both clamps
                 ("$s4", 6, 2, 11, 0x11, 0x22, 3, 2, 0),     # NULL pixel address
                 ("$s6", 6, 2, 11, 0x11, 0x22, 3, 2, 0),     # depth 12
                 ("$s0", 0, 0, 15, 0x33, 0x44, 1, 1, 0),
                 ("$s0", 2, 0, 15, 0x33, 0x44, 5, 3, 7),
                 ("$s0", 4, 0, 15, 0x33, 0x44, 5, 3, -3),    # negative phase
                 ("$s2", 10, 0, 10, 0x55, 0x66, 2, 3, 4),    # stride 19
                 ("$s5", 5, 0, 9, 0x77, 0x88, 2, 2, 1),      # depth 16
                 ("$s0", 6, 0, 15, 0x33, 0x44, 7, 1, 0),
                 ("$s0", 8, 0, 15, -1, 0x44, 2, 6, 3)]),
}
