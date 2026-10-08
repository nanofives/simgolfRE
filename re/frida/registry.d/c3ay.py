# Batch c3ay: six 16-bit drawing primitives of jgld.dll's Surface (module-aware hooks; addr is the RVA,
# VA = 0x10000000 + addr). Every surface, palette, pixel buffer and rectangle the vectors touch is allocated by
# the fixture (re/frida/js/fixtures.d/c3ay.js) on jgld's own Surface (0x1011d0b0) and Palette (0x1011db10)
# vtables; nothing live (the Display at 0x10128420, a screen surface, the module's own Palette, DirectDraw) is
# passed in or written.
#
# These are the 16-bit twins batch c3av left alone because every one of them opens with a palette lookup
# (log/c3/c3av_notes.txt, "The 16-bit twins"). The fixture supplies that Palette, so both arms of the lookup
# table switch (RGB565 through Palette slot +0x18, RGB555 through slot +0x1c) and the surface-palette /
# global-fallback / no-palette / unsupported-format arms are all driven from our own memory.
#
# State: $pix is the single block holding all ten pixel buffers, so one region says which surface changed;
# $l0..$l9 are the per-surface lock windows (surface+0x4c0: base, use count, current base) so a missing
# slot-+0x24 release reads RED.
#
# Two module globals are deliberately NOT state regions, for the reason c3av recorded for 0x1012847c: they
# belong to the module, not to a vector. 0x10128458 is fillRect16's row skip (written at VA 0x1000deaf and never
# read back by anything in these six bodies) and 0x10128450 is the pixel count dashedHLine16 parks so it can use
# ebp as its loop counter (written at VA 0x1000d380, read back at 0x1000d38b). Both values are verified
# indirectly and deterministically by the multi-row and multi-pixel vectors: a wrong skip or count puts the
# written words at the wrong offsets in $pix.

_PIX = ("$pix", 0, 0x7000)
_LOCKS = [(f"$l{i}", 0, 0x14) for i in range(14)]
_STATE = [_PIX] + _LOCKS

_F = "c3ay_surfaces"

