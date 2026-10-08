# Batch c3bh: jgld.dll render (module-aware hooks; addr is the RVA, VA = 0x10000000 + addr).
#   0x00012db0 Surface::blitT_8to16   the 8-bit -> 16-bit block copy, five overlap bodies and a palette table
#   0x0000ed70 Surface::blitT_16to16  its 16-bit twin, five overlap bodies, no palette
#   0x0001bae0 Sprite::remap8         the colour substitution over a Sprite's 8-bit bitmap (a leaf)
#   0x0000afe0 Surface::fill16        the 16-bit twin of Surface::fill8 0xf880 (batch c3av)
#
# Every surface, palette and pixel buffer the vectors touch is allocated by the fixture
# (re/frida/js/fixtures.d/c3bh.js) on jgld's own Surface (0x1011d0b0) and Palette (0x1011db10) vtables; nothing
# live (the Display at 0x10128420, a screen surface, the module's own Palette, DirectDraw) is passed in or
# written. log/c3/alt/c3bh_probe.py, one 12 s boot at the menu with counters attached straight to the module
# addresses: 0 entries into 0x12db0 and 0 into its only caller Surface::blitT 0x91a0.
#
# State: $pix is the single block holding all eighteen pixel buffers, so one region says which surface changed;
# $l0..$l17 are the per-surface lock windows (surface+0x4c0: base, use count, current base) so a missing
# slot-+0x24 release reads RED. The function writes no argument (its RECT is a local at [ebp-0x20]).
#
# Four module globals are deliberately NOT state regions, for the reason c3av recorded for 0x1012847c and c3bc
# for the blitKey8to16 six: they belong to the module, not to a vector. 0x10128498 is the source row skip
# (loaded with [this+0x40] at VA 0x1001300b, then overwritten with srcPitch -+ cols), 0x1012848c the pixels per
# row, 0x10128494 the row counter and 0x1012849c the colour table pointer (the last three shared with
# blitKey8to16 0x12900). The probe measured 0 changes in each over 12 s at the menu. What each is set to is
# verified indirectly and deterministically through $pix: a wrong skip puts rows 2..h at the wrong offsets, a
# wrong count writes the wrong number of pixels per row and a wrong table writes the wrong colours.

_PIX = ("$pix", 0, 0x16000)                      # 22 surfaces x 0x1000
_LOCKS = [(f"$l{i}", 0, 0x14) for i in range(22)]
_STATE = [_PIX] + _LOCKS

_F = "c3bh_surfaces"

