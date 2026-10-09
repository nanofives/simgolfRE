# Batch c3bz: jgld.dll render, the nine Sprite draw rows batch c3bu handed over (module-aware hooks; addr is
# the RVA, VA = 0x10000000 + addr).
#   0x00022a10 Sprite::draw16_10022a10  the Palette colour averaged field by field with a caller colour
#   0x00024140 Sprite::draw16_10024140  a caller colour scaled by the Palette colour's weighted sum
#   0x00021720 Sprite::draw16_10021720  two 16-bit tables, applied in OPPOSITE orders in the two halves
#   0x00063f60 Sprite::draw8_10063f60   one 64 KB table indexed by (payload << 8) | destination
#   0x00061db0 Sprite::draw8_10061db0   the payload byte remapped through a byte table
#   0x00060cf0 Sprite::draw8_10060cf0   payload 0xf8 remaps the destination, every other byte is copied
#   0x0005fd20 Sprite::draw8_1005fd20   one flat colour; the stepped half writes zero instead
#   0x0005dde0 Sprite::draw_1005dde0    the payload byte copied, threshold 0xff
#   0x0005ed80 Sprite::draw_1005ed80    the payload byte copied, threshold 0xfe
#
# Every object is allocated by re/frida/js/fixtures.d/c3bz.js on jgld's own Surface (0x1011d0b0), Palette
# (0x1011db10) and Sprite (0x1011d380) vtables; nothing live is passed in.
#
# EVERY key runs with the game's first thread suspended. log/c3/alt/c3bz_probe.py, 12 s at the menu: that
# thread enters Sprite::draw_1005ed80 0x5ed80 (one of these nine) 87_674 times and Sprite::draw16_1001bc40
# 0x1bc40 2_348 times, and between them writes six globals of the 8-bit block and five of the 16-bit one.
# The fixture restores 0x101284d0..0x10128698, the three scale globals and the palette client before it
# resumes the thread. See the header of c3bz.js.
#
# One fixture per setting of 0x10122dc0 / 0x10122dc4 / 0x10122dc8, hence one key per setting (the round-5
# rule): _s111 (1,1,1), _sm11 (-1,1,1), _s1m1 (1,-1,1) and _sm1m1 (-1,-1,1) are the four mirror bodies of
# the unscaled half; _s223 (2,2,3), _sm223, _s2m23 and _sm2m23 the four of the 16.16 stepped half (step 1.5
# in both directions, so the fractional carry of `adc esi` and the row selector `sbb eax, eax` both occur);
# _s323 (3,2,3) enters the stepped half through the SECOND conjunct of the scale test alone; _nopal points
# the palette client at one carrying no Palette, the only way to the `return 16` exit at 0x10022a9f.
# The six 8-bit members have no Palette and no `return 16` exit, so instead of _nopal they carry _s553
# (5,5,3), a magnification whose 16.16 step 0x9999 has an odd low half.
#
# State: $pix is the single block holding all six pixel buffers, so one region says which surface changed;
# $l0..$l5 are the per-surface lock windows (surface+0x4c0: base, use count, current base) so a missing or
# extra slot-+0x24 release reads RED; $spr is the five Sprite structures (none of these nine writes a sprite
# field, and the region proves it). The module globals the two stepped halves own and the argument slots ARE
# state regions: the 16-bit block ($gvstep 0x10128540/44, $gtbl 0x10128548, $gpos 0x10128554/58, $gcols
# 0x10128570/74, $gxstep 0x10128580/84/88/8c/90, $growpair the two ints 0x10122e20 and 0x10122e1c address)
# and the 8-bit one ($g8vstep 0x10128624/28, $g8tbl 0x1012862c, $g8pos 0x1012863c/40, $g8cols 0x10128660,
# $g8colour 0x10128664, $g8pix 0x1012866c, $g8step 0x1012867c/80, $g8xstep 0x10128688/8c/90, $g8rowpair the
# two ints 0x10122e58 and 0x10122e54 address). Both blocks are regions of EVERY key, so a 16-bit member that
# touched the 8-bit block (or the other way round) would read RED, and so would one of the five members that
# must NOT park an integer step at 0x1012867c if it did. The SHARED loop globals (0x1012857c, 0x1012855c,
# 0x1012856c, 0x10128578, 0x10128564, 0x1012853c, 0x10128560 and their 8-bit twins 0x10128678, 0x10128644,
# 0x10128670, 0x10128650, 0x1012861c, 0x10128648) are NOT regions, for the reason c3av recorded for
# 0x1012847c and the probe above repeats: the game writes them. What they are set to is still verified
# through $pix, deterministically (a wrong source pitch, column count or row skip puts the written pixels at
# the wrong offsets and a wrong row count writes the wrong number of rows).