HOOKS = {
    # ---- clear16: the whole surface in one colour.
    #      Colour block: $s0/$s9 take the RGB565 table, $s2 the RGB555 one, $s3's format code 2 aborts the call,
    #      $s5 has no palette of its own so the module global 0x1012873c decides, and a colour with bit 31 set
    #      skips the lookup altogether (and therefore also skips $s3's abort).
    #      Fill: $s0/$s2/$s6/$s7/$s9 have clip == bounds and take the fillWords path; $s1 and $s8 differ and hand
    #      the work to the depth dispatcher of slot +0x44, which forwards a 16-bit surface to fillRect16.
    #      $s4 is excluded on purpose: the fillWords path takes the base from slot +0x20 with NO null test
    #      (VA 0x1000dbdd straight into 0x1000dc35), exactly as c3av excluded it from clear8.
    "Surface::clear16": dict(
        module="jgld.dll", addr=0x0000DA70, abi="thiscall", ret="void", args=["pointer", "uint32"],
        fixture=_F, state=_STATE,
        vectors=[("$s0", 0x11), ("$s0", 0x00), ("$s0", 0xFF),
                 ("$s0", 0x1234),               # only the low byte indexes the table
                 ("$s0", 0x80001234),           # bit 31 set: no lookup, the low 16 bits are the colour
                 ("$s0", 0x8000FFFF),
                 ("$s2", 0x22), ("$s2", 0x7F),  # RGB555 table, stride 19
                 ("$s2", 0x80000ABC),
                 ("$s9", 0x11), ("$s9", 0x55),  # the second palette: same index, different words
                 ("$s5", 0x44),                 # no surface palette: the global fallback
                 ("$s3", 0x33),                 # format code 2: returns with nothing drawn
                 ("$s3", 0x80004444),           # format code 2 but bit 31 set: the switch is never reached
                 ("$s1", 0x66),                 # clip != bounds: slot +0x44 -> fillRect16
                 ("$s1", 0x8000AAAA),
                 ("$s8", 0x77),                 # clip wider than the bounds: slot +0x44, skip 0
                 ("$s6", 0x88),                 # depth 8 surface, clip == bounds
                 ("$s7", 0x99)]),               # depth 12 surface, clip == bounds

    # ---- fillRect16: one rectangle. A null rectangle goes to clear16. The rectangle widths cover w even, w odd
    #      and w == 1; r_out / r_empty / r_inv make IntersectRect fail; $s4 and $s7 make the pixel address NULL.
    "Surface::fillRect16": dict(
        module="jgld.dll", addr=0x0000DCF0, abi="thiscall", ret="int", args=["pointer", "pointer", "uint32"],
        fixture=_F, state=_STATE,
        vectors=[("$s0", "$r_full", 0x11),       # w 32, h 16
                 ("$s0", "$r_in", 0x12),         # w 16, h 10
                 ("$s0", "$r_odd", 0x13),        # w 11, h 9
                 ("$s0", "$r_even", 0x14),       # w 12, h 8
                 ("$s0", "$r_1x1", 0x15),        # w 1, h 1
                 ("$s0", "$r_5x3", 0x16),
                 ("$s0", "$r_7x5", 0x17),
                 ("$s0", "$r_neg", 0x18),        # clipped at the top-left
                 ("$s0", "$r_out", 0x19),        # empty intersection -> 0
                 ("$s0", "$r_empty", 0x1A),      # zero area -> 0
                 ("$s0", "$r_inv", 0x1B),        # inverted rectangle -> 0
                 ("$s0", "$r_full", 0x8000ABCD), # bit 31 set: no lookup
                 ("$s2", "$r_s19", 0x1C),        # RGB555, stride 19
                 ("$s9", "$r_full", 0x11),       # second palette, same index as the first vector
                 ("$s5", "$r_full", 0x1D),       # no surface palette: the global fallback
                 ("$s3", "$r_full", 0x1E),       # format code 2 -> 24
                 ("$s3", "$r_full", 0x80001111), # format code 2 but bit 31 set: fills
                 ("$s4", "$r_full", 0x1F),       # no base: NULL pixel address -> 0
                 ("$s7", "$r_full", 0x20),       # depth 12: NULL pixel address -> 0
                 ("$s6", "$r_in", 0x21),         # depth 8 surface
                 ("$s1", "$r_full", 0x22),       # clipped to {4,2,20,12}
                 ("$s8", "$r_offsurf", 0x23),    # inside the wide clip, past the surface width -> NULL
                 ("$s0", 0, 0x24),               # null rectangle -> clear16 (fillWords path)
                 ("$s1", 0, 0x25)]),             # null rectangle -> clear16 -> slot +0x44 -> fillRect16

    # ---- ditherRect16: every second pixel, parity alternating per row. The four (w parity, h parity) pairs are
    #      covered by r_full (even, even), r_odd (odd, odd), r_w32h15 (even, odd) and r_7x5 (odd, odd with h 5);
    #      r_1x1 and r_1x4 drive w >> 1 == 0; a null rectangle takes the bounds rectangle of slot +0xd4.
    "Surface::ditherRect16": dict(
        module="jgld.dll", addr=0x0000E580, abi="thiscall", ret="int", args=["pointer", "pointer", "uint32"],
        fixture=_F, state=_STATE,
        vectors=[("$s0", "$r_full", 0x31),       # w 32 even, h 16 even
                 ("$s0", "$r_odd", 0x32),        # w 11 odd, h 9 odd
                 ("$s0", "$r_even", 0x33),       # w 12 even, h 8 even
                 ("$s0", "$r_5x3", 0x34),        # w 5 odd, h 3 odd
                 ("$s0", "$r_7x5", 0x35),        # w 7 odd, h 5 odd
                 ("$s0", "$r_w32h15", 0x36),     # w 32 even, h 15 odd
                 ("$s0", "$r_1x1", 0x37),        # w 1, h 1: the inner loop writes nothing
                 ("$s0", "$r_1x4", 0x38),        # w 1, h 4: the even rows write one word each
                 ("$s0", "$r_in", 0x39),
                 ("$s0", "$r_neg", 0x3A),        # clipped at the top-left
                 ("$s0", "$r_out", 0x3B),        # empty intersection
                 ("$s0", "$r_empty", 0x3C),
                 ("$s0", "$r_inv", 0x3D),
                 ("$s0", "$r_full", 0x80003333), # bit 31 set: no lookup
                 ("$s2", "$r_s19", 0x3E),        # RGB555, stride 19, width 19 odd
                 ("$s9", "$r_full", 0x31),       # second palette, same index as the first vector
                 ("$s5", "$r_full", 0x3F),       # no surface palette: the global fallback
                 ("$s3", "$r_full", 0x40),       # format code 2 -> 24, nothing drawn
                 ("$s3", "$r_full", 0x80005555), # format code 2 but bit 31 set: draws
                 ("$s4", "$r_full", 0x41),       # no base: NULL pixel address
                 ("$s7", "$r_full", 0x42),       # depth 12: NULL pixel address
                 ("$s6", "$r_in", 0x43),         # depth 8 surface
                 ("$s1", "$r_full", 0x44),       # clipped to {4,2,20,12}
                 ("$s8", "$r_offsurf", 0x45),    # past the surface width -> NULL
                 ("$s0", 0, 0x46),               # null rectangle -> the bounds rectangle
                 ("$s1", 0, 0x47),               # null rectangle -> bounds, then clipped
                 ("$s8", 0, 0x48)]),             # null rectangle -> bounds {0,0,16,8} clipped to {0,0,16,4}

    # ---- hline16: one horizontal run. $s1's clip {4,2,20,12} sits inside the bounds {0,0,32,16}, so every clamp
    #      and every early return is reachable; $s4, $s7 and $s8 make the pixel address NULL.
    "Surface::hline16": dict(
        module="jgld.dll", addr=0x0000BA90, abi="thiscall", ret="void",
        args=["pointer", "int", "int", "int", "uint32"], fixture=_F, state=_STATE,
        vectors=[("$s1", 6, 18, 5, 0xA1),        # inside the clip rectangle
                 ("$s1", 6, 18, 1, 0xA2),        # y < top
                 ("$s1", 6, 18, 12, 0xA3),       # y >= bottom
                 ("$s1", 9, 9, 6, 0xA4),         # x1 == x2
                 ("$s1", 18, 6, 7, 0xA5),        # x1 > x2: the three-xor swap
                 ("$s1", 22, 28, 8, 0xA6),       # x1 >= right
                 ("$s1", 0, 3, 9, 0xA7),         # x2 < left
                 ("$s1", 0, 15, 10, 0xA8),       # x1 raised to left
                 ("$s1", 10, 31, 11, 0xA9),      # x2 lowered to right - 1
                 ("$s1", -5, 40, 4, 0xAA),       # both clamps
                 ("$s0", 0, 31, 0, 0xAB),        # first row, full width
                 ("$s0", 0, 31, 15, 0xAC),       # last row
                 ("$s0", 0, 31, 7, 0x8000BEEF),  # bit 31 set: no lookup
                 ("$s2", 0, 18, 5, 0xAD),        # RGB555, stride 19
                 ("$s9", 0, 31, 3, 0xAB),        # second palette, same index as the s0 vector
                 ("$s5", 0, 31, 2, 0xAE),        # no surface palette: the global fallback
                 ("$s3", 0, 31, 4, 0xAF),        # format code 2: returns
                 ("$s3", 0, 31, 6, 0x80001234),  # format code 2 but bit 31 set: draws
                 ("$s4", 6, 18, 5, 0xB0),        # no base: NULL pixel address
                 ("$s7", 0, 20, 5, 0xB1),        # depth 12: NULL pixel address
                 ("$s6", 0, 31, 9, 0xB2),        # depth 8 surface
                 ("$s8", 0, 15, 1, 0xB3),        # inside both the clip and the surface
                 ("$s8", 20, 30, 2, 0xB4)]),     # inside the wide clip, past the surface width -> NULL

    # ---- vline16: the vertical twin.
    "Surface::vline16": dict(
        module="jgld.dll", addr=0x0000BDE0, abi="thiscall", ret="void",
        args=["pointer", "int", "int", "int", "uint32"], fixture=_F, state=_STATE,
        vectors=[("$s1", 10, 4, 10, 0xC1),
                 ("$s1", 2, 4, 10, 0xC2),        # x < left
                 ("$s1", 25, 4, 10, 0xC3),       # x >= right
                 ("$s1", 10, 6, 6, 0xC4),        # y1 == y2
                 ("$s1", 11, 10, 4, 0xC5),       # y1 > y2: the swap
                 ("$s1", 12, 13, 20, 0xC6),      # y1 >= bottom
                 ("$s1", 13, -5, 1, 0xC7),       # y2 < top
                 ("$s1", 14, 0, 8, 0xC8),        # y1 raised to top
                 ("$s1", 15, 4, 20, 0xC9),       # y2 lowered to bottom - 1
                 ("$s1", 16, -3, 30, 0xCA),      # both clamps
                 ("$s0", 0, 0, 15, 0xCB),        # first column, full height
                 ("$s0", 31, 0, 15, 0xCC),       # last column
                 ("$s0", 5, 0, 15, 0x8000CAFE),  # bit 31 set: no lookup
                 ("$s2", 18, 0, 10, 0xCD),       # RGB555, stride 19
                 ("$s9", 0, 0, 15, 0xCB),        # second palette, same index as the s0 vector
                 ("$s5", 3, 0, 15, 0xCE),        # no surface palette: the global fallback
                 ("$s3", 4, 0, 15, 0xCF),        # format code 2: returns
                 ("$s3", 6, 0, 15, 0x80002222),  # format code 2 but bit 31 set: draws
                 ("$s4", 10, 4, 10, 0xD0),       # no base: NULL pixel address
                 ("$s7", 10, 0, 10, 0xD1),       # depth 12: NULL pixel address
                 ("$s6", 7, 0, 15, 0xD2),        # depth 8 surface
                 ("$s8", 5, 0, 3, 0xD3),         # inside both the clip and the surface
                 ("$s8", 20, 0, 3, 0xD4)]),      # inside the wide clip, past the surface width -> NULL

    # ---- dashedHLine16: len1 pixels of c1 then len2 of c2, from a phase. Both colours -1 returns at once; a
    #      single -1 keeps bit 31 set so that colour is NOT looked up, and after the unconditional 16-bit masking
    #      at VA 0x1000d14a / 0x1000d155 it is 0xffff, so the loop writes it like any other colour (the two
    #      single-colour loops at 0x1000d2f2 and 0x1000d331 can never be selected: see log/c3/c3ay_purpose.md).
    #      len1 + len2 is never 0: the phase reduction is an `idiv` by it at 0x1000d2b2.
    "Surface::dashedHLine16": dict(
        module="jgld.dll", addr=0x0000CF90, abi="thiscall", ret="void",
        args=["pointer", "int", "int", "int", "uint32", "uint32", "int", "int", "int"], fixture=_F, state=_STATE,
        vectors=[("$s1", 4, 19, 3, 0x11, 0x22, 3, 2, 0),            # both colours, phase < len1
                 ("$s1", 4, 19, 4, 0x11, 0x22, 3, 2, 4),            # phase >= len1
                 ("$s1", 4, 19, 5, 0xFFFFFFFF, 0x22, 3, 2, 1),      # c1 == -1: not looked up, written as 0xffff
                 ("$s1", 4, 19, 6, 0x11, 0xFFFFFFFF, 3, 2, 1),      # c2 == -1
                 ("$s1", 19, 4, 7, 0x11, 0x22, 3, 2, 1),            # x1 > x2: the swap
                 ("$s1", 19, 4, 8, 0xFFFFFFFF, 0x22, 3, 2, 1),      # the swap also swaps the two colours
                 ("$s1", 4, 19, 1, 0x11, 0x22, 3, 2, 0),            # y < top
                 ("$s1", 4, 19, 12, 0x11, 0x22, 3, 2, 0),           # y >= bottom
                 ("$s1", 9, 9, 5, 0x11, 0x22, 3, 2, 0),             # x1 == x2
                 ("$s1", 4, 19, 5, 0xFFFFFFFF, 0xFFFFFFFF, 3, 2, 0),  # both colours -1: returns
                 ("$s1", 22, 28, 5, 0x11, 0x22, 3, 2, 0),           # x1 >= right
                 ("$s1", 0, 3, 5, 0x11, 0x22, 3, 2, 0),             # x2 < left
                 ("$s1", 0, 31, 9, 0x11, 0x22, 4, 4, 2),            # both clamps
                 ("$s0", 0, 31, 0, 0x33, 0x44, 1, 1, 0),            # single-pixel runs
                 ("$s0", 0, 31, 2, 0x33, 0x44, 5, 3, 7),            # phase inside the second run
                 ("$s0", 0, 31, 4, 0x33, 0x44, 5, 3, -3),           # negative phase (the idiv is signed)
                 ("$s0", 0, 31, 6, 0x33, 0x44, 7, 1, 0),
                 ("$s0", 0, 31, 8, 0x8000AAAA, 0x8000BBBB, 2, 6, 3),  # both bit 31 set: neither is looked up
                 ("$s0", 0, 31, 10, 0x80001111, 0x22, 2, 6, 3),     # one looked up, one not
                 ("$s2", 0, 18, 5, 0x55, 0x66, 2, 3, 4),            # RGB555, stride 19
                 ("$s9", 0, 31, 0, 0x33, 0x44, 1, 1, 0),            # second palette, same as the s0 vector
                 ("$s5", 0, 31, 12, 0x11, 0x22, 3, 2, 0),           # no surface palette: the global fallback
                 ("$s3", 0, 31, 14, 0x11, 0x22, 3, 2, 0),           # format code 2: returns
                 ("$s3", 0, 31, 13, 0x8000CCCC, 0x8000DDDD, 3, 2, 0),  # format code 2 but both bit 31 set: draws
                 ("$s4", 4, 19, 5, 0x11, 0x22, 3, 2, 0),            # no base: NULL pixel address
                 ("$s7", 4, 19, 5, 0x11, 0x22, 3, 2, 0),            # depth 12: NULL pixel address
                 ("$s6", 0, 31, 11, 0x11, 0x22, 3, 2, 0),           # depth 8 surface
                 ("$s8", 0, 15, 1, 0x11, 0x22, 3, 2, 0),            # inside both the clip and the surface
                 ("$s8", 20, 30, 2, 0x11, 0x22, 3, 2, 0)]),         # past the surface width -> NULL

    # ---- blit8from8: the 8-bit keyed copy between two surfaces, on the four 8-bit surfaces of the fixture.
    #      `this` is $s10 (32 x 16, stride 32), which the body READS; the first argument is $s11 / $s12 / $s13,
    #      which it WRITES at the positions already holding the key byte (VA 0x10013acd compares EDI, the
    #      argument's pixels; 0x10013ad1 stores ESI, `this`'s pixels, into it). The argument slices of $pix carry
    #      a seven-value alphabet, so keys 0x40..0x46 match roughly one pixel in seven and 0x50 matches none.
    #      The four cols & 3 entry points of the unrolled loop (0x10013a88..0x10013ac9) are driven by the
    #      widths 16, 17, 18 and 19.
    "Surface::blit8from8": dict(
        module="jgld.dll", addr=0x00013800, abi="thiscall", ret="int",
        args=["pointer", "pointer", "int", "int", "int", "int", "int", "int", "int"],
        fixture=_F, state=_STATE,
        vectors=[("$s10", "$s11", 0, 0, 0, 0, 24, 12, 0x40),      # whole source, cols 24: cols & 3 == 0
                 ("$s10", "$s11", 0, 0, 0, 0, 24, 12, 0x43),      # another key: different pixels match
                 ("$s10", "$s11", 0, 0, 0, 0, 24, 12, 0x50),      # no pixel matches: only the compare's miss side
                 ("$s10", "$s11", 0, 0, 0, 0, 16, 8, 0x41),       # cols 16: cols & 3 == 0
                 ("$s10", "$s11", 0, 0, 0, 0, 17, 8, 0x41),       # cols 17: cols & 3 == 1
                 ("$s10", "$s11", 0, 0, 0, 0, 18, 8, 0x41),       # cols 18: cols & 3 == 2
                 ("$s10", "$s11", 0, 0, 0, 0, 19, 8, 0x41),       # cols 19: cols & 3 == 3
                 ("$s10", "$s11", 0, 0, 0, 0, 1, 1, 0x40),        # one pixel
                 ("$s10", "$s11", 4, 3, 2, 1, 16, 8, 0x42),       # offset on both surfaces
                 ("$s10", "$s11", -3, 0, 5, 0, 10, 6, 0x42),      # x < 0: w, sx and x adjusted
                 ("$s10", "$s11", 0, -2, 0, 4, 10, 6, 0x42),      # y < 0: h, sy and y adjusted
                 ("$s10", "$s11", -3, -2, 5, 4, 10, 6, 0x42),     # both adjusted
                 ("$s10", "$s11", 40, 0, 0, 0, 8, 8, 0x42),       # x > width -> 0
                 ("$s10", "$s11", 0, 20, 0, 0, 8, 8, 0x42),       # y > height -> 0
                 ("$s10", "$s11", 32, 0, 0, 0, 8, 8, 0x42),       # x == width: accepted, then w cut to 0
                 ("$s10", "$s11", 28, 0, 0, 0, 10, 8, 0x42),      # x + w > width: w cut to 4
                 ("$s10", "$s11", 0, 12, 0, 0, 8, 10, 0x42),      # y + h > height: h cut to 4
                 ("$s10", "$s11", 0, 0, 100, 100, 8, 8, 0x42),    # empty intersection -> 0
                 ("$s10", "$s11", 0, 0, 0, 0, 0, 0, 0x42),        # zero size -> 0
                 ("$s10", "$s11", 0, 0, 20, 8, 16, 8, 0x44),      # intersection clipped at the right and bottom
                 ("$s10", "$s12", 0, 0, 0, 0, 24, 12, 0x44),      # the argument's clip trims left and top
                 ("$s10", "$s12", 2, 2, 4, 3, 10, 5, 0x45),
                 ("$s10", "$s13", 0, 0, 0, 0, 16, 8, 0x42),       # argument has no base: NULL -> 3
                 ("$s4", "$s11", 0, 0, 0, 0, 16, 8, 0x42),        # `this` has no base: NULL -> 3
                 ("$s7", "$s11", 0, 0, 0, 0, 16, 8, 0x42),        # `this` is depth 12: NULL -> 3
                 ("$s10", 0, 0, 0, 0, 0, 16, 8, 0x42)]),          # null argument -> 3
}
