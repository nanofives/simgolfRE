# Batch c3bm: jgld.dll render, the Sprite family (module-aware hooks; addr is the RVA, VA = 0x10000000 + addr).
#   0x00025900 Sprite::blitPair          two 8-bit payloads blended onto a 16-bit surface pair
#   0x00025dd0 Sprite::blitPair1         the same blend with one destination
#   0x0001ab40 Sprite::draw16t_1001ab40  a 16-bit payload with a key colour and a destination-indexed table
#   0x0001b060 Sprite::draw16t_1001b060  the same shape with a (source, destination) pair table
#   0x0001df80 Sprite::draw16m_1001df80  an 8-bit payload through a Palette table, gated by a mask surface
#   0x0001e970 Sprite::draw16m_1001e970  its twin without the flat-colour arm
#
# Every object is allocated by re/frida/js/fixtures.d/c3bm.js on jgld's own Surface (0x1011d0b0), Palette
# (0x1011db10) and Sprite (0x1011d380) vtables; nothing live is passed in. log/c3/alt/c3bm_probe.py, one 12 s
# boot at the menu with counters attached straight to the module addresses: 0 entries into all six functions
# and into each of their static callers, against 2_047 into the live Sprite::draw16_1001bc40 0x1bc40.
#
# EVERY key runs with the game's first thread suspended, because that thread is inside the same loop-global
# block (the probe counted 523/524/524/130/25 changes in 0x1012857c, 0x10128564, 0x10128560, 0x1012856c and
# 0x10128578 in those twelve seconds) and because the three scale globals, which pick the body, have to be
# written. The fixture restores all of them before it resumes the thread. See the header of c3bm.js.
#
# One fixture per setting of the scale globals 0x10122dc0 / 0x10122dc4 / 0x10122dc8, hence one registry key
# per setting (the round-5 rule):
#   _s111 (1,1,1) the pixel loop   _sm11 (-1,1,1) and _s1m1 (1,-1,1) the two sign tests that skip it
#   _s112 (1,1,2) the scaled half  _s212 (2,1,2) the scaled half entered through the SECOND conjunct only
#   _nopal        the palette client carries no Palette: the `return 16` exits
# The pair blitters read none of the three, so they have one plain key and one _nopal key.
#
# State: $pix is the single block holding all twelve pixel buffers, so one region says which surface changed;
# $l0..$l11 are the per-surface lock windows (surface+0x4c0: base, use count, current base) so a missing or
# extra slot-+0x24 release reads RED; $spr is the ten Sprite structures (draw16t_1001b060 writes +0x28 at
# VA 0x1001b0f2). $gdraw16t (0x101284d8..0x1012851c), $gmask (0x10128530/34/38), $glevel (0x10128568) and
# $gratio (0x1012858c/90) ARE state regions: the probe measured 0 changes in each over 12 s and no function
# that writes them is ever entered by the game, and the thread that could is suspended. The SEVEN SHARED loop
# globals are NOT, for the reason c3av recorded for 0x1012847c: they belong to the module, not to a vector.
# What they are set to is still verified through $pix, deterministically (a wrong source pitch, column count
# or row skip puts the written pixels at the wrong offsets and a wrong row count writes the wrong rows).

_PIX = ("$pix", 0, 0xC000)                       # 12 surfaces x 0x1000
_LOCKS = [(f"$l{i}", 0, 0x14) for i in range(12)]
_SPR = ("$spr", 0, 0x500)                        # 10 sprites x 0x80
_BASE = [_PIX] + _LOCKS + [_SPR]
_T16 = _BASE + [("$gdraw16t", 0, 0x48)]
_T16M = _BASE + [("$gmask", 0, 0xC), ("$glevel", 0, 4), ("$gratio", 0, 8)]