HOOKS = {
    # ---- blitT_8to16(dst, sx, sy, dx, dy, w, h): an 8-bit source block expanded through a Palette colour
    #      table into a 16-bit destination. `this` is the SOURCE and the first argument the DESTINATION, as in
    #      blitT_8to8 0x13c50. The five copy bodies are selected by `dst == this` (VA 0x10013013) and, when
    #      they are the same object, by dx vs sx (0x10013103) and dy vs sy (0x1001310f / 0x100132f1).
    #      The two `dx >= sx` bodies start their cursors cols - 4 (or cols - 1) columns right of the rectangle
    #      and still walk each row forward, so they write past it; every such write stays inside the surface's
    #      own 0x1000-byte slice of $pix (see the cpp and the fixture). The last body counts rows with the last
    #      source byte of each row, so $mb1, $mb2 and $mb3 carry planted bytes that end it after one, two and
    #      one row.
    "Surface::blitT_8to16": dict(
        module="jgld.dll", addr=0x00012DB0, abi="thiscall", ret="int",
        args=["pointer", "pointer", "int", "int", "int", "int", "int", "int"],
        fixture=_F, state=_STATE,
        vectors=[# --- the preamble, on the dst != this body
                 ("$s8nb", "$d16a", 0, 0, 0, 0, 8, 8),       # [this+0x4c0] == 0 -> 7
                 ("$s8a", 0, 0, 0, 0, 0, 8, 8),              # null dst -> 16
                 ("$s8a", "$d16a", 0, 0, 0, 0, 32, 16),      # the whole 32 x 16 destination
                 ("$s8a", "$d16a", 2, 3, 1, 1, 16, 8),       # offset on both surfaces
                 ("$s8a", "$d16a", -3, 0, 5, 0, 10, 6),      # sx < 0: w and dx adjusted
                 ("$s8a", "$d16a", 0, -2, 0, 4, 10, 6),      # sy < 0: h and dy adjusted
                 ("$s8a", "$d16a", -3, -2, 5, 4, 10, 6),     # both adjusted
                 ("$s8a", "$d16a", 40, 0, 0, 0, 8, 8),       # sx > width -> 0
                 ("$s8a", "$d16a", 0, 20, 0, 0, 8, 8),       # sy > height -> 0
                 ("$s8a", "$d16a", 32, 0, 0, 0, 8, 8),       # sx == width: accepted, then w cut to 0
                 ("$s8a", "$d16a", 28, 0, 0, 0, 10, 8),      # sx + w > width: w cut to 4
                 ("$s8a", "$d16a", 0, 12, 0, 0, 8, 10),      # sy + h > height: h cut to 4
                 ("$s8a", "$d16a", 0, 0, 100, 100, 8, 8),    # empty intersection -> 0
                 ("$s8a", "$d16a", 0, 0, 0, 0, 0, 0),        # zero size -> 0
                 ("$s8a", "$d16b", 0, 0, 0, 0, 32, 16),      # the destination's clip trims left and top
                 ("$s8a", "$d16b", 2, 2, 4, 3, 10, 5),
                 ("$s8np", "$d16a", 0, 0, 0, 0, 16, 8),      # this has no Palette: the client fallback
                 ("$s8a", "$d16c", 0, 0, 0, 0, 19, 11),      # dst format code 1: Palette slot +0x1c
                 ("$s8b", "$d16c", 0, 0, 0, 0, 19, 11),      # the source carries the OTHER palette: only this
                                                             # supplies the table, so the colours are s8b's
                 ("$s8a", "$d16d", 0, 0, 0, 0, 16, 8),       # dst format code 2 -> 1
                 ("$s8a", "$d16f", 0, 0, 0, 0, 16, 8),       # dst has no base: lockAt16 NULL -> 3
                 ("$s12", "$d16a", 0, 0, 0, 0, 16, 8),       # this depth 12: source NULL -> dst released, 3
                 ("$s8a", "$d16e", 0, 0, 0, 0, 16, 4),       # dst clip wider than its bounds, rect inside both
                 ("$s8a", "$d16e", 0, 0, 20, 1, 8, 2),       # dst: past the surface width -> NULL -> 3
                 ("$s8a", "$d16a", 0, 0, 0, 0, 1, 1),        # one pixel
                 ("$s8b", "$d16a", 0, 0, 0, 0, 24, 12),      # a 24 x 12 source at stride 24
                 ("$s8b", "$d16b", 1, 1, 3, 2, 12, 6),
                 # --- dst == this, dx < sx, dy < sy: the body that is identical to the first one
                 ("$m1", "$m1", 12, 8, 4, 2, 8, 5),
                 ("$m1f1", "$m1f1", 12, 8, 4, 2, 8, 5),      # the same with format code 1
                 ("$m12", "$m12", 12, 8, 4, 2, 8, 5),        # depth 12: destination address NULL -> 3
                 # --- dst == this, dx < sx, dy >= sy: rows bottom to top
                 ("$m1", "$m1", 12, 2, 4, 8, 8, 5),
                 ("$m1", "$m1", 12, 4, 4, 4, 8, 5),          # dy == sy: the >= side of the test
                 ("$m12", "$m12", 12, 2, 4, 8, 8, 5),        # depth 12 -> 3
                 # --- dst == this, dx >= sx, dy < sy: the cursor starts cols - 4 to the right
                 ("$m1", "$m1", 0, 8, 14, 2, 10, 3),         # cols 10 (>= 4)
                 ("$m1", "$m1", 0, 8, 14, 2, 3, 3),          # cols 3 (< 4): the cols - 1 start
                 ("$mclip", "$mclip", 0, 4, 14, 0, 8, 2),    # clip wider than the bounds: the start column is
                                                             # past the width -> NULL -> 3
                 # --- dst == this, dx >= sx, dy >= sy: the body whose row count is the last source byte
                 ("$mb1", "$mb1", 0, 0, 16, 4, 10, 3),       # the planted 1 at byte 79: exactly one row
                 ("$mb2", "$mb2", 0, 0, 16, 4, 10, 3),       # 5 at 79 then 1 at 67: exactly two rows
                 ("$mb3", "$mb3", 0, 0, 16, 4, 3, 3),        # cols 3 (< 4), the planted 1 at byte 68: one row
                 ("$mclip", "$mclip", 0, 0, 14, 4, 8, 2),    # the start column is past the width -> NULL -> 3
                 ("$m12", "$m12", 0, 0, 16, 4, 10, 3)]),     # depth 12 -> 3

    # ---- blitT_16to16(dst, sx, sy, dx, dy, w, h): the same five-way overlap split with 16-bit pixels on both
    #      sides and no Palette. Each row is `rep movsd` of cols >> 1 pairs plus, when cols is odd, one more
    #      pixel: a forward `rep movsw` in the three ascending bodies and a hand-written `mov cx, [esi+2]` in
    #      the two descending ones, which lands two bytes above where the descending dwords stopped. Odd and
    #      even widths are therefore both driven in every body, and cols 1, 2 and 3 drive the cols < 4 start.
    "Surface::blitT_16to16": dict(
        module="jgld.dll", addr=0x0000ED70, abi="thiscall", ret="int",
        args=["pointer", "pointer", "int", "int", "int", "int", "int", "int"],
        fixture=_F, state=_STATE,
        vectors=[("$d16a", 0, 0, 0, 0, 0, 8, 8),         # null dst -> 3 (not 16: this function has no base test)
                 ("$d16a", "$d16d", 0, 0, 0, 0, 32, 16), # the whole 32 x 16 destination, even width
                 ("$d16a", "$d16d", 0, 0, 0, 0, 9, 5),   # odd width: the trailing `rep movsw`
                 ("$d16a", "$d16d", 1, 1, 2, 2, 7, 3),   # odd width with offsets on both surfaces
                 ("$d16a", "$d16b", 0, 0, 0, 0, 32, 16), # the destination's clip trims left and top
                 ("$d16a", "$d16b", 2, 2, 4, 3, 10, 5),
                 ("$d16a", "$d16d", -3, 0, 5, 0, 10, 6), # sx < 0
                 ("$d16a", "$d16d", 0, -2, 0, 4, 10, 6), # sy < 0
                 ("$d16a", "$d16d", -3, -2, 5, 4, 10, 6),
                 ("$d16a", "$d16d", 40, 0, 0, 0, 8, 8),  # sx > width -> 0
                 ("$d16a", "$d16d", 0, 20, 0, 0, 8, 8),  # sy > height -> 0
                 ("$d16a", "$d16d", 32, 0, 0, 0, 8, 8),  # sx == width: accepted, then w cut to 0
                 ("$d16a", "$d16d", 28, 0, 0, 0, 10, 8), # w cut to 4
                 ("$d16a", "$d16d", 0, 12, 0, 0, 8, 10), # h cut to 4
                 ("$d16a", "$d16d", 0, 0, 100, 100, 8, 8),   # empty intersection -> 0
                 ("$d16a", "$d16d", 0, 0, 0, 0, 0, 0),   # zero size -> 0
                 ("$d16a", "$d16f", 0, 0, 0, 0, 8, 8),   # dst has no base -> 3
                 ("$d16f", "$d16a", 0, 0, 0, 0, 8, 8),   # this has no base: source NULL -> dst released, 3
                 ("$s12", "$d16a", 0, 0, 0, 0, 8, 8),    # this depth 12: source NULL -> 3
                 ("$d16a", "$s12", 0, 0, 0, 0, 8, 8),    # dst depth 12: destination NULL -> 3
                 ("$d16c", "$d16a", 0, 0, 0, 0, 19, 11), # a 19 x 11 source at stride 19
                 ("$d16a", "$d16e", 0, 0, 0, 0, 16, 4),  # dst clip wider than its bounds, rect inside both
                 ("$d16a", "$d16e", 0, 0, 20, 1, 8, 2),  # past the destination's width -> NULL -> 3
                 ("$d16a", "$d16d", 0, 0, 0, 0, 1, 1),   # cols 1: no dword, one word
                 ("$d16a", "$d16d", 0, 0, 0, 0, 2, 1),   # cols 2: one dword, no word
                 ("$d16a", "$d16d", 0, 0, 0, 0, 3, 2),   # cols 3
                 # --- dst == this, dx < sx, dy < sy
                 ("$m16", "$m16", 12, 8, 4, 2, 8, 5),
                 ("$m16", "$m16", 12, 8, 4, 2, 7, 5),    # odd width
                 ("$m12", "$m12", 12, 8, 4, 2, 8, 5),    # depth 12: destination NULL -> 3
                 # --- dst == this, dx < sx, dy >= sy: rows bottom to top
                 ("$m16", "$m16", 12, 2, 4, 8, 8, 5),
                 ("$m16", "$m16", 12, 2, 4, 8, 7, 5),    # odd width
                 ("$m16", "$m16", 12, 4, 4, 4, 8, 5),    # dy == sy: the >= side of the test
                 ("$m12", "$m12", 12, 2, 4, 8, 8, 5),    # depth 12 -> 3
                 # --- dst == this, dx >= sx, dy < sy: `std`, the cursor starts cols - 2 to the right
                 ("$m16", "$m16", 0, 8, 14, 2, 10, 3),   # even width: the dword-only loop
                 ("$m16", "$m16", 0, 8, 14, 2, 9, 3),    # odd width: the loop with the extra pixel
                 ("$m16", "$m16", 0, 8, 14, 2, 3, 3),    # cols 3 (< 4): the cols - 1 start, odd
                 ("$m16", "$m16", 0, 8, 14, 2, 2, 3),    # cols 2 (< 4): the cols - 1 start, even
                 ("$m16", "$m16", 0, 8, 14, 2, 1, 3),    # cols 1: no dword at all, only the extra pixel
                 ("$m16c", "$m16c", 0, 4, 14, 0, 8, 2),  # clip wider than the bounds: the start column is past
                                                         # the width -> NULL -> 3
                 # --- dst == this, dx >= sx, dy >= sy: `std` and rows bottom to top
                 ("$m16", "$m16", 0, 0, 16, 4, 10, 3),   # even width
                 ("$m16", "$m16", 0, 0, 16, 4, 9, 3),    # odd width
                 ("$m16", "$m16", 0, 0, 16, 4, 3, 3),    # cols 3 (< 4)
                 ("$m16", "$m16", 0, 0, 16, 4, 1, 3),    # cols 1
                 ("$m16", "$m16", 4, 4, 4, 4, 8, 4),     # dx == sx and dy == sy: a copy onto itself
                 ("$m16c", "$m16c", 0, 0, 14, 4, 8, 2)]),   # the start column is past the width -> NULL -> 3

    # ---- remap8(from, to): a leaf (its only call is the frame check 0x1007e780), so the gate wants at least
    #      ten vectors; there are sixteen. The planted bitmap layout (see the fixture) makes `from` = 0x41 take
    #      both sides of all eight compares of the block unrolled four times; sp6's single group ends on a
    #      match, which is the only way to leave the after-a-match chain's `loop` (0x1001bbce) not taken.
    "Sprite::remap8": dict(
        module="jgld.dll", addr=0x0001BAE0, abi="thiscall", ret="int",
        args=["pointer", "int", "int"],
        fixture="c3bh_sprites", state=[("$spix", 0, 0x700), ("$spr", 0, 0x380)],
        vectors=[("$sp5", 0x41, 0x7E),        # no bitmap -> 7
                 ("$sp0", 0x41, 0x7E),        # 10 groups, no tail: the whole planted layout
                 ("$sp0", 0x41, 0x41),        # to == from: every write stores the byte that is already there
                 ("$sp0", 0x40, 0x7D),        # another value, matching wherever the base pattern is 0x40
                 ("$sp0", 0x00, 0x7C),
                 ("$sp1", 0x41, 0x7B),        # 10 groups and a 3-byte tail, the planted 0x41 at index 41
                 ("$sp1", 0x40, 0x7A),
                 ("$sp1", 0x45, 0x73),
                 ("$sp2", 0x41, 0x79),        # bit 0 clear: pitch x height
                 ("$sp2", 0x41, 0x00),
                 ("$sp3", 0x41, 0x78),        # 8 groups and a 3-byte tail
                 ("$sp3", 0x42, 0x77),
                 ("$sp4", 0x41, 0x76),        # 0 groups: the tail is skipped, nothing is written
                 ("$sp6", 0x41, 0x75),        # 1 group ending on a match, then a 1-byte tail
                 ("$sp6", 0x42, 0x74),
                 ("$sp0", 0x141, 0x27E)]),    # both arguments masked to a byte

    # ---- fill16(x, y, w, h, c1, c2): replaces c1 with c2 over the clipped rectangle. $f16a and $f16b hold a
    #      seven-word alphabet, so 0x2305 matches about one word in seven (both sides of the compare at
    #      VA 0x1000b0ba run thousands of times in one vector) and 0x7fff matches none. $d16f has no pixel
    #      base and $s12 an unsupported depth, which are the two ways slot +0x1c returns NULL (-> 7).
    "Surface::fill16": dict(
        module="jgld.dll", addr=0x0000AFE0, abi="thiscall", ret="int",
        args=["pointer", "int", "int", "int", "int", "int", "int"],
        fixture=_F, state=_STATE,
        vectors=[("$f16a", 0, 0, 32, 16, 0x2305, 0x7E01),    # the whole surface
                 ("$f16a", 0, 0, 32, 16, 0x7FFF, 0x7E01),    # a colour that is in no pixel: no write at all
                 ("$f16a", 0, 0, 32, 16, 0x2305, 0x2305),    # c2 == c1: every write stores what is there
                 ("$f16a", 0, 0, 32, 16, 0x0801, 0x7E02),    # another alphabet value
                 ("$f16a", 4, 2, 10, 6, 0x2305, 0x7E03),     # a rectangle inside
                 ("$f16a", -5, -3, 20, 10, 0x2305, 0x7E04),  # clipped at the left and the top
                 ("$f16a", 25, 10, 20, 20, 0x2305, 0x7E05),  # clipped at the right and the bottom
                 ("$f16a", 40, 40, 8, 8, 0x2305, 0x7E06),    # empty intersection -> 0
                 ("$f16a", 0, 0, 0, 0, 0x2305, 0x7E07),      # zero size -> 0
                 ("$f16a", 3, 4, 1, 1, 0x3407, 0x7E08),      # one pixel
                 ("$f16a", 3, 4, 1, 5, 0x3407, 0x7E09),      # one column: the pixel loop runs once per row
                 ("$f16a", 3, 4, 9, 1, 0x3407, 0x7E0A),      # one row
                 ("$f16b", 0, 0, 32, 16, 0x2305, 0x7E0B),    # the clip {4,2,20,12} trims the rectangle
                 ("$f16b", 6, 4, 6, 4, 0x560B, 0x7E0C),
                 ("$f16a", 0, 0, 32, 16, 0x32305, 0x37E0D),  # both colours masked to 16 bits
                 ("$d16f", 0, 0, 8, 8, 0x2305, 0x7E0E),      # no pixel base -> 7
                 ("$s12", 0, 0, 8, 8, 0x2305, 0x7E0F)]),     # depth 12 -> 7
}
