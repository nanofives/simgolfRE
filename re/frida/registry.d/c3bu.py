# Batch c3bu: jgld.dll render, the ten Sprite draw rows batch c3bp left over (module-aware hooks; addr is the
# RVA, VA = 0x10000000 + addr).
#   0x0002ed60 Sprite::draw16_1002ed60  Palette colour at 1/4 against the destination, RGB555 fields
#   0x00030290 Sprite::draw16_10030290  destination at 1/4 against the Palette colour, RGB555 fields
#   0x00041f80 Sprite::draw16_10041f80  Palette colour at 1/2, RGB565 fields, result NOT shifted back
#   0x00043430 Sprite::draw16_10043430  Palette colour at 1/4, RGB565 fields, result NOT shifted back
#   0x00044940 Sprite::draw16_10044940  destination at 1/4, RGB565 fields, result NOT shifted back
#   0x00045e50 Sprite::draw16_10045e50  Palette colour at 1/8, RGB565 fields, result NOT shifted back
#   0x000473b0 Sprite::draw16_100473b0  destination at 1/8, RGB565 fields, result NOT shifted back
#   0x00048910 Sprite::draw16_10048910  destination at 15/16, RGB565 fields, result NOT shifted back
#   0x00020430 Sprite::draw16_10020430  an index remap plus a 16-bit destination remap for payload 0xf8
#   0x00062dd0 Sprite::draw_10062dd0    the same pair of tables onto an 8-BIT destination
#
# Every object is allocated by re/frida/js/fixtures.d/c3bu.js on jgld's own Surface (0x1011d0b0), Palette
# (0x1011db10) and Sprite (0x1011d380) vtables; nothing live is passed in.
#
# EVERY key runs with the game's first thread suspended, because that thread is inside the same seven 16-bit
# loop globals (c3bp measured 462/462/462/122/6 changes in 0x1012857c, 0x10128564, 0x10128560, 0x1012856c and
# 0x10128578 over twelve seconds at the menu) and because the three scale globals, which pick which of the
# eight bodies runs, have to be written. The fixture restores all of them, and the whole 8-bit block
# 0x1012861c..0x10128694 that 0x62dd0 uses, before it resumes the thread. See the header of c3bu.js.
#
# One fixture per setting of 0x10122dc0 / 0x10122dc4 / 0x10122dc8, hence one key per setting (the round-5
# rule): _s111 (1,1,1), _sm11 (-1,1,1), _s1m1 (1,-1,1) and _sm1m1 (-1,-1,1) are the four mirror bodies of the
# unscaled half; _s223 (2,2,3), _sm223, _s2m23 and _sm2m23 the four of the 16.16 stepped half (step 1.5 in
# both directions, so the fractional carry of `adc esi` and the row selector `sbb eax, eax` both occur);
# _s323 (3,2,3) enters the stepped half through the SECOND conjunct of the scale test alone; _nopal points
# the palette client at one carrying no Palette, the only way to the `return 16` exit at 0x1002edef.
# 0x62dd0 has no Palette at all (no `return 16` exit), so instead of _nopal it carries _s553 (5,5,3), a
# magnification whose 16.16 step 0x9999 has an odd low half.
#
# State: $pix is the single block holding all six pixel buffers, so one region says which surface changed;
# $l0..$l5 are the per-surface lock windows (surface+0x4c0: base, use count, current base) so a missing or
# extra slot-+0x24 release reads RED; $spr is the five Sprite structures (none of these ten writes a sprite
# field, and the region proves it). The module globals the two stepped halves own and the four table slots
# ARE state regions: the 16-bit block ($gvstep 0x10128540/44, $gtbl 0x10128548, $gpos 0x10128554/58, $gcols
# 0x10128570/74, $gxstep 0x10128580/84/88/8c/90, $growpair the two ints 0x10122e20 and 0x10122e1c address)
# and the 8-bit one ($g8vstep 0x10128624/28, $g8tbl 0x1012862c, $g8pos 0x1012863c/40, $g8cols 0x10128660,
# $g8pix 0x1012866c, $g8step 0x1012867c/80, $g8xstep 0x10128688/8c/90, $g8rowpair the two ints 0x10122e58
# and 0x10122e54 address). Both blocks are regions of EVERY key, so a 16-bit member that touched the 8-bit
# block (or the other way round) would read RED. The SHARED loop globals are NOT regions, for the reason
# c3av recorded for 0x1012847c: they belong to the module, not to a vector. What they are set to is still
# verified through $pix, deterministically (a wrong source pitch, column count or row skip puts the written
# pixels at the wrong offsets and a wrong row count writes the wrong number of rows).

_STATE = ([("$pix", 0, 0x6000), ("$spr", 0, 0x280)] +
          [(f"$l{i}", 0, 0x14) for i in range(6)] +
          [("$gvstep", 0, 8), ("$gtbl", 0, 4), ("$gpos", 0, 8),
           ("$gcols", 0, 8), ("$gxstep", 0, 0x14), ("$growpair", 0, 8),
           ("$g8vstep", 0, 8), ("$g8tbl", 0, 4), ("$g8pos", 0, 8), ("$g8cols", 0, 4),
           ("$g8pix", 0, 4), ("$g8step", 0, 8), ("$g8xstep", 0, 0xC), ("$g8rowpair", 0, 8)])

