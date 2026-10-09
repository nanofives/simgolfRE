# Batch c3by: jgld.dll render, the twenty-one `char flag` Sprite blend members batches c3bp and c3bu left
# over (module-aware hooks; addr is the RVA, VA = 0x10000000 + addr).
#
#   RGB555 field layout, the result shifted back down by four before the store
#     0x000317c0 Sprite::draw16_100317c0  three halvings, the Palette colour is the halved side
#     0x00032d60 Sprite::draw16_10032d60  three halvings, the destination pixel is the halved side
#     0x00034300 Sprite::draw16_10034300  (15 * destination + Palette) >> 4
#     0x000358b0 Sprite::draw16_100358b0  (15 * Palette + destination) >> 4
#     0x00036e60 Sprite::draw16_10036e60  ((3 * Palette + destination) >> 2 + destination) >> 1
#     0x00038460 Sprite::draw16_10038460  ((3 * destination + Palette) >> 2 + Palette) >> 1
#     0x00039a60 Sprite::draw16_10039a60  ((7 * Palette + destination) >> 3 + destination) >> 1
#     0x0003b060 Sprite::draw16_1003b060  ((7 * destination + Palette) >> 3 + Palette) >> 1
#     0x0003c660 Sprite::draw16_1003c660  (13 * destination + 3 * Palette) >> 4
#     0x0003dca0 Sprite::draw16_1003dca0  (13 * Palette + 3 * destination) >> 4
#     0x0003f2e0 Sprite::draw16_1003f2e0  (11 * destination + 5 * Palette) >> 4
#     0x00040930 Sprite::draw16_10040930  (11 * Palette + 5 * destination) >> 4
#   RGB565 field layout, the result NOT shifted back (c3bu's finding 1)
#     0x00049ea0 Sprite::draw16_10049ea0  (15 * Palette + destination) >> 4
#     0x0004b430 Sprite::draw16_1004b430  ((3 * Palette + destination) >> 2 + destination) >> 1
#     0x0004ca10 Sprite::draw16_1004ca10  ((3 * destination + Palette) >> 2 + Palette) >> 1
#     0x0004dff0 Sprite::draw16_1004dff0  ((7 * Palette + destination) >> 3 + destination) >> 1
#     0x0004f5d0 Sprite::draw16_1004f5d0  ((7 * destination + Palette) >> 3 + Palette) >> 1
#     0x00050bb0 Sprite::draw16_10050bb0  (13 * destination + 3 * Palette) >> 4
#     0x000521d0 Sprite::draw16_100521d0  (13 * Palette + 3 * destination) >> 4
#     0x000537f0 Sprite::draw16_100537f0  (11 * destination + 5 * Palette) >> 4
#     0x00054e20 Sprite::draw16_10054e20  (11 * Palette + 5 * destination) >> 4
#
# Every object is allocated by re/frida/js/fixtures.d/c3by.js on jgld's own Surface (0x1011d0b0), Palette
# (0x1011db10) and Sprite (0x1011d380) vtables; nothing live is passed in.
#
# EVERY key runs with the game's first thread suspended, because that thread is inside the same seven
# 16-bit loop globals (c3bp measured 462/462/462/122/6 changes in 0x1012857c, 0x10128564, 0x10128560,
# 0x1012856c and 0x10128578 over twelve seconds at the menu) and because the three scale globals, which
# pick which of the eight bodies runs, have to be written. The fixture restores all of them, and the
# whole 8-bit block 0x1012861c..0x10128694, before it resumes the thread. See the header of c3by.js.
#
# One fixture per setting of 0x10122dc0 / 0x10122dc4 / 0x10122dc8, hence one key per setting (the round-5
# rule): _s111 (1,1,1), _sm11 (-1,1,1), _s1m1 (1,-1,1) and _sm1m1 (-1,-1,1) are the four mirror bodies of
# the unscaled half; _s223 (2,2,3), _sm223, _s2m23 and _sm2m23 the four of the 16.16 stepped half (step
# 1.5 in both directions, so the fractional carry of `adc esi` and the row selector `sbb eax, eax` both
# occur); _s323 (3,2,3) enters the stepped half through the SECOND conjunct of the scale test alone;
# _nopal points the palette client at one carrying no Palette, the only way to the `return 16` exit at
# 0x1003184f.
#
# State: $pix is the single block holding all six pixel buffers, so one region says which surface
# changed; $l0..$l5 are the per-surface lock windows (surface+0x4c0: base, use count, current base) so a
# missing or extra slot-+0x24 release reads RED; $spr is the five Sprite structures (none of these
# twenty-one writes a sprite field, and the region proves it). The module globals the stepped half owns
# ARE state regions: the 16-bit block ($gvstep 0x10128540/44, $gtbl 0x10128548, $gpos 0x10128554/58,
# $gcols 0x10128570/74, $gxstep 0x10128580/84/88/8c/90, $growpair the two ints 0x10122e20 and 0x10122e1c
# address) and the 8-bit one ($g8vstep 0x10128624/28, $g8tbl 0x1012862c, $g8pos 0x1012863c/40, $g8cols
# 0x10128660, $g8pix 0x1012866c, $g8step 0x1012867c/80, $g8xstep 0x10128688/8c/90, $g8rowpair the two
# ints 0x10122e58 and 0x10122e54 address). Both blocks are regions of EVERY key, so one of these
# sixteen-bit members touching the 8-bit block would read RED. The SHARED loop globals are NOT regions,
# for the reason c3av recorded for 0x1012847c: they belong to the module, not to a vector. What they are
# set to is still verified through $pix, deterministically (a wrong source pitch, column count or row
# skip puts the written pixels at the wrong offsets and a wrong row count writes the wrong number of
# rows).