_STATE = ([("$pix", 0, 0x6000), ("$spr", 0, 0x280)] +
          [(f"$l{i}", 0, 0x14) for i in range(6)] +
          [("$gvstep", 0, 8), ("$gtbl", 0, 4), ("$gpos", 0, 8),
           ("$gcols", 0, 8), ("$gxstep", 0, 0x14), ("$growpair", 0, 8),
           ("$g8vstep", 0, 8), ("$g8tbl", 0, 4), ("$g8pos", 0, 8), ("$g8cols", 0, 4),
           ("$g8colour", 0, 4), ("$g8pix", 0, 4), ("$g8step", 0, 8), ("$g8xstep", 0, 0xC),
           ("$g8rowpair", 0, 8)])

# this, dst, x, y, p4, p5          (0x22a10 and 0x24140: `ret 0x14`, both extra arguments plain ints)
_A5 = ["pointer", "pointer", "int", "int", "int", "int"]
# this, dst, x, y, table4, table5  (0x21720: the same frame, both extra arguments tables)
_A5P = ["pointer", "pointer", "int", "int", "pointer", "pointer"]
# this, dst, x, y, table           (0x63f60, 0x61db0, 0x60cf0: `ret 0x10`, the fourth argument a pointer)
_A4P = ["pointer", "pointer", "int", "int", "pointer"]
# this, dst, x, y, colour          (0x5fd20: the fourth argument is a byte in a dword slot)
_A4I = ["pointer", "pointer", "int", "int", "int"]
# this, dst, x, y                  (0x5dde0, 0x5ed80: `ret 0xc`)
_A3 = ["pointer", "pointer", "int", "int"]

# The placements every function is driven with. The first list is the full sweep used by the _s111 key; the
# short one covers the other keys, which exist to select a body rather than to re-walk the clipping.
_FULL = [("$spA", "$d0", 0, 0),        # the whole 16 x 8 sprite inside the destination
         ("$spA", "$d0", 4, 2),        # offset
         ("$spA", "$d0", -3, -2),      # clipped left and top: the source cursor moves
         ("$spA", "$d0", 24, 10),      # clipped right and bottom: cols 8, rows 6
         ("$spA", "$d0", 100, 100),    # empty intersection -> 0 (0x10022c56)
         ("$spA", "$d0", -20, 0),      # entirely left of the clip -> 0
         ("$spA", "$d1", 0, 0),        # the destination's clip trims left and top
         ("$spA", "$d2", 0, 0),        # format 1: Palette slot +0x1c (0x10022b76)
         ("$spA", "$d3", 0, 0),        # format 2: the switch default (0x10022b89)
         ("$spA", "$d4", 0, 0),        # no pixel base: the guard's lock returns 0 -> 3
         ("$spNB", "$d0", 0, 0),       # no payload -> 3 (0x10022a36)
         ("$spA", 0, 0, 0),            # dst NULL -> 3 (0x10022a3c)
         ("$spNP", "$d0", 0, 0),       # no Palette of its own -> the client's (pal2)
         ("$spB", "$d0", 0, 0),        # a 20 x 10 sprite at pitch 20, the other Palette and alphabet
         ("$spD", "$d0", 5, 5),        # 1 x 1: one column and one row
         ("$spA", "$d0", 31, 0),       # cols 1
         ("$spA", "$d0", 0, 15),       # rows 1
         ("$spA", "$d5", 0, 0)]        # a 24-wide destination: the row skips change
_BODY = [("$spA", "$d0", 0, 0),
         ("$spA", "$d0", 4, 2),
         ("$spA", "$d0", -3, -2),
         ("$spA", "$d1", 0, 0),
         ("$spB", "$d0", 1, 1),
         ("$spA", "$d0", 100, 100)]    # empty intersection, before the sign tests
_NOPAL = [("$spNP", "$d0", 0, 0),      # client Palette NULL -> unlock(1) and 16
          ("$spNP", "$d1", 2, 2),      # 16
          ("$spNP", "$d3", 0, 0),      # 16: the Palette test precedes the format switch
          ("$spA", "$d0", 0, 0),       # its own Palette: the normal path still runs
          ("$spA", "$d2", 1, 1)]       # and still reaches the 555 table