# ---- blitPair(other, dst, dst2, x, y). `this` supplies the colour byte, `other` the weight, `dst` the pixel
#      read into the blend and `dst2` the pixel written (both stores reload the cursor from [ebp-0x2c],
#      0x10025c4b and 0x10025c5d). A destination with no pixel base is never passed: slot +0x20 would return
#      NULL and the loop would write through it (this function has no lock test, unlike draw16t_1001b060).
_PAIR_ARGS = ["pointer", "pointer", "pointer", "pointer", "int", "int"]
_PAIR1_ARGS = ["pointer", "pointer", "pointer", "int", "int"]
_T_ARGS = ["pointer", "pointer", "int", "int", "pointer"]
_M_ARGS = ["pointer", "pointer", "int", "int", "pointer", "int", "int"]
_M1_ARGS = ["pointer", "pointer", "int", "int", "pointer", "int"]

HOOKS = {
    "Sprite::blitPair": dict(
        module="jgld.dll", addr=0x00025900, abi="thiscall", ret="int", args=_PAIR_ARGS,
        fixture="c3bm_pair", state=_BASE,
        vectors=[("$spA", "$spW", "$d0", "$d5", 0, 0),      # the whole 16 x 8 sprite inside the destination
                 ("$spA", "$spW", "$d0", "$d5", 4, 2),      # offset
                 ("$spA", "$spW", "$d0", "$d5", -3, -2),    # clipped left and top: the source cursor moves
                 ("$spA", "$spW", "$d0", "$d5", 24, 10),    # clipped right and bottom: cols 8, rows 6
                 ("$spA", "$spW", "$d0", "$d5", 100, 100),  # empty intersection -> 0 (0x10025a7f)
                 ("$spA", "$spW", "$d0", "$d5", -20, 0),    # entirely left of the clip -> 0
                 ("$spA", "$spW", "$d1", "$d5", 0, 0),      # the destination's clip trims left and top
                 ("$spA", "$spW", "$d2", "$d5", 0, 0),      # format 1: Palette slot +0x1c (0x100259e3)
                 ("$spA", "$spW", "$d3", "$d5", 0, 0),      # format 2: the switch default -> 1 (0x10025a0a)
                 ("$spNP", "$spW", "$d0", "$d5", 0, 0),     # no Palette of its own -> the client's (pal2)
                 ("$spA", "$spC", "$d0", "$d5", 2, 1),      # the weights come from the other alphabet
                 ("$spC", "$spW", "$d0", "$d5", 0, 0),      # a 20 x 10 sprite at pitch 20, Palette pal2
                 ("$spD", "$spW", "$d0", "$d5", 5, 5),      # 1 x 1: one column and one row
                 ("$spA", "$spW", "$d0", "$d5", 31, 0),     # cols 1
                 ("$spA", "$spW", "$d0", "$d5", 0, 15),     # rows 1
                 ("$spA", "$spW", "$d6", "$d7", 0, 0),      # a 24-wide destination: the row skips change
                 ("$spA", "$spW", "$d0", "$d0", 0, 0)]),    # dst2 == dst: each pixel is read then overwritten

    "Sprite::blitPair_nopal": dict(
        module="jgld.dll", addr=0x00025900, abi="thiscall", ret="int", args=_PAIR_ARGS,
        fixture="c3bm_pair_nopal", state=_BASE,
        vectors=[("$spNP", "$spW", "$d0", "$d5", 0, 0),     # client Palette NULL -> unlock(1) and 16
                 ("$spNP", "$spW", "$d1", "$d5", 4, 2),     # 16
                 ("$spNP", "$spW", "$d3", "$d5", 0, 0),     # 16: the Palette test precedes the format switch
                 ("$spA", "$spW", "$d0", "$d5", 0, 0),      # its own Palette: the normal path still runs
                 ("$spA", "$spW", "$d2", "$d5", 1, 1)]),    # and still reaches the 555 table

    # ---- blitPair1(other, dst, x, y): the same blend with one destination (0x100260ea / 0x100260f7).
    "Sprite::blitPair1": dict(
        module="jgld.dll", addr=0x00025DD0, abi="thiscall", ret="int", args=_PAIR1_ARGS,
        fixture="c3bm_pair", state=_BASE,
        vectors=[("$spA", "$spW", "$d0", 0, 0),
                 ("$spA", "$spW", "$d0", 4, 2),
                 ("$spA", "$spW", "$d0", -3, -2),
                 ("$spA", "$spW", "$d0", 24, 10),
                 ("$spA", "$spW", "$d0", 100, 100),         # empty -> 0 (0x10025f4f)
                 ("$spA", "$spW", "$d0", -20, 0),
                 ("$spA", "$spW", "$d1", 0, 0),
                 ("$spA", "$spW", "$d2", 0, 0),             # format 1 -> 555
                 ("$spA", "$spW", "$d3", 0, 0),             # format 2 -> 1 (0x10025e28 is the 16 exit)
                 ("$spNP", "$spW", "$d0", 0, 0),            # the client's Palette
                 ("$spA", "$spC", "$d0", 2, 1),
                 ("$spC", "$spW", "$d0", 0, 0),
                 ("$spD", "$spW", "$d0", 5, 5),             # 1 x 1
                 ("$spA", "$spW", "$d0", 31, 0),            # cols 1
                 ("$spA", "$spW", "$d0", 0, 15),            # rows 1
                 ("$spA", "$spW", "$d6", 0, 0)]),

    "Sprite::blitPair1_nopal": dict(
        module="jgld.dll", addr=0x00025DD0, abi="thiscall", ret="int", args=_PAIR1_ARGS,
        fixture="c3bm_pair_nopal", state=_BASE,
        vectors=[("$spNP", "$spW", "$d0", 0, 0),            # -> 16
                 ("$spNP", "$spW", "$d1", 4, 2),            # -> 16
                 ("$spNP", "$spW", "$d3", 0, 0),            # -> 16
                 ("$spA", "$spW", "$d0", 0, 0),
                 ("$spA", "$spW", "$d2", 1, 1)]),

    # ---- draw16t_1001ab40(dst, x, y, table): a 16-bit payload whose LOW BYTE picks the arm. Equal to the
    #      sprite's key at +0x28 the pixel is left alone (0x1001ad9f), zero replaces the destination pixel by
    #      the table entry that pixel indexes (0x1001ada8), anything else copies the source word (0x1001adb5).
    #      A destination with no pixel base is never passed: this function does not lock, so slot +0x20 would
    #      return NULL and the loop would write through it.
    "Sprite::draw16t_1001ab40": dict(
        module="jgld.dll", addr=0x0001AB40, abi="thiscall", ret="int", args=_T_ARGS,
        fixture="c3bm_s111", state=_T16,
        vectors=[("$sp16a", "$d0", 0, 0, "$t16"),       # the whole 16 x 8 sprite, key 0xfe
                 ("$sp16a", "$d0", 4, 2, "$t16"),
                 ("$sp16a", "$d0", -3, -2, "$t16"),     # clipped left and top
                 ("$sp16a", "$d0", 24, 10, "$t16"),     # clipped right and bottom
                 ("$sp16a", "$d0", 100, 100, "$t16"),   # empty intersection -> 0 (0x1001acb9)
                 ("$sp16a", "$d1", 0, 0, "$t16"),       # the destination's clip trims
                 ("$sp16a", 0, 0, 0, "$t16"),           # dst NULL -> 3 (0x1001ab6a)
                 ("$spNB", "$d0", 0, 0, "$t16"),        # no payload -> 3 (0x1001ab64)
                 ("$sp16c", "$d0", 0, 0, "$t16"),       # key 0x00: the key arm and the table arm swap
                 ("$sp16d", "$d0", 1, 1, "$t16"),       # key 0x11, 12 x 6 at pitch 12
                 ("$sp16a", "$d0", 0, 0, "$t16b"),      # the other table
                 ("$sp16a", "$d0", 31, 0, "$t16"),      # cols 1
                 ("$sp16a", "$d0", 0, 15, "$t16"),      # rows 1
                 ("$sp16a", "$d6", 0, 0, "$t16")]),     # a 24-wide destination

    "Sprite::draw16t_1001ab40_sm11": dict(
        module="jgld.dll", addr=0x0001AB40, abi="thiscall", ret="int", args=_T_ARGS,
        fixture="c3bm_sm11", state=_T16,
        vectors=[("$sp16a", "$d0", 0, 0, "$t16"),       # dc0 < 0: the loop is skipped (0x1001acdf)
                 ("$sp16a", "$d0", 4, 2, "$t16"),
                 ("$sp16a", "$d0", 100, 100, "$t16"),   # empty intersection -> 0 before the sign test
                 ("$sp16d", "$d1", 2, 2, "$t16b"),      # another pitch and table parked
                 ("$spNB", "$d0", 0, 0, "$t16"),        # -> 3
                 ("$sp16a", 0, 0, 0, "$t16")]),         # -> 3

    "Sprite::draw16t_1001ab40_s1m1": dict(
        module="jgld.dll", addr=0x0001AB40, abi="thiscall", ret="int", args=_T_ARGS,
        fixture="c3bm_s1m1", state=_T16,
        vectors=[("$sp16a", "$d0", 0, 0, "$t16"),       # dc4 < 0: the second sign test skips (0x1001acec)
                 ("$sp16a", "$d0", 4, 2, "$t16"),
                 ("$sp16a", "$d0", 100, 100, "$t16"),
                 ("$sp16d", "$d1", 2, 2, "$t16b"),
                 ("$spNB", "$d0", 0, 0, "$t16")]),

    "Sprite::draw16t_1001ab40_s112": dict(
        module="jgld.dll", addr=0x0001AB40, abi="thiscall", ret="int", args=_T_ARGS,
        fixture="c3bm_s112", state=_T16,
        vectors=[("$sp16a", "$d0", 0, 0, "$t16"),       # the scaled half: 16 x 8 stepped to 8 x 4
                 ("$sp16a", "$d0", 4, 2, "$t16"),
                 ("$sp16a", "$d0", 100, 100, "$t16"),   # the module's own intersect fails -> 0 (0x1001ae56)
                 ("$sp16a", "$d1", 0, 0, "$t16"),       # the clip trims the stepped rectangle
                 ("$sp16d", "$d0", 1, 1, "$t16"),       # 12 x 6 stepped to 6 x 3
                 ("$sp16a", "$d6", 2, 2, "$t16"),       # a 24-wide destination: the pitch is re-read
                 ("$spNB", "$d0", 0, 0, "$t16"),        # -> 3
                 ("$sp16a", 0, 0, 0, "$t16")]),         # -> 3

    "Sprite::draw16t_1001ab40_s212": dict(
        module="jgld.dll", addr=0x0001AB40, abi="thiscall", ret="int", args=_T_ARGS,
        fixture="c3bm_s212", state=_T16,
        vectors=[("$sp16a", "$d0", 0, 0, "$t16"),       # abs(dc0) == abs(dc8) but abs(dc4) != abs(dc8)
                 ("$sp16a", "$d0", 4, 2, "$t16"),
                 ("$sp16a", "$d1", 0, 0, "$t16"),
                 ("$sp16d", "$d0", 1, 1, "$t16b"),
                 ("$sp16a", "$d0", 100, 100, "$t16")]),

    # ---- draw16t_1001b060(dst, x, y, table): the same shape with the destination locked in the guard
    #      (0x1001b096), the sprite's key at +0x28 overwritten with 0xff (0x1001b0f2) and the table indexed by
    #      the BYTE offset (source << 16) | destination (0x1001b2d4..0x1001b2dd). $sp16b's payload words are
    #      0x0000..0x0003 and the transparent 0x00ff, and every destination word is below 0x2000, so the
    #      largest offset is 0x31fff and $t32 is 0x32000 bytes.
    "Sprite::draw16t_1001b060": dict(
        module="jgld.dll", addr=0x0001B060, abi="thiscall", ret="int", args=_T_ARGS,
        fixture="c3bm_s111", state=_T16,
        vectors=[("$sp16b", "$d0", 0, 0, "$t32"),
                 ("$sp16b", "$d0", 4, 2, "$t32"),
                 ("$sp16b", "$d0", -3, -2, "$t32"),
                 ("$sp16b", "$d0", 24, 10, "$t32"),
                 ("$sp16b", "$d0", 100, 100, "$t32"),   # empty intersection -> 0 (0x1001b1ec)
                 ("$sp16b", "$d1", 0, 0, "$t32"),
                 ("$sp16b", 0, 0, 0, "$t32"),           # dst NULL -> 3 (0x1001b08a)
                 ("$spNB", "$d0", 0, 0, "$t32"),        # no payload -> 3 (0x1001b084)
                 ("$sp16b", "$d4", 0, 0, "$t32"),       # the destination has no base: lock fails -> 3
                 ("$sp16b", "$d0", 31, 0, "$t32"),      # cols 1
                 ("$sp16b", "$d0", 0, 15, "$t32"),      # rows 1
                 ("$sp16b", "$d6", 0, 0, "$t32")]),     # a 24-wide destination

    "Sprite::draw16t_1001b060_sm11": dict(
        module="jgld.dll", addr=0x0001B060, abi="thiscall", ret="int", args=_T_ARGS,
        fixture="c3bm_sm11", state=_T16,
        vectors=[("$sp16b", "$d0", 0, 0, "$t32"),       # dc0 < 0: the loop is skipped (0x1001b212)
                 ("$sp16b", "$d0", 4, 2, "$t32"),
                 ("$sp16b", "$d0", 100, 100, "$t32"),
                 ("$sp16b", "$d1", 2, 2, "$t32"),
                 ("$sp16b", "$d4", 0, 0, "$t32"),       # -> 3
                 ("$spNB", "$d0", 0, 0, "$t32")]),      # -> 3

    "Sprite::draw16t_1001b060_s1m1": dict(
        module="jgld.dll", addr=0x0001B060, abi="thiscall", ret="int", args=_T_ARGS,
        fixture="c3bm_s1m1", state=_T16,
        vectors=[("$sp16b", "$d0", 0, 0, "$t32"),       # dc4 < 0 (0x1001b21f)
                 ("$sp16b", "$d0", 4, 2, "$t32"),
                 ("$sp16b", "$d0", 100, 100, "$t32"),
                 ("$sp16b", "$d1", 2, 2, "$t32"),
                 ("$sp16b", 0, 0, 0, "$t32")]),         # -> 3

    "Sprite::draw16t_1001b060_s112": dict(
        module="jgld.dll", addr=0x0001B060, abi="thiscall", ret="int", args=_T_ARGS,
        fixture="c3bm_s112", state=_T16,
        vectors=[("$sp16b", "$d0", 0, 0, "$t32"),       # the scaled half (0x1001b305)
                 ("$sp16b", "$d0", 4, 2, "$t32"),
                 ("$sp16b", "$d0", 100, 100, "$t32"),   # the module's own intersect fails -> 0 (0x1001b37f)
                 ("$sp16b", "$d1", 0, 0, "$t32"),
                 ("$sp16b", "$d6", 2, 2, "$t32"),
                 ("$sp16b", "$d4", 0, 0, "$t32"),       # -> 3
                 ("$spNB", "$d0", 0, 0, "$t32")]),      # -> 3

    "Sprite::draw16t_1001b060_s212": dict(
        module="jgld.dll", addr=0x0001B060, abi="thiscall", ret="int", args=_T_ARGS,
        fixture="c3bm_s212", state=_T16,
        vectors=[("$sp16b", "$d0", 0, 0, "$t32"),
                 ("$sp16b", "$d0", 4, 2, "$t32"),
                 ("$sp16b", "$d1", 0, 0, "$t32"),
                 ("$sp16b", "$d6", 1, 1, "$t32"),
                 ("$sp16b", "$d0", 100, 100, "$t32")]),

    # ---- draw16m_1001df80(dst, x, y, mask, level, color): an 8-bit payload drawn through the Palette table,
    #      gated per pixel by a second 8-bit surface. Where the mask byte is below `level` only the payload
    #      value 0xfe is drawn, and then as the flat `color` (0x1001e36b); where it is at or above `level`
    #      every value but 0xfe is drawn through the table (0x1001e37b); 0xff is transparent either way.
    #      `level` and `color` are declared int: the original reads only the low byte (0x1001e0f7) and the low
    #      word (0x1001e100) of their argument slots, and two vectors pass values above those widths to prove
    #      it. ONLY the quadrant with both numerators non-negative can be driven: the three mirror bodies read
    #      their mask cursor from [ebp-0x30], written only at 0x1001e2ea inside this quadrant, so in the other
    #      three it still holds the debug frame fill 0xcccccccc and `cmp byte ptr [ecx], bl` faults.
    "Sprite::draw16m_1001df80": dict(
        module="jgld.dll", addr=0x0001DF80, abi="thiscall", ret="int", args=_M_ARGS,
        fixture="c3bm_s111", state=_T16M,
        vectors=[("$spA", "$d0", 0, 0, "$m0", 0x80, 0x1234),     # the whole 16 x 8 sprite
                 ("$spA", "$d0", 4, 2, "$m0", 0x40, 0x2345),
                 ("$spA", "$d0", -3, -2, "$m0", 0xC0, 0x3456),   # clipped left and top
                 ("$spA", "$d0", 24, 10, "$m0", 0x20, 0x4567),   # clipped right and bottom
                 ("$spA", "$d0", 100, 100, "$m0", 0x80, 0x5678), # empty intersection -> 0 (0x1001e22e)
                 ("$spA", "$d1", 0, 0, "$m0", 0x80, 0x6789),     # the destination's clip trims
                 ("$spA", "$d2", 0, 0, "$m0", 0x80, 0x789A),     # format 1 -> the 555 table
                 ("$spA", "$d3", 0, 0, "$m0", 0x80, 0x89AB),     # format 2 -> 1, and NOTHING is unlocked
                 ("$spA", "$d0", 0, 0, "$m1", 0x80, 0x9ABC),     # the other mask pattern
                 ("$spA", "$d0", 0, 0, "$m2", 0x80, 0xABCD),     # a mask at stride 20
                 ("$spA", "$d0", 0, 0, "$mnb", 0x80, 0xBCDE),    # the mask has no base: its lock fails -> 3
                 ("$spA", "$d0", 0, 0, 0, 0x80, 0xCDEF),         # mask NULL -> 3 (0x1001dfd5)
                 ("$spA", "$d4", 0, 0, "$m0", 0x80, 0xDEF0),     # the destination's lock fails -> 3
                 ("$spNB", "$d0", 0, 0, "$m0", 0x80, 0xEF01),    # no payload -> 3 (0x1001dfa7)
                 ("$spA", 0, 0, 0, "$m0", 0x80, 0xF012),         # dst NULL -> 3 (0x1001dfad)
                 ("$spA", "$d0", 0, 0, "$m0", 0x00, 0x0123),     # level 0: every mask byte takes the >= side
                 ("$spA", "$d0", 0, 0, "$m0", 0xFF, 0x1230),     # level 0xff: only the 0xff bytes do
                 ("$spC", "$d0", 0, 0, "$m0", 0x80, 0x2301),     # a 20 x 10 sprite with the other Palette
                 ("$spD", "$d0", 5, 5, "$m0", 0x80, 0x3012),     # 1 x 1
                 ("$spA", "$d0", 31, 0, "$m0", 0x80, 0x4321),    # cols 1
                 ("$spA", "$d0", 0, 15, "$m0", 0x80, 0x5432),    # rows 1
                 ("$spA", "$d6", 0, 0, "$m0", 0x80, 0x6543),     # a 24-wide destination
                 ("$spA", "$d0", 0, 0, "$m0", 0x180, 0x11234)]), # both masked to a byte and to a word

    "Sprite::draw16m_1001df80_nopal": dict(
        module="jgld.dll", addr=0x0001DF80, abi="thiscall", ret="int", args=_M_ARGS,
        fixture="c3bm_s111_nopal", state=_T16M,
        vectors=[("$spNP", "$d0", 0, 0, "$m0", 0x80, 0x1234),    # client Palette NULL -> 16 (0x1001e060)
                 ("$spNP", "$d1", 2, 2, "$m0", 0x40, 0x2345),    # 16
                 ("$spNP", "$d0", 0, 0, 0, 0x80, 0x4567),        # mask NULL: 3, before the Palette test
                 ("$spA", "$d0", 0, 0, "$m0", 0x80, 0x3456),     # its own Palette: the loop still runs
                 ("$spA", "$d2", 0, 0, "$m0", 0x80, 0x5678)]),   # and still reaches the 555 table

    "Sprite::draw16m_1001df80_s112": dict(
        module="jgld.dll", addr=0x0001DF80, abi="thiscall", ret="int", args=_M_ARGS,
        fixture="c3bm_s112", state=_T16M,
        vectors=[("$spA", "$d0", 0, 0, "$m0", 0x80, 0x1234),     # the scaled else: four releases, no drawing
                 ("$spA", "$d1", 4, 2, "$m0", 0x40, 0x2345),
                 ("$spA", "$d2", 0, 0, "$m0", 0x80, 0x3456),     # the 555 table is still parked
                 ("$spA", "$d3", 0, 0, "$m0", 0x80, 0x4567),     # format 2 -> 1 before the scale test
                 ("$spA", "$d0", 0, 0, "$mnb", 0x80, 0x5678),    # -> 3
                 ("$spC", "$d6", 1, 1, "$m2", 0x40, 0x6789)]),

    "Sprite::draw16m_1001df80_s212": dict(
        module="jgld.dll", addr=0x0001DF80, abi="thiscall", ret="int", args=_M_ARGS,
        fixture="c3bm_s212", state=_T16M,
        vectors=[("$spA", "$d0", 0, 0, "$m0", 0x80, 0x1234),     # abs(dc0) == abs(dc8), abs(dc4) != abs(dc8):
                 ("$spA", "$d1", 4, 2, "$m0", 0x40, 0x2345),     # the taken side of 0x1001e1bd, the only way
                 ("$spA", "$d2", 0, 0, "$m0", 0x80, 0x3456),     # to the scaled else through the SECOND
                 ("$spA", "$d0", 0, 0, "$m1", 0x80, 0x4567),     # conjunct
                 ("$spC", "$d6", 1, 1, "$m2", 0x40, 0x5678)]),

    # ---- draw16m_1001e970(dst, x, y, mask, level): the twin with no flat-colour arm. A payload byte below
    #      0xfe is drawn through the Palette table wherever the mask byte is at or above `level` (0x1001ed42
    #      and 0x1001ed47); nothing else is written. Its three mirror bodies fault for the same reason as the
    #      twin's: the mask cursor at [ebp-0x30] is written only at 0x1001ecd0, inside this quadrant.
    "Sprite::draw16m_1001e970": dict(
        module="jgld.dll", addr=0x0001E970, abi="thiscall", ret="int", args=_M1_ARGS,
        fixture="c3bm_s111", state=_T16M,
        vectors=[("$spA", "$d0", 0, 0, "$m0", 0x80),
                 ("$spA", "$d0", 4, 2, "$m0", 0x40),
                 ("$spA", "$d0", -3, -2, "$m0", 0xC0),
                 ("$spA", "$d0", 24, 10, "$m0", 0x20),
                 ("$spA", "$d0", 100, 100, "$m0", 0x80),         # empty intersection -> 0 (0x1001ec14)
                 ("$spA", "$d1", 0, 0, "$m0", 0x80),
                 ("$spA", "$d2", 0, 0, "$m0", 0x80),             # format 1 -> 555
                 ("$spA", "$d3", 0, 0, "$m0", 0x80),             # format 2 -> 1, nothing unlocked
                 ("$spA", "$d0", 0, 0, "$m1", 0x80),
                 ("$spA", "$d0", 0, 0, "$m2", 0x80),             # a mask at stride 20
                 ("$spA", "$d0", 0, 0, "$mnb", 0x80),            # the mask lock fails -> 3
                 ("$spA", "$d0", 0, 0, 0, 0x80),                 # mask NULL -> 3
                 ("$spA", "$d4", 0, 0, "$m0", 0x80),             # the destination's lock fails -> 3
                 ("$spNB", "$d0", 0, 0, "$m0", 0x80),            # no payload -> 3
                 ("$spA", 0, 0, 0, "$m0", 0x80),                 # dst NULL -> 3
                 ("$spA", "$d0", 0, 0, "$m0", 0x00),             # level 0: every mask byte takes the >= side
                 ("$spA", "$d0", 0, 0, "$m0", 0xFF),             # level 0xff: only the 0xff bytes do
                 ("$spC", "$d0", 0, 0, "$m0", 0x80),             # a 20 x 10 sprite with the other Palette
                 ("$spD", "$d0", 5, 5, "$m0", 0x80),             # 1 x 1
                 ("$spA", "$d0", 31, 0, "$m0", 0x80),            # cols 1
                 ("$spA", "$d0", 0, 15, "$m0", 0x80),            # rows 1
                 ("$spA", "$d6", 0, 0, "$m0", 0x80),             # a 24-wide destination
                 ("$spA", "$d0", 0, 0, "$m0", 0x180)]),          # level masked to a byte

    "Sprite::draw16m_1001e970_nopal": dict(
        module="jgld.dll", addr=0x0001E970, abi="thiscall", ret="int", args=_M1_ARGS,
        fixture="c3bm_s111_nopal", state=_T16M,
        vectors=[("$spNP", "$d0", 0, 0, "$m0", 0x80),            # -> 16 (0x1001ea50)
                 ("$spNP", "$d1", 2, 2, "$m0", 0x40),            # -> 16
                 ("$spNP", "$d0", 0, 0, 0, 0x80),                # mask NULL: 3, before the Palette test
                 ("$spA", "$d0", 0, 0, "$m0", 0x80),
                 ("$spA", "$d2", 0, 0, "$m0", 0x80)]),

    "Sprite::draw16m_1001e970_s112": dict(
        module="jgld.dll", addr=0x0001E970, abi="thiscall", ret="int", args=_M1_ARGS,
        fixture="c3bm_s112", state=_T16M,
        vectors=[("$spA", "$d0", 0, 0, "$m0", 0x80),             # the scaled else: four releases
                 ("$spA", "$d1", 4, 2, "$m0", 0x40),
                 ("$spA", "$d2", 0, 0, "$m0", 0x80),
                 ("$spA", "$d3", 0, 0, "$m0", 0x80),             # format 2 -> 1
                 ("$spA", "$d0", 0, 0, "$mnb", 0x80),            # -> 3
                 ("$spC", "$d6", 1, 1, "$m2", 0x40)]),

    "Sprite::draw16m_1001e970_s212": dict(
        module="jgld.dll", addr=0x0001E970, abi="thiscall", ret="int", args=_M1_ARGS,
        fixture="c3bm_s212", state=_T16M,
        vectors=[("$spA", "$d0", 0, 0, "$m0", 0x80),             # the taken side of 0x1001eba3
                 ("$spA", "$d1", 4, 2, "$m0", 0x40),
                 ("$spA", "$d2", 0, 0, "$m0", 0x80),
                 ("$spA", "$d0", 0, 0, "$m1", 0x80),
                 ("$spC", "$d6", 1, 1, "$m2", 0x40)]),
}