_STATE = ([("$pix", 0, 0x6000), ("$spr", 0, 0x280)] +
          [(f"$l{i}", 0, 0x14) for i in range(6)] +
          [("$gvstep", 0, 8), ("$gtbl", 0, 4), ("$gpos", 0, 8),
           ("$gcols", 0, 8), ("$gxstep", 0, 0x14), ("$growpair", 0, 8),
           ("$g8vstep", 0, 8), ("$g8tbl", 0, 4), ("$g8pos", 0, 8), ("$g8cols", 0, 4),
           ("$g8pix", 0, 4), ("$g8step", 0, 8), ("$g8xstep", 0, 0xC), ("$g8rowpair", 0, 8)])

# this, dst, x, y, flag            (the fourth argument is a char)
_A4I = ["pointer", "pointer", "int", "int", "int"]

# The placements every function is driven with. The first list is the full sweep used by the _s111 key;
# the short one covers the other keys, which exist to select a body rather than to re-walk the clipping.
_FULL = [("$spA", "$d0", 0, 0),        # the whole 16 x 8 sprite inside the destination
         ("$spA", "$d0", 4, 2),        # offset
         ("$spA", "$d0", -3, -2),      # clipped left and top: the source cursor moves
         ("$spA", "$d0", 24, 10),      # clipped right and bottom: cols 8, rows 6
         ("$spA", "$d0", 100, 100),    # empty intersection -> 0 (0x10031a06)
         ("$spA", "$d0", -20, 0),      # entirely left of the clip -> 0
         ("$spA", "$d1", 0, 0),        # the destination's clip trims left and top
         ("$spA", "$d2", 0, 0),        # format 1: Palette slot +0x1c (0x10031926)
         ("$spA", "$d3", 0, 0),        # format 2: the switch default (0x10031939)
         ("$spA", "$d4", 0, 0),        # no pixel base: the guard's lock returns 0 -> 3
         ("$spNB", "$d0", 0, 0),       # no payload -> 3 (0x100317e6)
         ("$spA", 0, 0, 0),            # dst NULL -> 3 (0x100317ec)
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

_FIXTURES = [("", "c3by_s111"), ("_sm11", "c3by_sm11"), ("_s1m1", "c3by_s1m1"),
             ("_sm1m1", "c3by_sm1m1"), ("_s223", "c3by_s223"), ("_sm223", "c3by_sm223"),
             ("_s2m23", "c3by_s2m23"), ("_sm2m23", "c3by_sm2m23"), ("_s323", "c3by_s323"),
             ("_nopal", "c3by_nopal")]

# The fourth argument is a char read inside the loop (`or byte ptr [ebp + 0x14], 0`, 0x10031ae6): 0
# blends every payload byte below 0xff, non-zero skips 0xf8..0xfe as well, and 0x100 proves only the low
# byte is read.
_FLAGS = [(0,), (1,), (0x100,), (1,)]


def _keys(name, addr):
    """Ten keys for one function: the full sweep on (1,1,1), the short sweep on each of the eight other
    body selections, and the no-Palette variant. The flag argument cycles over _FLAGS."""
    out = {}
    for suffix, fixture in _FIXTURES:
        base = _FULL if suffix == "" else (_NOPAL if suffix == "_nopal" else _BODY)
        vectors = [v + _FLAGS[i % len(_FLAGS)] for i, v in enumerate(base)]
        out[name + suffix] = dict(module="jgld.dll", addr=addr, abi="thiscall", ret="int", args=_A4I,
                                  fixture=fixture, state=_STATE, vectors=vectors)
    return out


HOOKS = {}
for _name, _addr in [
    ("Sprite::draw16_100317c0", 0x000317C0),
    ("Sprite::draw16_10032d60", 0x00032D60),
    ("Sprite::draw16_10034300", 0x00034300),
    ("Sprite::draw16_100358b0", 0x000358B0),
    ("Sprite::draw16_10036e60", 0x00036E60),
    ("Sprite::draw16_10038460", 0x00038460),
    ("Sprite::draw16_10039a60", 0x00039A60),
    ("Sprite::draw16_1003b060", 0x0003B060),
    ("Sprite::draw16_1003c660", 0x0003C660),
    ("Sprite::draw16_1003dca0", 0x0003DCA0),
    ("Sprite::draw16_1003f2e0", 0x0003F2E0),
    ("Sprite::draw16_10040930", 0x00040930),
    ("Sprite::draw16_10049ea0", 0x00049EA0),
    ("Sprite::draw16_1004b430", 0x0004B430),
    ("Sprite::draw16_1004ca10", 0x0004CA10),
    ("Sprite::draw16_1004dff0", 0x0004DFF0),
    ("Sprite::draw16_1004f5d0", 0x0004F5D0),
    ("Sprite::draw16_10050bb0", 0x00050BB0),
    ("Sprite::draw16_100521d0", 0x000521D0),
    ("Sprite::draw16_100537f0", 0x000537F0),
    ("Sprite::draw16_10054e20", 0x00054E20),
]:
    HOOKS.update(_keys(_name, _addr))