_SCALES = [("", "c3bz_s111"), ("_sm11", "c3bz_sm11"), ("_s1m1", "c3bz_s1m1"), ("_sm1m1", "c3bz_sm1m1"),
           ("_s223", "c3bz_s223"), ("_sm223", "c3bz_sm223"), ("_s2m23", "c3bz_s2m23"),
           ("_sm2m23", "c3bz_sm2m23"), ("_s323", "c3bz_s323")]
_FIXTURES16 = _SCALES + [("_nopal", "c3bz_nopal")]
_FIXTURES8 = _SCALES + [("_s553", "c3bz_s553")]


def _keys(name, addr, args, extra, fixtures):
    """Ten keys for one function: the full sweep on (1,1,1), the short sweep on each of the eight other
    body selections, and a tenth variant. `extra` is the tuple of extra arguments appended to every vector
    (a list of alternatives, cycled); None where the member takes none."""
    out = {}
    for suffix, fixture in fixtures:
        base = _FULL if suffix == "" else (_NOPAL if suffix == "_nopal" else _BODY)
        vectors = []
        for i, v in enumerate(base):
            vectors.append(v + extra[i % len(extra)] if extra else v)
        out[name + suffix] = dict(module="jgld.dll", addr=addr, abi="thiscall", ret="int", args=args,
                                  fixture=fixture, state=_STATE, vectors=vectors)
    return out


HOOKS = {}

# ---- 0x22a10 and 0x24140: the fourth argument is a 16-bit colour read as `mov bp, word ptr [ebp + 0x14]`
#      (0x10022d26 / 0x10024c56), so 0x12340a5b proves that only its low word is used; the fifth argument is
#      referenced nowhere in either function, so its varying values must change nothing.
_COL = [(0x7FFF, 0), (0x0421, 0x5A5A), (0x001F, -1), (0x12340A5B, 7), (0x03E0, 0x7FFF)]
HOOKS.update(_keys("Sprite::draw16_10022a10", 0x00022A10, _A5, _COL, _FIXTURES16))
HOOKS.update(_keys("Sprite::draw16_10024140", 0x00024140, _A5, _COL, _FIXTURES16))

# ---- 0x21720: the fourth argument is the 16-bit table parked at 0x10128548 (0x1002182f), the fifth the
#      16-bit table parked at 0x10128574 (0x10021837) that payload 0xf8 indexes with the DESTINATION pixel.
#      Two different pointers per slot so a vector proves which one each loop read.
HOOKS.update(_keys("Sprite::draw16_10021720", 0x00021720, _A5P,
                   [("$u", "$t"), ("$u2", "$t2"), ("$u", "$t2"), ("$u2", "$t")], _FIXTURES16))

# ---- the three 8-bit members with a table argument, all parked at 0x1012862c.
_TBL8 = [("$b",), ("$b2",), ("$b",), ("$b2",)]
HOOKS.update(_keys("Sprite::draw8_10063f60", 0x00063F60, _A4P, _TBL8, _FIXTURES8))
HOOKS.update(_keys("Sprite::draw8_10061db0", 0x00061DB0, _A4P, _TBL8, _FIXTURES8))
HOOKS.update(_keys("Sprite::draw8_10060cf0", 0x00060CF0, _A4P, _TBL8, _FIXTURES8))

# ---- 0x5fd20: the fourth argument is a colour masked to a byte before it is parked at 0x10128664
#      (`and edx, 0xff`, 0x1005fde2), so 0x1234 and 0x34 must give the same pixels; the stepped half writes
#      0 whatever the argument (0x100604a5 / 0x100604aa).
HOOKS.update(_keys("Sprite::draw8_1005fd20", 0x0005FD20, _A4I,
                   [(0x5A,), (0x00,), (0xFF,), (0x1234,), (0x34,)], _FIXTURES8))

# ---- 0x5dde0 and 0x5ed80: no argument beyond the placement; the two differ only in their threshold byte.
HOOKS.update(_keys("Sprite::draw_1005dde0", 0x0005DDE0, _A3, None, _FIXTURES8))
HOOKS.update(_keys("Sprite::draw_1005ed80", 0x0005ED80, _A3, None, _FIXTURES8))