# this, dst, x, y, flag            (the eight blend members: the fourth argument is a char)
_A4I = ["pointer", "pointer", "int", "int", "int"]
# this, dst, x, y, indexMap, pixelMap   (0x20430 and 0x62dd0)
_A5 = ["pointer", "pointer", "int", "int", "pointer", "pointer"]

# The placements every function is driven with. The first list is the full sweep used by the _s111 key; the
# short one covers the other keys, which exist to select a body rather than to re-walk the clipping.
_FULL = [("$spA", "$d0", 0, 0),        # the whole 16 x 8 sprite inside the destination
         ("$spA", "$d0", 4, 2),        # offset
         ("$spA", "$d0", -3, -2),      # clipped left and top: the source cursor moves
         ("$spA", "$d0", 24, 10),      # clipped right and bottom: cols 8, rows 6
         ("$spA", "$d0", 100, 100),    # empty intersection -> 0 (0x1002efa6)
         ("$spA", "$d0", -20, 0),      # entirely left of the clip -> 0
         ("$spA", "$d1", 0, 0),        # the destination's clip trims left and top
         ("$spA", "$d2", 0, 0),        # format 1: Palette slot +0x1c (0x1002eebc)
         ("$spA", "$d3", 0, 0),        # format 2: the switch default (0x1002eecd)
         ("$spA", "$d4", 0, 0),        # no pixel base: the guard's lock returns 0 -> 3
         ("$spNB", "$d0", 0, 0),       # no payload -> 3 (0x1002ed86)
         ("$spA", 0, 0, 0),            # dst NULL -> 3 (0x1002ed8c)
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

_SCALES = [("", "c3bu_s111"), ("_sm11", "c3bu_sm11"), ("_s1m1", "c3bu_s1m1"), ("_sm1m1", "c3bu_sm1m1"),
           ("_s223", "c3bu_s223"), ("_sm223", "c3bu_sm223"), ("_s2m23", "c3bu_s2m23"),
           ("_sm2m23", "c3bu_sm2m23"), ("_s323", "c3bu_s323")]
_FIXTURES = _SCALES + [("_nopal", "c3bu_nopal")]
# 0x62dd0 reads no Palette, so the no-Palette variant would exercise nothing: it gets the odd-step
# magnification instead.
_FIXTURES8 = _SCALES + [("_s553", "c3bu_s553")]


def _keys(name, addr, args, extra, fixtures=_FIXTURES):
    """Ten keys for one function: the full sweep on (1,1,1), the short sweep on each of the eight other
    body selections, and a tenth variant. `extra` is the tuple of extra arguments appended to every vector
    (a list of alternatives, cycled)."""
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

# ---- the eight blend members. The fourth argument is a char read inside the loop
#      (`or byte ptr [ebp + 0x14], 0`, 0x1002f086): 0 blends every payload byte below 0xff, non-zero skips
#      0xf8..0xfe as well, and 0x100 proves only the low byte is read.
_FLAGS = [(0,), (1,), (0x100,), (1,)]
HOOKS.update(_keys("Sprite::draw16_1002ed60", 0x0002ED60, _A4I, _FLAGS))
HOOKS.update(_keys("Sprite::draw16_10030290", 0x00030290, _A4I, _FLAGS))
HOOKS.update(_keys("Sprite::draw16_10041f80", 0x00041F80, _A4I, _FLAGS))
HOOKS.update(_keys("Sprite::draw16_10043430", 0x00043430, _A4I, _FLAGS))
HOOKS.update(_keys("Sprite::draw16_10044940", 0x00044940, _A4I, _FLAGS))
HOOKS.update(_keys("Sprite::draw16_10045e50", 0x00045E50, _A4I, _FLAGS))
HOOKS.update(_keys("Sprite::draw16_100473b0", 0x000473B0, _A4I, _FLAGS))
HOOKS.update(_keys("Sprite::draw16_10048910", 0x00048910, _A4I, _FLAGS))

# ---- 0x20430: the fourth argument is the 256-byte index remap parked at 0x10128548 (0x1002053f), the fifth
#      the 16-bit table parked at 0x10128574 (0x10020547) that payload 0xf8 indexes with the DESTINATION
#      pixel. Two different tables per slot so a vector proves which pointer each loop read.
HOOKS.update(_keys("Sprite::draw16_10020430", 0x00020430, _A5,
                   [("$m", "$t"), ("$m2", "$t2"), ("$m", "$t2"), ("$m2", "$t")]))

# ---- 0x62dd0: the same pair, parked at 0x1012862c (0x10062e92) and 0x1012866c (0x10062e9b); both are
#      256-byte tables here because the destination is 8-bit. The colour table it reads at 0x10128658
#      (0x10063023) is one the fixture installs: the function never writes it.
HOOKS.update(_keys("Sprite::draw_10062dd0", 0x00062DD0, _A5,
                   [("$m", "$n"), ("$m2", "$n2"), ("$m", "$n2"), ("$m2", "$n")], _FIXTURES8))
