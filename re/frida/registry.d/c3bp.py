# Batch c3bp: jgld.dll render, the Sprite::draw16_* / draw_* family (module-aware hooks; addr is the RVA,
# VA = 0x10000000 + addr).
#   0x0001cde0 Sprite::draw16_1001cde0  8-bit payload through the Palette table, transparent at 0xff
#   0x00056450 Sprite::draw16_10056450  the same, drawn only where both loop counters are even
#   0x000576a0 Sprite::draw16_100576a0  the same with a third test on bit 1 of the counters' sum
#   0x0001f2d0 Sprite::draw_1001f2d0    one flat colour wherever the payload is below 0xfe
#   0x00026240 Sprite::draw16_10026240  the payload index remapped through a 256-byte table first
#   0x00027430 Sprite::draw16_10027430  the Palette colour remapped through a 16-bit table after
#   0x00028630 Sprite::draw16_10028630  payload 0xf8 remaps the DESTINATION pixel through that table
#   0x000298b0 Sprite::draw16_100298b0  payload 0xf8..0xfe picks a plane of a (payload, destination) table
#   0x0002bcf0 Sprite::draw_1002bcf0    payload 0 writes 0, 1..0xf pick a plane, no Palette table at all
#   0x0002d890 Sprite::draw16_1002d890  the Palette colour averaged with the destination, gated by a flag
#
# Every object is allocated by re/frida/js/fixtures.d/c3bp.js on jgld's own Surface (0x1011d0b0), Palette
# (0x1011db10) and Sprite (0x1011d380) vtables; nothing live is passed in. log/c3/alt/c3bp_probe.py, one 12 s
# boot at the menu with counters attached straight to the module addresses: 0 entries into all ten functions
# and into each of their static callers (0x15180, 0x193d0, 0x15f80, 0x169a0, 0x16cb0, 0x165a0, 0x19830,
# 0x18120, 0x18920), against 1_704 into the live twin Sprite::draw16_1001bc40 0x1bc40.
#
# EVERY key runs with the game's first thread suspended, because that thread is inside the same seven loop
# globals (the probe counted 462/462/462/122/6 changes in 0x1012857c, 0x10128564, 0x10128560, 0x1012856c and
# 0x10128578 in those twelve seconds) and because the three scale globals, which pick which of the eight
# bodies runs, have to be written. The fixture restores all of them before it resumes the thread. See the
# header of c3bp.js.
#
# One fixture per setting of 0x10122dc0 / 0x10122dc4 / 0x10122dc8, hence one key per setting (the round-5
# rule): _s111 (1,1,1), _sm11 (-1,1,1), _s1m1 (1,-1,1) and _sm1m1 (-1,-1,1) are the four mirror bodies of the
# unscaled half; _s223 (2,2,3), _sm223, _s2m23 and _sm2m23 the four of the 16.16 stepped half (step 1.5 in
# both directions, so the fractional carry of `adc esi` and the row selector `sbb eax, eax` both occur);
# _s323 (3,2,3) enters the stepped half through the SECOND conjunct of the scale test alone; _nopal points
# the palette client at one carrying no Palette, the only way to the `return 16` exit at 0x1001ce6f.
#
# State: $pix is the single block holding all six pixel buffers, so one region says which surface changed;
# $l0..$l5 are the per-surface lock windows (surface+0x4c0: base, use count, current base) so a missing or
# extra slot-+0x24 release reads RED; $spr is the five Sprite structures (none of these ten writes a sprite
# field, and the region proves it). The module globals the stepped half and the three parameter slots use ARE
# state regions ($gflat 0x10128530, $gvstep 0x10128540/44, $gtbl 0x10128548, $gpos 0x10128554/58, $gcols
# 0x10128570/74, $gxstep 0x10128580/84/88/8c/90, $growpair the two ints 0x10122e20 and 0x10122e1c address):
# the probe measured 0 changes in each over 12 s and no function that writes them is ever entered by the
# game, and the thread that could is suspended. The SEVEN SHARED loop globals are NOT, for the reason c3av
# recorded for 0x1012847c: they belong to the module, not to a vector. What they are set to is still verified
# through $pix, deterministically (a wrong source pitch, column count or row skip puts the written pixels at
# the wrong offsets and a wrong row count writes the wrong number of rows).

_STATE = ([("$pix", 0, 0x6000), ("$spr", 0, 0x280)] +
          [(f"$l{i}", 0, 0x14) for i in range(6)] +
          [("$gflat", 0, 4), ("$gvstep", 0, 8), ("$gtbl", 0, 4), ("$gpos", 0, 8),
           ("$gcols", 0, 8), ("$gxstep", 0, 0x14), ("$growpair", 0, 8)])

# this, dst, x, y [, fourth argument]
_A3 = ["pointer", "pointer", "int", "int"]
_A4 = ["pointer", "pointer", "int", "int", "pointer"]   # the table-driven members
_A4I = ["pointer", "pointer", "int", "int", "int"]      # 0x1f2d0's colour and 0x2d890's flag

