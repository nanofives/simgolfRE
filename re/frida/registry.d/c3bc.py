# Batch c3bc: five jgld.dll render functions (module-aware hooks; addr is the RVA, VA = 0x10000000 + addr).
#   0x00013c50 Surface::blitT_8to8     the 8-bit copy with four same-surface overlap cases
#   0x00012900 Surface::blitKey8to16   the 8-bit -> 16-bit copy that writes only over one colour
#   0x0000f6a0 Surface::blit16from16   the shade blend through a 0x80000-entry table (x87 shade index)
#   0x0000fb70 Surface::line8          Bresenham, 8-bit
#   0x0000b140 Surface::line16         Bresenham, 16-bit, with the palette colour block of batch c3ay
#
# Every surface, palette, pixel buffer, rectangle and shade table the vectors touch is allocated by the fixture
# (re/frida/js/fixtures.d/c3bc.js) on jgld's own Surface (0x1011d0b0) and Palette (0x1011db10) vtables; nothing
# live (the Display at 0x10128420, a screen surface, the module's own Palette, DirectDraw) is passed in or
# written. log/c3/alt/c3bc_probe.py: the running game enters none of the five, nor their callers, at the menu.
#
# State: $pix is the single block holding all nineteen pixel buffers, so one region says which surface changed;
# $l0..$l18 are the per-surface lock windows (surface+0x4c0: base, use count, current base) so a missing
# slot-+0x24 release reads RED; $rects holds the ten rectangles blit16from16 intersects IN PLACE
# (VA 0x1000f706), which is the only argument any of the five writes.
#
# Seven module globals are deliberately NOT state regions, for the reason c3av recorded for 0x1012847c: they
# belong to the module, not to a vector. 0x101284b4 is blitT_8to8's `cols & 3`; 0x1012848c / 0x10128490 /
# 0x10128494 / 0x1012849c / 0x101284a0 / 0x101284a4 are blitKey8to16's pixels per row, destination row skip, row
# counter, colour table pointer, unused group count and source row skip. The probe measured 0 changes in each
# over 12 s at the menu. What each is set to is verified indirectly and deterministically through $pix: a wrong
# skip puts rows 2..h at the wrong offsets and a wrong count writes the wrong number of pixels per row.

_PIX = ("$pix", 0, 0x4C00)                       # 19 surfaces x 0x400
_LOCKS = [(f"$l{i}", 0, 0x14) for i in range(19)]
_RECTS = ("$rects", 0, 0xA0)                     # 10 RECTs
_STATE = [_PIX] + _LOCKS + [_RECTS]

_F = "c3bc_surfaces"

HOOKS = {
    # ---- blitT_8to8(dst, sx, sy, dx, dy, w, h): an 8-bit block copy. `this` is the SOURCE and the first
    #      argument the DESTINATION. The five copy bodies are selected by `dst == this` (VA 0x10013e19) and,
    #      when they are the same object, by dx vs sx (0x10013ef1) and dy vs sy (0x10013f03 / 0x100140b5).
    #      s9 -> s10/s11 drive the dst != this body; s14 -> s14 drives the four same-surface ones, always with
    #      dx >= 4 so that the three bytes the two descending bodies write before rc.left (the `rep movsd`
    #      then `rep movsb` defect, see the cpp) stay inside s14's own slice of $pix.
    "Surface::blitT_8to8": dict(
        module="jgld.dll", addr=0x00013C50, abi="thiscall", ret="int",
        args=["pointer", "pointer", "int", "int", "int", "int", "int", "int"],
        fixture=_F, state=_STATE,
        vectors=[("$s12", "$s10", 0, 0, 0, 0, 8, 8),      # [this+0x4c0] == 0 -> 7
                 ("$s9", 0, 0, 0, 0, 0, 8, 8),            # null dst -> 16
                 ("$s9", "$s10", 0, 0, 0, 0, 24, 12),     # the whole 24 x 12 destination
                 ("$s9", "$s10", 2, 3, 1, 1, 16, 8),      # offset on both surfaces
                 ("$s9", "$s10", -3, 0, 5, 0, 10, 6),     # sx < 0: w, dx adjusted
                 ("$s9", "$s10", 0, -2, 0, 4, 10, 6),     # sy < 0: h, dy adjusted
                 ("$s9", "$s10", -3, -2, 5, 4, 10, 6),    # both adjusted
                 ("$s9", "$s10", 40, 0, 0, 0, 8, 8),      # sx > width -> 0
                 ("$s9", "$s10", 0, 20, 0, 0, 8, 8),      # sy > height -> 0
                 ("$s9", "$s10", 32, 0, 0, 0, 8, 8),      # sx == width: accepted, then w cut to 0
                 ("$s9", "$s10", 28, 0, 0, 0, 10, 8),     # sx + w > width: w cut to 4
                 ("$s9", "$s10", 0, 12, 0, 0, 8, 10),     # sy + h > height: h cut to 4
                 ("$s9", "$s10", 0, 0, 100, 100, 8, 8),   # empty intersection -> 0
                 ("$s9", "$s10", 0, 0, 0, 0, 0, 0),       # zero size -> 0
                 ("$s9", "$s11", 0, 0, 0, 0, 24, 12),     # the destination's clip trims left and top
                 ("$s9", "$s11", 2, 2, 4, 3, 10, 5),
                 ("$s9", "$s13", 0, 0, 0, 0, 16, 8),      # dst depth 12: destination address NULL -> 3
                 ("$s13", "$s10", 0, 0, 0, 0, 16, 8),     # this depth 12: source NULL -> dst released, 3
                 ("$s9", "$s15", 0, 0, 0, 0, 16, 8),      # dst clip wider than its bounds, rect inside both
                 ("$s9", "$s15", 0, 0, 20, 1, 8, 2),      # dst: past the surface width -> NULL -> 3
                 ("$s9", "$s10", 0, 0, 0, 0, 1, 1),       # cols 1: only `rep movsb`
                 ("$s9", "$s10", 0, 0, 0, 0, 2, 1),       # cols 2
                 ("$s9", "$s10", 0, 0, 0, 0, 3, 1),       # cols 3
                 ("$s9", "$s10", 0, 0, 0, 0, 4, 2),       # cols 4: one `rep movsd` group, no tail
                 ("$s9", "$s10", 0, 0, 0, 0, 5, 2),       # cols 5: one group plus a 1-byte tail
                 ("$s9", "$s10", 0, 0, 0, 0, 6, 2),       # cols 6
                 ("$s9", "$s10", 0, 0, 0, 0, 7, 2),       # cols 7
                 ("$s9", "$s10", 0, 0, 0, 0, 8, 2),       # cols 8: two groups
                 ("$s14", "$s14", 8, 4, 4, 2, 10, 6),     # same surface, dx < sx and dy < sy: ascending
                 ("$s14", "$s14", 8, 4, 4, 2, 1, 1),      # the same with one byte
                 ("$s14", "$s14", 8, 2, 4, 6, 10, 6),     # dx < sx, dy >= sy: rows bottom to top
                 ("$s14", "$s14", 8, 4, 4, 4, 10, 6),     # dx < sx, dy == sy: the >= side of the dy test
                 ("$s14", "$s14", 4, 4, 8, 2, 10, 6),     # dx >= sx, dy < sy: descending, cols 10 (10 & 3 = 2)
                 ("$s14", "$s14", 4, 4, 8, 2, 3, 4),      # the same with cols 3 (< 4: no `rep movsd`)
                 ("$s14", "$s14", 4, 4, 8, 2, 12, 6),     # the same with cols 12 (12 & 3 = 0)
                 ("$s14", "$s14", 4, 2, 8, 6, 10, 6),     # dx >= sx, dy >= sy: descending, rows bottom to top
                 ("$s14", "$s14", 4, 2, 8, 6, 8, 6),      # the same with cols 8
                 ("$s14", "$s14", 4, 2, 8, 6, 3, 4),      # the same with cols 3
                 ("$s14", "$s14", 4, 4, 8, 4, 10, 6),     # dx >= sx, dy == sy
                 ("$s14", "$s14", 4, 2, 4, 2, 8, 6),      # dx == sx, dy == sy: a copy onto itself
                 ("$s14", "$s14", 0, 0, 16, 8, 16, 8),    # a half-surface shift, cols 16
                 ("$s15", "$s15", 0, 0, 20, 0, 4, 2),     # same surface, past its width -> NULL -> 3
                 ("$s13", "$s13", 8, 4, 4, 2, 10, 6),     # same surface, depth 12: the ascending body's
                                                          # destination address is NULL -> 3
                 ("$s13", "$s13", 8, 2, 4, 6, 10, 6),     # the same in the rows-backward body
                 ("$s18", "$s18", 0, 2, 14, 0, 6, 2),     # clip wider than the bounds: the descending body's
                                                          # start column is past the width -> NULL -> 3
                 ("$s9", "$s10", 0, 0, 0, 0, 10, 1),      # one row (dst != this)
                 ("$s14", "$s14", 8, 4, 4, 2, 10, 1),     # one row, ascending
                 ("$s14", "$s14", 8, 2, 4, 6, 10, 1),     # one row, rows backward
                 ("$s14", "$s14", 4, 4, 8, 2, 10, 1),     # one row, descending
                 ("$s14", "$s14", 4, 2, 8, 6, 10, 1)]),   # one row, descending and backward

    # ---- blitKey8to16(dst, sx, sy, dx, dy, w, h, key): 8-bit source, 16-bit destination. The colour table
    #      comes from the palette of `this` but is selected by the format code of dst, so s9 (palette) with s2
    #      (format 1) proves which object supplies which. The 16-bit slices of $pix hold a seven-value alphabet
    #      from that palette, so keys 0x40..0x46 match roughly one word in seven and 0x50 matches none.
    "Surface::blitKey8to16": dict(
        module="jgld.dll", addr=0x00012900, abi="thiscall", ret="int",
        args=["pointer", "pointer", "int", "int", "int", "int", "int", "int", "uint32"],
        fixture=_F, state=_STATE,
        vectors=[("$s9", "$s0", 0, 0, 0, 0, 32, 16, 0x40),
                 ("$s9", "$s0", 0, 0, 0, 0, 32, 16, 0x43),   # another key: different words match
                 ("$s9", "$s0", 0, 0, 0, 0, 32, 16, 0x46),
                 ("$s9", "$s0", 0, 0, 0, 0, 32, 16, 0x50),   # no word matches: only the miss side runs
                 ("$s9", "$s0", 0, 0, 0, 0, 16, 8, 0x41),
                 ("$s9", "$s0", 0, 0, 0, 0, 17, 8, 0x41),    # odd widths: the row counter is a plain `dec`
                 ("$s9", "$s0", 0, 0, 0, 0, 18, 8, 0x41),
                 ("$s9", "$s0", 0, 0, 0, 0, 19, 8, 0x41),
                 ("$s9", "$s0", 0, 0, 0, 0, 1, 1, 0x40),     # one pixel
                 ("$s9", "$s0", 4, 3, 2, 1, 16, 8, 0x42),    # offset on both surfaces
                 ("$s9", "$s0", -3, 0, 5, 0, 10, 6, 0x42),   # sx < 0
                 ("$s9", "$s0", 0, -2, 0, 4, 10, 6, 0x42),   # sy < 0
                 ("$s9", "$s0", -3, -2, 5, 4, 10, 6, 0x42),  # both
                 ("$s9", "$s0", 40, 0, 0, 0, 8, 8, 0x42),    # sx > width -> 0
                 ("$s9", "$s0", 0, 20, 0, 0, 8, 8, 0x42),    # sy > height -> 0
                 ("$s9", "$s0", 32, 0, 0, 0, 8, 8, 0x42),    # sx == width: accepted, then w cut to 0
                 ("$s9", "$s0", 28, 0, 0, 0, 10, 8, 0x42),   # w cut to 4
                 ("$s9", "$s0", 0, 12, 0, 0, 8, 10, 0x42),   # h cut to 4
                 ("$s9", "$s0", 0, 0, 100, 100, 8, 8, 0x42), # empty intersection -> 0
                 ("$s9", "$s0", 0, 0, 0, 0, 0, 0, 0x42),     # zero size -> 0
                 ("$s9", "$s1", 0, 0, 0, 0, 32, 16, 0x44),   # the destination's clip trims left and top
                 ("$s9", "$s1", 2, 2, 4, 3, 10, 5, 0x45),
                 ("$s9", "$s2", 0, 0, 0, 0, 19, 11, 0x40),   # destination format 1: Palette slot +0x1c
                 ("$s9", "$s2", 0, 0, 0, 0, 19, 11, 0x44),
                 ("$s9", "$s6", 0, 0, 0, 0, 32, 16, 0x40),   # a destination carrying the OTHER palette: only
                                                             # this (s9) supplies the table, so the words that
                                                             # match are still the ones s9's palette maps
                 ("$s9", "$s3", 0, 0, 0, 0, 16, 8, 0x42),    # destination format code 2 -> 1
                 ("$s9", "$s4", 0, 0, 0, 0, 16, 8, 0x42),    # destination has no base: NULL -> 3
                 ("$s13", "$s0", 0, 0, 0, 0, 16, 8, 0x42),   # this depth 12: source NULL -> 3
                 ("$s17", "$s0", 0, 0, 0, 0, 16, 8, 0x42),   # this has no palette: the client fallback
                 ("$s9", "$s7", 0, 0, 0, 0, 16, 8, 0x42),    # dst clip wider than its bounds
                 ("$s9", "$s7", 0, 0, 20, 1, 8, 2, 0x42),    # past the destination's width -> NULL -> 3
                 ("$s9", 0, 0, 0, 0, 0, 8, 8, 0x42),         # null dst -> 3
                 ("$s10", "$s0", 0, 0, 0, 0, 24, 12, 0x41),  # a 24 x 12 source
                 ("$s10", "$s1", 1, 1, 3, 2, 12, 6, 0x45)]),

    # ---- blit16from16(r, src, alpha, table): the shade index is the only floating point in the batch
    #      (fild / fdiv 100.0f / fmul 15.0f / __ftol at VA 0x1000f6bd..0x1000f6cc). alpha stays in [0, 100] so
    #      the index (15 - n) << 15 | colour stays inside the 0x80000-word table; outside that range the
    #      original indexes gigabytes away from it. Neither pixel address is tested for NULL, so every vector
    #      uses surfaces that have one.
    "Surface::blit16from16": dict(
        module="jgld.dll", addr=0x0000F6A0, abi="thiscall", ret="int",
        args=["pointer", "pointer", "pointer", "int", "pointer"],
        fixture=_F, state=_STATE,
        vectors=[("$s0", "$br_full", "$s6", 0, "$tbl"),      # alpha 0 -> n 0 -> shade 15 << 15
                 ("$s0", "$br_full", "$s6", 100, "$tbl"),    # alpha 100 -> n 15 -> shade 0
                 ("$s0", "$br_full", "$s6", 50, "$tbl"),
                 ("$s0", "$br_full", "$s6", 1, "$tbl"),      # 0.15 -> 0
                 ("$s0", "$br_full", "$s6", 7, "$tbl"),      # 1.05 -> 1
                 ("$s0", "$br_full", "$s6", 33, "$tbl"),     # 4.95 -> 4
                 ("$s0", "$br_full", "$s6", 66, "$tbl"),     # 9.9  -> 9
                 ("$s0", "$br_full", "$s6", 99, "$tbl"),     # 14.85 -> 14
                 ("$s0", "$br_full", "$s6", 20, "$tbl"),     # 3.0 exactly
                 ("$s0", "$br_full", "$s6", 40, "$tbl"),     # 6.0 exactly
                 ("$s0", "$br_full", "$s6", 50, "$tbl2"),    # the same vector through the shifted table
                 ("$s0", "$br_in", "$s6", 20, "$tbl"),
                 ("$s0", "$br_odd", "$s6", 40, "$tbl"),      # w 11, h 9
                 ("$s0", "$br_1x1", "$s6", 60, "$tbl"),      # one pixel
                 ("$s0", "$br_2x3", "$s6", 80, "$tbl"),
                 ("$s0", "$br_neg", "$s6", 25, "$tbl"),      # clipped at the top-left
                 ("$s0", "$br_over", "$s6", 75, "$tbl"),     # clipped at the right and the bottom
                 ("$s0", "$br_out", "$s6", 50, "$tbl"),      # empty intersection -> 3
                 ("$s2", "$br_s19", "$s8", 50, "$tbl"),      # 19 x 11 at stride 19: a non-zero row skip
                 ("$s2", "$br_s19in", "$s8", 10, "$tbl"),
                 ("$s1", "$br_full", "$s6", 45, "$tbl"),     # a surface whose clip differs from its bounds
                 ("$s6", "$br_in", "$s0", 90, "$tbl")]),     # the two surfaces swapped

    # ---- line8(x1, y1, x2, y2, c): Bresenham on an 8-bit surface. s16 has the clip rectangle {4,2,20,12}
    #      strictly inside the bounds {0,0,32,16}, so every guard, every pre-walk, every endpoint clamp and
    #      both inner exits are reachable; s9 has clip == bounds and s12 no pixels at all.
    "Surface::line8": dict(
        module="jgld.dll", addr=0x0000FB70, abi="thiscall", ret="void",
        args=["pointer", "int", "int", "int", "int", "int"],
        fixture=_F, state=_STATE,
        vectors=[("$s16", 6, 3, 18, 10, 0x11),      # down, x-major, wholly inside
                 ("$s16", 6, 3, 10, 10, 0x12),      # down, y-major
                 ("$s16", 6, 10, 18, 3, 0x13),      # up, x-major
                 ("$s16", 6, 10, 10, 3, 0x14),      # up, y-major
                 ("$s16", 18, 10, 6, 3, 0x15),      # x2 < x1: the endpoints swap, then down
                 ("$s16", 18, 3, 6, 10, 0x16),      # the swap, then up
                 ("$s16", 9, 3, 9, 10, 0x17),       # x1 == x2 -> vline 0x10010620
                 ("$s16", 6, 5, 18, 5, 0x18),       # y1 == y2 -> hline 0x100103f0
                 ("$s16", -8, -4, 18, 10, 0x19),    # pre-walk from left and above, x-major
                 ("$s16", -8, 1, 10, 11, 0x1A),     # pre-walk from the left only
                 ("$s16", -2, -6, 6, 11, 0x1B),     # pre-walk, y-major
                 ("$s16", 6, 18, 18, 1, 0x1C),      # up, pre-walk from below
                 ("$s16", -4, 18, 8, 1, 0x1D),      # up, pre-walk from the left and below, y-major
                 ("$s16", 6, 3, 30, 11, 0x1E),      # x2 past the right: clamped to right - 1
                 ("$s16", 6, 3, 10, 30, 0x1F),      # y2 past the bottom: clamped to bottom - 1
                 ("$s16", 6, 10, 30, 1, 0x20),      # up, x2 clamped
                 ("$s16", 6, 10, 8, -20, 0x21),     # up, y-major, y2 below the top: clamped to top
                 ("$s16", 6, 3, 25, 25, 0x22),      # down, y-major, both ends outside
                 ("$s16", 6, 3, 25, 4, 0x23),       # down, nearly horizontal
                 ("$s16", 3, 3, 25, 9, 0x24),       # one pre-walk step from the left
                 ("$s16", 25, 3, 30, 9, 0x25),      # x1 > clip.right: rejected
                 ("$s16", -30, 5, -10, 9, 0x26),    # x2 < clip.left: rejected
                 ("$s16", 6, 15, 18, 20, 0x27),     # y1 > clip.bottom: rejected
                 ("$s16", 6, -20, 18, -5, 0x28),    # y2 < clip.top: rejected
                 ("$s12", 6, 3, 18, 10, 0x29),      # lockBits() == 0: returns with no release
                 ("$s9", 0, 0, 31, 15, 0x2A),       # clip == bounds, the full diagonal
                 ("$s9", 0, 15, 31, 0, 0x2B),       # the full diagonal upward
                 ("$s9", 31, 0, 0, 15, 0x2C),       # the swap, then upward
                 ("$s9", 1, 2, 30, 3, 0x2D),        # nearly horizontal
                 ("$s9", 2, 1, 3, 14, 0x2E),        # nearly vertical
                 ("$s16", 19, 2, 21, 11, 0x30),     # down, y-major: the inner x1 >= right exit
                 ("$s16", 19, 11, 21, 2, 0x31),     # up, y-major: the inner x1 >= right exit
                 ("$s16", 6, 10, 20, 14, 0x32),     # down, x-major: the inner y1 >= bottom exit
                 ("$s16", 6, 4, 20, 0, 0x33),       # up, x-major: the inner y1 < top exit
                 ("$s16", 6, 3, 19, 11, 0x34),      # the far end exactly on the last column and row
                 # The sixteen below close the branch table (log/c3/alt/c3bc_branches.py): each octant needs a
                 # pre-walk entered by the VERTICAL condition alone and a pre-walk that ends outside the
                 # rectangle on each axis, and the upward guard needs all four of its rejections.
                 ("$s16", 5, 0, 25, 18, 0x40),      # octant 1 (down, x-major): pre-walk on y only, succeeds
                 ("$s16", 6, -10, 25, 5, 0x41),     # octant 1: the pre-walk ends past the right -> dropped
                 ("$s16", -20, 5, 20, 40, 0x42),    # octant 1: the pre-walk ends past the bottom -> dropped
                 ("$s16", 6, -10, 12, 11, 0x43),    # octant 2 (down, y-major): pre-walk on y only, succeeds
                 ("$s16", 19, -10, 25, 11, 0x44),   # octant 2: the pre-walk ends past the right -> dropped
                 ("$s16", -6, 0, 30, 100, 0x45),    # octant 2: the pre-walk ends past the bottom -> dropped
                 ("$s16", 5, 14, 25, -4, 0x46),     # octant 3 (up, x-major): pre-walk on y only, succeeds
                 ("$s16", 6, 24, 25, 9, 0x47),      # octant 3: the pre-walk ends past the right -> dropped
                 ("$s16", -20, 10, 20, -25, 0x48),  # octant 3: the pre-walk ends above the top -> dropped
                 ("$s16", 19, 24, 25, 5, 0x49),     # octant 4 (up, y-major): ends past the right -> dropped
                 ("$s16", -6, 10, 30, -90, 0x4A),   # octant 4: the pre-walk ends above the top -> dropped
                 ("$s16", 25, 10, 30, 3, 0x4B),     # upward guard: x1 > clip.right
                 ("$s16", 6, 1, 18, -5, 0x4C),      # upward guard: y1 < clip.top
                 ("$s16", -30, 10, -10, 3, 0x4D),   # upward guard: x2 < clip.left
                 ("$s16", 6, 20, 18, 14, 0x4E),     # upward guard: y2 > clip.bottom
                 ("$s9", 4, 2, 20, 13, 0x4F)]),     # clip == bounds, a second shape

    # ---- line16(x1, y1, x2, y2, c): the same geometry on 16-bit surfaces, plus the palette colour block.
    #      s1 is the 16-bit twin of s16 (clip {4,2,20,12} inside bounds {0,0,32,16}). A colour with bit 31 set
    #      skips the lookup; s2 takes the RGB555 table, s3's format code 2 returns before lockBits, s5 has no
    #      palette of its own and falls back to the client at 0x1012873c, s6 carries the second palette and s4
    #      has no pixels.
    "Surface::line16": dict(
        module="jgld.dll", addr=0x0000B140, abi="thiscall", ret="void",
        args=["pointer", "int", "int", "int", "int", "uint32"],
        fixture=_F, state=_STATE,
        vectors=[("$s1", 6, 3, 18, 10, 0x11),       # down, x-major, wholly inside
                 ("$s1", 6, 3, 10, 10, 0x12),       # down, y-major
                 ("$s1", 6, 10, 18, 3, 0x13),       # up, x-major
                 ("$s1", 6, 10, 10, 3, 0x14),       # up, y-major
                 ("$s1", 18, 10, 6, 3, 0x15),       # x2 < x1: the endpoints swap, then down
                 ("$s1", 18, 3, 6, 10, 0x16),       # the swap, then up
                 ("$s1", 9, 3, 9, 10, 0x17),        # x1 == x2 -> vline16 0x1000bde0, with the RAW colour
                 ("$s1", 6, 5, 18, 5, 0x18),        # y1 == y2 -> hline16 0x1000ba90, with the RAW colour
                 ("$s1", 9, 3, 9, 10, 0x80001234),  # the vertical case with bit 31 set
                 ("$s1", 6, 3, 18, 10, 0x8000ABCD), # bit 31 set: no lookup, the low 16 bits are the colour
                 ("$s1", 6, 3, 18, 10, 0x1234),     # only the low byte indexes the table
                 ("$s1", -8, -4, 18, 10, 0x19),     # pre-walk from left and above, x-major
                 ("$s1", -8, 1, 10, 11, 0x1A),      # pre-walk from the left only
                 ("$s1", -2, -6, 6, 11, 0x1B),      # pre-walk, y-major
                 ("$s1", 6, 18, 18, 1, 0x1C),       # up, pre-walk from below
                 ("$s1", -4, 18, 8, 1, 0x1D),       # up, pre-walk from the left and below, y-major
                 ("$s1", 6, 3, 30, 11, 0x1E),       # x2 past the right: clamped to right - 1
                 ("$s1", 6, 3, 10, 30, 0x1F),       # y2 past the bottom: clamped to bottom - 1
                 ("$s1", 6, 10, 30, 1, 0x20),       # up, x2 clamped
                 ("$s1", 6, 10, 8, -20, 0x21),      # up, y-major, y2 clamped to the top
                 ("$s1", 6, 3, 25, 25, 0x22),       # down, y-major, both ends outside
                 ("$s1", 3, 3, 25, 9, 0x24),        # one pre-walk step from the left
                 ("$s1", 25, 3, 30, 9, 0x25),       # x1 > clip.right: rejected
                 ("$s1", -30, 5, -10, 9, 0x26),     # x2 < clip.left: rejected
                 ("$s1", 6, 15, 18, 20, 0x27),      # y1 > clip.bottom: rejected
                 ("$s1", 6, -20, 18, -5, 0x28),     # y2 < clip.top: rejected
                 ("$s1", 19, 2, 21, 11, 0x30),      # down, y-major: the inner x1 >= right exit
                 ("$s1", 19, 11, 21, 2, 0x31),      # up, y-major: the inner x1 >= right exit
                 ("$s1", 6, 10, 20, 14, 0x32),      # down, x-major: the inner y1 >= bottom exit
                 ("$s1", 6, 4, 20, 0, 0x33),        # up, x-major: the inner y1 < top exit
                 ("$s0", 0, 0, 31, 15, 0x2A),       # clip == bounds, the full diagonal
                 ("$s0", 0, 15, 31, 0, 0x2B),       # the full diagonal upward
                 ("$s0", 31, 0, 0, 15, 0x2C),       # the swap, then upward
                 ("$s2", 0, 0, 18, 10, 0x2D),       # RGB555 (format code 1), stride 19
                 ("$s2", 0, 10, 18, 0, 0x2E),       # the same upward
                 ("$s2", 0, 0, 18, 10, 0x80005555), # format 1 with bit 31 set: no lookup
                 ("$s6", 0, 0, 31, 15, 0x2A),       # the second palette: the same index, a different word
                 ("$s5", 0, 0, 31, 15, 0x2F),       # no palette of its own: the client fallback
                 ("$s3", 0, 0, 31, 15, 0x35),       # format code 2: returns before lockBits
                 ("$s3", 0, 0, 31, 15, 0x80006666), # format code 2 with bit 31 set: draws
                 ("$s4", 6, 3, 18, 10, 0x36),       # no pixels: lockBits() == 0
                 # The same sixteen closures as line8 (log/c3/alt/c3bc_branches.py).
                 ("$s1", 5, 0, 25, 18, 0x40),       # octant 1 (down, x-major): pre-walk on y only, succeeds
                 ("$s1", 6, -10, 25, 5, 0x41),      # octant 1: the pre-walk ends past the right -> dropped
                 ("$s1", -20, 5, 20, 40, 0x42),     # octant 1: the pre-walk ends past the bottom -> dropped
                 ("$s1", 6, -10, 12, 11, 0x43),     # octant 2 (down, y-major): pre-walk on y only, succeeds
                 ("$s1", 19, -10, 25, 11, 0x44),    # octant 2: the pre-walk ends past the right -> dropped
                 ("$s1", -6, 0, 30, 100, 0x45),     # octant 2: the pre-walk ends past the bottom -> dropped
                 ("$s1", 5, 14, 25, -4, 0x46),      # octant 3 (up, x-major): pre-walk on y only, succeeds
                 ("$s1", 6, 24, 25, 9, 0x47),       # octant 3: the pre-walk ends past the right -> dropped
                 ("$s1", -20, 10, 20, -25, 0x48),   # octant 3: the pre-walk ends above the top -> dropped
                 ("$s1", 19, 24, 25, 5, 0x49),      # octant 4 (up, y-major): ends past the right -> dropped
                 ("$s1", -6, 10, 30, -90, 0x4A),    # octant 4: the pre-walk ends above the top -> dropped
                 ("$s1", 25, 10, 30, 3, 0x4B),      # upward guard: x1 > clip.right
                 ("$s1", 6, 1, 18, -5, 0x4C),       # upward guard: y1 < clip.top
                 ("$s1", -30, 10, -10, 3, 0x4D),    # upward guard: x2 < clip.left
                 ("$s1", 6, 20, 18, 14, 0x4E),      # upward guard: y2 > clip.bottom
                 ("$s2", 0, 10, 18, 0, 0x8000AAAA)]),  # RGB555 upward with bit 31 set

    # ---- dashedLine8(x1, y1, x2, y2, c1, c2, len1, len2, phase): line8 with a dash pattern. The geometry
    #      vectors are line8's; on top of them come the dash parameters, both colours -1 (which this function,
    #      unlike dashedHLine16 0xcf90, really can see because it does not mask them first), a negative phase
    #      for the signed `idiv` at VA 0x10010995, and the two degenerate run lengths. len1 + len2 is never 0:
    #      that `idiv` would fault.
    "Surface::dashedLine8": dict(
        module="jgld.dll", addr=0x00010870, abi="thiscall", ret="void",
        args=["pointer", "int", "int", "int", "int", "int", "int", "int", "int", "int"],
        fixture=_F, state=_STATE,
        vectors=[("$s16", 6, 3, 18, 10, 0x11, 0x22, 3, 2, 0),    # down, x-major, phase inside the c1 run
                 ("$s16", 6, 3, 18, 10, 0x11, 0x22, 3, 2, 4),    # phase >= len1: inside the c2 run
                 ("$s16", 6, 3, 18, 10, -1, 0x22, 3, 2, 1),      # c1 == -1: the c1 run leaves gaps
                 ("$s16", 6, 3, 18, 10, 0x11, -1, 3, 2, 1),      # c2 == -1
                 ("$s16", 6, 3, 18, 10, -1, -1, 3, 2, 0),        # both -1: the cursor runs, nothing is written
                 ("$s16", 6, 3, 18, 10, 0x11, 0x22, 1, 1, 0),    # single-pixel runs
                 ("$s16", 6, 3, 18, 10, 0x33, 0x44, 5, 3, 7),    # phase inside the second run
                 ("$s16", 6, 3, 18, 10, 0x33, 0x44, 5, 3, -3),   # a negative phase: the idiv is signed
                 ("$s16", 6, 3, 18, 10, 0x33, 0x44, 7, 1, 0),
                 ("$s16", 6, 3, 18, 10, 0x33, 0x44, 0, 4, 1),    # len1 == 0: the cursor never goes negative
                 ("$s16", 6, 3, 18, 10, 0x33, 0x44, 3, 0, 1),    # len2 == 0
                 ("$s16", 6, 3, 10, 10, 0x11, 0x22, 3, 2, 0),    # down, y-major
                 ("$s16", 6, 10, 18, 3, 0x11, 0x22, 3, 2, 0),    # up, x-major
                 ("$s16", 6, 10, 10, 3, 0x11, 0x22, 3, 2, 0),    # up, y-major
                 ("$s16", 18, 10, 6, 3, 0x11, 0x22, 3, 2, 1),    # the swap: ends, runs, colours and phase
                 ("$s16", 18, 3, 6, 10, -1, 0x22, 3, 2, 1),      # the swap moves the -1 to the other colour
                 ("$s16", 9, 3, 9, 10, 0x11, 0x22, 3, 2, 0),     # x1 == x2 -> dashedVLine8 0x10011880
                 ("$s16", 6, 5, 18, 5, 0x11, 0x22, 3, 2, 0),     # y1 == y2 -> dashedHLine8 0x100114e0
                 ("$s16", -8, -4, 18, 10, 0x11, 0x22, 3, 2, 0),  # pre-walk from left and above
                 ("$s16", -2, -6, 6, 11, 0x11, 0x22, 3, 2, 2),   # pre-walk, y-major
                 ("$s16", 6, 18, 18, 1, 0x11, 0x22, 3, 2, 0),    # up, pre-walk from below
                 ("$s16", -4, 18, 8, 1, 0x11, 0x22, 3, 2, 0),    # up, pre-walk from the left and below
                 ("$s16", 6, 3, 30, 11, 0x11, 0x22, 3, 2, 0),    # x2 clamped to right - 1
                 ("$s16", 6, 3, 10, 30, 0x11, 0x22, 3, 2, 0),    # y2 clamped to bottom - 1
                 ("$s16", 6, 10, 30, 1, 0x11, 0x22, 3, 2, 0),    # up, x2 clamped
                 ("$s16", 6, 10, 8, -20, 0x11, 0x22, 3, 2, 0),   # up, y-major, y2 clamped to the top
                 ("$s16", 25, 3, 30, 9, 0x11, 0x22, 3, 2, 0),    # x1 > clip.right: rejected
                 ("$s16", -30, 5, -10, 9, 0x11, 0x22, 3, 2, 0),  # x2 < clip.left: rejected
                 ("$s16", 6, 15, 18, 20, 0x11, 0x22, 3, 2, 0),   # y1 > clip.bottom: rejected
                 ("$s16", 6, -20, 18, -5, 0x11, 0x22, 3, 2, 0),  # y2 < clip.top: rejected
                 ("$s12", 6, 3, 18, 10, 0x11, 0x22, 3, 2, 0),    # lockBits() == 0
                 ("$s9", 0, 0, 31, 15, 0x55, 0x66, 4, 4, 2),     # clip == bounds, the full diagonal
                 ("$s9", 0, 15, 31, 0, 0x55, 0x66, 4, 4, 2),     # the full diagonal upward
                 ("$s9", 1, 2, 30, 3, 0x55, 0x66, 2, 3, 1),      # nearly horizontal
                 ("$s9", 2, 1, 3, 14, 0x55, 0x66, 2, 3, 1),      # nearly vertical
                 ("$s16", 19, 2, 21, 11, 0x11, 0x22, 3, 2, 0),   # down, y-major: the inner x1 >= right exit
                 ("$s16", 19, 11, 21, 2, 0x11, 0x22, 3, 2, 0),   # up, y-major: the inner x1 >= right exit
                 ("$s16", 6, 10, 20, 14, 0x11, 0x22, 3, 2, 0),   # down, x-major: the inner y1 >= bottom exit
                 ("$s16", 6, 4, 20, 0, 0x11, 0x22, 3, 2, 0),     # up, x-major: the inner y1 < top exit
                 # -1 in the other three octants, so each of the eight dash-write sites sees both sides of
                 # its two `cmp ..., -1` (log/c3/alt/c3bc_branches.py).
                 ("$s16", 6, 3, 10, 10, -1, 0x22, 3, 2, 1),      # octant 2, c1 == -1
                 ("$s16", 6, 3, 10, 10, 0x11, -1, 3, 2, 1),      # octant 2, c2 == -1
                 ("$s16", 6, 10, 18, 3, -1, 0x22, 3, 2, 1),      # octant 3, c1 == -1
                 ("$s16", 6, 10, 18, 3, 0x11, -1, 3, 2, 1),      # octant 3, c2 == -1
                 ("$s16", 6, 10, 10, 3, -1, 0x22, 3, 2, 1),      # octant 4, c1 == -1
                 ("$s16", 6, 10, 10, 3, 0x11, -1, 3, 2, 1),      # octant 4, c2 == -1
                 # The same sixteen pre-walk / guard closures as line8.
                 ("$s16", 5, 0, 25, 18, 0x11, 0x22, 3, 2, 0),
                 ("$s16", 6, -10, 25, 5, 0x11, 0x22, 3, 2, 0),
                 ("$s16", -20, 5, 20, 40, 0x11, 0x22, 3, 2, 0),
                 ("$s16", 6, -10, 12, 11, 0x11, 0x22, 3, 2, 0),
                 ("$s16", 19, -10, 25, 11, 0x11, 0x22, 3, 2, 0),
                 ("$s16", -6, 0, 30, 100, 0x11, 0x22, 3, 2, 0),
                 ("$s16", 5, 14, 25, -4, 0x11, 0x22, 3, 2, 0),
                 ("$s16", 6, 24, 25, 9, 0x11, 0x22, 3, 2, 0),
                 ("$s16", -20, 10, 20, -25, 0x11, 0x22, 3, 2, 0),
                 ("$s16", 19, 24, 25, 5, 0x11, 0x22, 3, 2, 0),
                 ("$s16", -6, 10, 30, -90, 0x11, 0x22, 3, 2, 0),
                 ("$s16", 25, 10, 30, 3, 0x11, 0x22, 3, 2, 0),
                 ("$s16", 6, 1, 18, -5, 0x11, 0x22, 3, 2, 0),
                 ("$s16", -30, 10, -10, 3, 0x11, 0x22, 3, 2, 0),
                 ("$s16", 6, 20, 18, 14, 0x11, 0x22, 3, 2, 0),
                 # The FIRST dash write of each octant happens once per call, so the five conditional jumps of
                 # that site need one vector per side: a cursor of -1 (phase == len1 - 1, the c1 run reloads),
                 # a cursor of 2 and of 1 (the c2 run, the second of which reloads) and c2 == -1 inside the
                 # c2 run. Geometry per octant as above; len1 = 3, len2 = 2.
                 ("$s16", 6, 3, 18, 10, 0x11, 0x22, 3, 2, 2),    # octant 1: first write, cursor -1
                 ("$s16", 6, 3, 18, 10, 0x11, 0x22, 3, 2, 3),    # octant 1: first write, cursor 2
                 ("$s16", 6, 3, 18, 10, 0x11, -1, 3, 2, 4),      # octant 1: first write, c2 == -1
                 ("$s16", 6, 3, 10, 10, 0x11, 0x22, 3, 2, 2),    # octant 2
                 ("$s16", 6, 3, 10, 10, 0x11, 0x22, 3, 2, 3),
                 ("$s16", 6, 3, 10, 10, 0x11, 0x22, 3, 2, 4),
                 ("$s16", 6, 3, 10, 10, 0x11, -1, 3, 2, 4),
                 ("$s16", 6, 10, 18, 3, 0x11, 0x22, 3, 2, 2),    # octant 3
                 ("$s16", 6, 10, 18, 3, 0x11, 0x22, 3, 2, 3),
                 ("$s16", 6, 10, 18, 3, 0x11, 0x22, 3, 2, 4),
                 ("$s16", 6, 10, 18, 3, 0x11, -1, 3, 2, 4),
                 ("$s16", 6, 10, 10, 3, 0x11, 0x22, 3, 2, 2),    # octant 4
                 ("$s16", 6, 10, 10, 3, 0x11, 0x22, 3, 2, 3),
                 ("$s16", 6, 10, 10, 3, 0x11, 0x22, 3, 2, 4),
                 ("$s16", 6, 10, 10, 3, 0x11, -1, 3, 2, 4)]),
}