# The placements every function is driven with. The first list is the full sweep used by the _s111 key; the
# short one covers the other nine, which exist to select a body rather than to re-walk the clipping.
_FULL = [("$spA", "$d0", 0, 0),        # the whole 16 x 8 sprite inside the destination
         ("$spA", "$d0", 4, 2),        # offset
         ("$spA", "$d0", -3, -2),      # clipped left and top: the source cursor moves
         ("$spA", "$d0", 24, 10),      # clipped right and bottom: cols 8, rows 6
         ("$spA", "$d0", 100, 100),    # empty intersection -> 0 (0x1001d026)
         ("$spA", "$d0", -20, 0),      # entirely left of the clip -> 0
         ("$spA", "$d1", 0, 0),        # the destination's clip trims left and top
         ("$spA", "$d2", 0, 0),        # format 1: Palette slot +0x1c (0x1001cf3c)
         ("$spA", "$d3", 0, 0),        # format 2: the switch default
         ("$spA", "$d4", 0, 0),        # no pixel base: the guard's lock returns 0 -> 3
         ("$spNB", "$d0", 0, 0),       # no payload -> 3 (0x1001ce0a)
         ("$spA", 0, 0, 0),            # dst NULL -> 3 (0x1001ce10)
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

_FIXTURES = [("", "c3bp_s111"), ("_sm11", "c3bp_sm11"), ("_s1m1", "c3bp_s1m1"), ("_sm1m1", "c3bp_sm1m1"),
             ("_s223", "c3bp_s223"), ("_sm223", "c3bp_sm223"), ("_s2m23", "c3bp_s2m23"),
             ("_sm2m23", "c3bp_sm2m23"), ("_s323", "c3bp_s323"), ("_nopal", "c3bp_nopal")]
# Only 0x56450 and 0x576a0 need these four: they are the two that test bit 0 of the 16.16 horizontal
# position in their stepped halves, and (+-5, +-5, 3) is the setting whose step 0x9999 makes it odd.
_ODD = [("_s553", "c3bp_s553"), ("_sm553", "c3bp_sm553"), ("_s5m53", "c3bp_s5m53"),
        ("_sm5m53", "c3bp_sm5m53")]


def _keys(name, addr, args, extra, fixtures=_FIXTURES):
    """Ten keys for one function: the full sweep on (1,1,1), the short sweep on each of the eight other
    body selections, and the no-Palette variant. `extra` is the tuple appended to every vector (empty for
    the three-argument members, one element for the others; a list of alternatives is cycled)."""
    out = {}
    for suffix, fixture in fixtures:
        base = _FULL if suffix == "" else (_NOPAL if suffix == "_nopal" else _BODY)
        vectors = []
        for i, v in enumerate(base):
            vectors.append(v + (extra[i % len(extra)],) if extra else v)
        out[name + suffix] = dict(module="jgld.dll", addr=addr, abi="thiscall", ret="int", args=args,
                                  fixture=fixture, state=_STATE, vectors=vectors)
    return out


HOOKS = {}

# ---- 0x1cde0: the cold twin of the live 0x1bc40 (threshold 0xff at 0x1001d101 against 0xfe at 0x1001bf61).
HOOKS.update(_keys("Sprite::draw16_1001cde0", 0x0001CDE0, _A3, ()))

# ---- 0x56450 / 0x576a0: the same with the stipple tests at 0x10056778 / 0x10056782 and 0x100579c8 /
#      0x100579d2 / 0x100579df. The placements matter more here than anywhere else, because what is drawn
#      depends on the parity of the clipped column and row counts, which x, y and the clip rectangle set.
HOOKS.update(_keys("Sprite::draw16_10056450", 0x00056450, _A3, ()))
HOOKS.update(_keys("Sprite::draw16_100576a0", 0x000576A0, _A3, ()))
HOOKS.update(_keys("Sprite::draw16_10056450", 0x00056450, _A3, (), _ODD))
HOOKS.update(_keys("Sprite::draw16_100576a0", 0x000576A0, _A3, (), _ODD))

# ---- 0x1f2d0: the fourth argument is the colour. Negative values ARE the colour (0x1001f3ea), non-negative
#      ones index the Palette table the format code selects with their low byte (0x1001f438 / 0x1001f462).
#      The cycle is ordered so that the format-2 destination $d3 (the ninth placement) gets a NON-negative
#      colour: with a negative one the switch is skipped and the `cmp [ebp - 0x44], 1` at 0x1001f41d can
#      never fall through to the default.
HOOKS.update(_keys("Sprite::draw_1001f2d0", 0x0001F2D0, _A4I,
                   (-1, 0x5, 0x1FF, -0x1235, 0, 0x3C)))

# ---- 0x26240 / 0x27430 / 0x28630: the fourth argument is a table pointer parked at 0x10128548. Two
#      different tables per function so a vector proves which pointer the loop read.
HOOKS.update(_keys("Sprite::draw16_10026240", 0x00026240, _A4, ("$m", "$m2")))
HOOKS.update(_keys("Sprite::draw16_10027430", 0x00027430, _A4, ("$t", "$t2")))
HOOKS.update(_keys("Sprite::draw16_10028630", 0x00028630, _A4, ("$t", "$t2")))

# ---- 0x298b0: the same, parked at 0x10128574 instead (0x100299bf), which is also where the loops read it
#      (0x10029bbe); this function touches 0x10128548 nowhere.
HOOKS.update(_keys("Sprite::draw16_100298b0", 0x000298B0, _A4, ("$t", "$t2")))

# ---- 0x2bcf0: the table at 0x10128548 carries every colour; no Palette table is read at all.
HOOKS.update(_keys("Sprite::draw_1002bcf0", 0x0002BCF0, _A4, ("$t", "$t2")))

# ---- 0x2d890: the fourth argument is a char flag read inside the loop (0x1002dbb6). 0 blends every payload
#      byte below 0xff, non-zero skips 0xf8..0xfe as well; 0x100 proves only the low byte is read.
HOOKS.update(_keys("Sprite::draw16_1002d890", 0x0002D890, _A4I, (0, 1, 0x100, 1)))
