# Batch c3ci: the jgld.dll Sprite methods reached only through a slot of the Sprite vtable at RVA 0x11d380
# (module-aware hooks; addr is the RVA, VA = 0x10000000 + addr). Sprite::ctor 0x10014af0, Sprite::ctorN
# 0x10014cd0 and Sprite::dtor 0x10014de0 store that table at +0 of every Sprite.
#
#   accessors      0x00017eb0 setField10 (slot 4)   0x00017ef0 getField10 (slot 5)
#                  0x00017f30 getField28 (slot 7)   0x00017f70 getField14 (slot 8)
#                  0x00017fe0 getField20 (slot 10)  0x00018020 getField2c (slot 11)
#                  0x00018060 getField30 (slot 12)  0x000180a0 getField34 (slot 13)
#   remap          0x000150d0 (slot 14)             forwards to remap8 0x1bae0 when +0x20 is 8
#   drawQ          0x000175d0 (slot 24)             reaches no blitter: every arm is a debug report
#   blt forwarders 0x00019830 blt16a (slot 30) -> 0x298b0   0x00019900 blt16b (slot 18) -> 0x22a10
#                  0x000199d0 blt16c (slot 19) -> 0x24140   0x00019aa0 bltS   (slot 21) -> 0x25dd0
#                  0x00019b90 bltS2  (slot 20) -> 0x25900
#   (the two line slots 0x00018490 lineTo and 0x00018510 dashTo were prepared and dropped: see below)
#   dispatchers    0x00015180 draw  (slot 23) -> 0x5dde0 / 0x1cde0
#                  0x00016270 drawF (slot 28) -> 0x60cf0
#                  0x000169a0 drawH (slot 16) -> 0x61db0 / 0x26240
#                  0x00016fc0 drawJ (slot 26) -> 0x62dd0
#                  0x000178a0 drawL (slot 32) -> 0x63f60
#
# Every Surface, Palette, Sprite, payload and lookup table is allocated by re/frida/js/fixtures.d/c3ci.js on
# jgld's own vtables; nothing live is passed in or written.
#
# TWO FIXTURE FAMILIES.
#   c3ci_plain   freezes nothing and writes no module global: the eight accessors, remap (whose callee
#                remap8 0x1bae0 is a leaf that touches only the sprite's own payload) and drawQ (whose every
#                arm is a _CrtDbgReport) reach no blitter.
#   c3ci_s111 / c3ci_s223
#                suspend the game's first thread for the key's vectors and restore 0x101284d0..0x10128698,
#                the three scale globals and the palette client before resuming. The thread that draws the
#                menu is inside the same loop globals (it enters Sprite::draw16_1001bc40 and
#                Sprite::draw_1005ed80 thousands of times in twelve seconds, c3bz's probe) and DECREMENTS the
#                row counters 0x10128578 / 0x10128670 in memory, so a blitter driven without the freeze ends
#                its loop early or never. _s111 writes (1, 1, 1) into 0x10122dc0 / dc4 / dc8 (the unscaled
#                body of every blitter) and _s223 writes (2, 2, 3) (the 16.16 stepped body); the functions of
#                this batch read none of the three themselves, so the second key is spread and a check that
#                they are agnostic, not a branch of their own.
#
# STATE. $spr is the whole 25-sprite block: every dispatcher and every forwarder writes [sprite + 0x10], and
# the forwarders and the two early returns of the dispatchers leave it changed, which this region records.
# $pix holds all fourteen pixel buffers and $l0..$l13 the per-surface lock windows (surface + 0x4c0: base,
# use count, handed-out base), so a missing or extra slot-+0x24 release reads RED. $spix is the payload
# block, which remap8 rewrites in place and every blitter only reads. The module globals the blitters own are
# regions of every blitter-reaching key, the 16-bit block and the 8-bit one together, so a function that
# touched the wrong one would read RED. The SHARED loop globals (0x1012857c, 0x1012855c, 0x1012856c,
# 0x10128578, 0x10128564, 0x1012853c, 0x10128560 and their 8-bit twins) are NOT regions, for the reason c3av
# recorded for 0x1012847c: the game writes them. What they are set to is still verified through $pix.

_NSURF = 14
_PLAIN = [("$spr", 0, 0xC80)]
_PLAIN_PAY = _PLAIN + [("$spix", 0, 0x10000)]
_BLIT = ([("$pix", 0, _NSURF * 0x1000), ("$spr", 0, 0xC80), ("$spix", 0, 0x10000)] +
         [("$l%d" % i, 0, 0x14) for i in range(_NSURF)] +
         [("$gmask", 0, 0xC), ("$gvstep", 0, 8), ("$gtbl", 0, 4), ("$gpos", 0, 8),
          ("$glevel", 0, 4), ("$gcols", 0, 8), ("$gxstep", 0, 0x14), ("$growpair", 0, 8),
          ("$g8vstep", 0, 8), ("$g8tbl", 0, 4), ("$g8pos", 0, 8), ("$g8cols", 0, 4),
          ("$g8colour", 0, 4), ("$g8pix", 0, 4), ("$g8step", 0, 8), ("$g8xstep", 0, 0xC),
          ("$g8rowpair", 0, 8)])

# The sixteen sprites whose +0x10 is a small distinct integer (the accessor, remap and drawQ keys).
_SP = ["$spA", "$spB", "$spNP", "$spD", "$spNB", "$spW", "$spBsy", "$sp16", "$spX",
       "$a0", "$a1", "$a2", "$a3", "$a4", "$a5", "$a6"]

_THIS = ["pointer"]
# dst, x, y, table, Palette override. THE LAST ARGUMENT OF EVERY SLOT HERE IS A PALETTE POINTER, not a colour
# index: each body stores it into [sprite + 0x10] when it is not zero, and that word is where the blitters
# take their colour table from (+0x40c for the RGB565 table, +0x60c for RGB555). Measured, not assumed: the
# first run of Sprite::blt16a passed 0x33 and 0x7f there and 0x100298b0 faulted reading 0x33 and 0x7f
# (log/c3/alt/c3ci_bisect.py, vectors 1 and 16). The vectors below pass 0 or one of the fixture's own two
# Palettes, so a vector that overrides proves which table the blitter then read.
_A_DST4 = ["pointer", "pointer", "int", "int", "pointer", "pointer"]

HOOKS = {}

# ======================================================================== the accessor slots
# Sixteen vectors each, one per sprite, which is the leaf rule's minimum of ten with room to spare. The
# fixture gives the sixteen sprites distinct values in every field these slots read: +0x10 runs 0x11..0x19
# and 0x21..0x27 (16 results), +0x14 fourteen different payload addresses and two NULLs, +0x20 ten different
# bit depths, +0x28 eleven values, +0x2c nine pitches, +0x30 ten widths and +0x34 ten heights.

HOOKS["Sprite::getField10"] = dict(
    module="jgld.dll", addr=0x00017EF0, abi="thiscall", ret="int", args=_THIS,
    fixture="c3ci_plain", state=_PLAIN, vectors=[(s,) for s in _SP])
HOOKS["getField28"] = dict(
    module="jgld.dll", addr=0x00017F30, abi="thiscall", ret="int", args=_THIS,
    fixture="c3ci_plain", state=_PLAIN, vectors=[(s,) for s in _SP])
HOOKS["getField14"] = dict(
    module="jgld.dll", addr=0x00017F70, abi="thiscall", ret="pointer", args=_THIS,
    fixture="c3ci_plain", state=_PLAIN, vectors=[(s,) for s in _SP])
HOOKS["getField20"] = dict(
    module="jgld.dll", addr=0x00017FE0, abi="thiscall", ret="int", args=_THIS,
    fixture="c3ci_plain", state=_PLAIN, vectors=[(s,) for s in _SP])
HOOKS["getField2c"] = dict(
    module="jgld.dll", addr=0x00018020, abi="thiscall", ret="int", args=_THIS,
    fixture="c3ci_plain", state=_PLAIN, vectors=[(s,) for s in _SP])
HOOKS["Sprite::getField30"] = dict(
    module="jgld.dll", addr=0x00018060, abi="thiscall", ret="int", args=_THIS,
    fixture="c3ci_plain", state=_PLAIN, vectors=[(s,) for s in _SP])
HOOKS["Sprite::getField34"] = dict(
    module="jgld.dll", addr=0x000180A0, abi="thiscall", ret="int", args=_THIS,
    fixture="c3ci_plain", state=_PLAIN, vectors=[(s,) for s in _SP])

# the only writer of the eight: its evidence is $spr, since the body returns nothing
HOOKS["Sprite::setField10"] = dict(
    module="jgld.dll", addr=0x00017EB0, abi="thiscall", ret="void", args=["pointer", "int"],
    fixture="c3ci_plain", state=_PLAIN,
    vectors=[("$spA", 0), ("$spA", 1), ("$spA", -1), ("$spA", 0x7FFFFFFF), ("$spA", -0x80000000),
             ("$spB", 0x12345678), ("$spNP", 0xABCDEF), ("$spD", 2), ("$spNB", 3), ("$spW", 4),
             ("$spBsy", 5), ("$sp16", 6), ("$spX", 7), ("$a0", 0x55), ("$a3", 0x66), ("$a6", 0x77)])

# ======================================================================== remap, slot 14
# Both sides of the only test of the body (`cmp dword ptr [ebp - 8], 8` VA 0x100150f6) and, through the
# callee, both sides of remap8's payload test and of its span/rectangle test: spA, spB, spNP, spD and spW
# have bit 0 of +0x18 clear so remap8 walks pitch * height bytes (128, 200, 128, 4, 128), spBsy has it set so
# it walks the +0x24 count (0x36, whose remainder after the four-at-a-time loop is 2), and spNB has no
# payload at all. The a* sprites and sp16 / spX carry bits per pixel 1, 2, 4, 0x10, 0x18, 0x20, 0x30, 0x40
# and take the report arm.
HOOKS["Sprite::remap"] = dict(
    module="jgld.dll", addr=0x000150D0, abi="thiscall", ret="int", args=["pointer", "int", "int"],
    fixture="c3ci_plain", state=_PLAIN_PAY,
    vectors=[("$spA", 0x00, 0x7F), ("$spA", 0xA5, 0x01), ("$spA", 0xFF, 0x10),
             ("$spB", 0x3C, 0x05), ("$spNP", 0x0F, 0xFE), ("$spD", 0x05, 0x33),
             ("$spNB", 0x01, 0x02), ("$spW", 0x10, 0x20), ("$spBsy", 0x7F, 0x00),
             ("$spBsy", 0x13C, 0x1FF), ("$sp16", 0x01, 0x02), ("$spX", 0x01, 0x02),
             ("$a0", 0x01, 0x02), ("$a3", 0x01, 0x02)])

# ======================================================================== drawQ, slot 24
# Sprite bits per pixel 8 (spA, spBsy) takes the report at line base + 0xf with the result left 0; 0x10
# (sp16) tests the destination's depth, 8 reporting at + 0x29 with the result 0 and anything else at + 0x2f
# with 0x17; every other bits-per-pixel value (spX, a0, a3) reports at + 0x37 with 0x17. A NULL destination
# gives 0x10 and an invalid sprite 7, both without restoring [sprite + 0x10], which the key vectors and $spr
# show.
HOOKS["Sprite::drawQ"] = dict(
    module="jgld.dll", addr=0x000175D0, abi="thiscall", ret="int",
    args=["pointer", "pointer", "int", "int", "int", "int", "pointer"],
    fixture="c3ci_plain", state=_PLAIN,
    vectors=[("$spA", "$d0", 1, 2, 3, 4, 0),
             ("$spA", "$d0", 1, 2, 3, 4, "$pal"),
             ("$spA", 0, 0, 0, 0, 0, 0),
             ("$spA", 0, 0, 0, 0, 0, "$pal2"),
             ("$spNB", "$d0", 0, 0, 0, 0, 0),
             ("$spNB", "$d0", 0, 0, 0, 0, "$pal"),
             ("$sp16", "$d0", 0, 0, 0, 0, 0),
             ("$sp16", "$e0", 0, 0, 0, 0, 0),
             ("$sp16", "$x0", 0, 0, 0, 0, 0x55),   # a value that is no Palette: this slot never follows it
             ("$sp16", "$x1", 0, 0, 0, 0, 0),
             ("$sp16", "$x2", 0, 0, 0, 0, 0),
             ("$spX", "$d0", 0, 0, 0, 0, 0),
             ("$a0", "$d0", 0, 0, 0, 0, 0),
             ("$a3", "$e0", 0, 0, 0, 0, "$pal2"),
             ("$spBsy", "$d0", 0, 0, 0, 0, 0)])

# ======================================================================== the five blt forwarders
# None of the five tests the destination for NULL: the first thing each does is call slot +0xe4 on it, so a
# NULL destination faults in the original and no vector passes one.

_BLT16A = [("$bA", "$d0", 0, 0, "$t", 0),
           ("$bA", "$d0", 4, 2, "$t2", "$pal2"),   # the Palette override (not restored) and the other table
           ("$bA", "$d1", -3, -2, "$t", 0),        # the destination's clip trims left and top
           ("$bA", "$d2", 0, 0, "$t", 0),          # format 1: the RGB555 table
           ("$bA", "$d3", 0, 0, "$t", 0),          # format 2: the blitter's switch default
           ("$bA", "$d4", 0, 0, "$t", 0),          # no pixel base: the blitter's lock gives 0
           ("$bB", "$d0", 1, 1, "$t2", 0),         # the other Palette and alphabet phase
           ("$bNP", "$d0", 0, 0, "$t", 0),         # no Palette of its own: the client's
           ("$bD", "$d0", 5, 5, "$t", 0),          # 1 x 1
           ("$bA", "$d5", 0, 0, "$t", 0),          # a 24-wide destination
           ("$bNB", "$d0", 0, 0, "$t", 0),         # no payload: the guards pass, the blitter gives 3
           ("$bA", "$e0", 0, 0, "$t", 0),          # depth 8: the first guard gives 0x17
           ("$bA", "$x0", 0, 0, "$t", 0),          # depth 24: the same
           ("$b16", "$d0", 0, 0, "$t", 0),         # bits per pixel 16: the second guard gives 0x17
           ("$bX", "$d0", 0, 0, "$t", 0),          # bits per pixel 24: the same
           ("$bA", "$d0", 100, 100, "$t", 0),      # empty intersection: the blitter gives 0
           ("$bA", "$d0", 0, 0, "$t2", "$pal"),
           ("$bBsy", "$d0", 0, 0, "$t", 0)]        # bit 0 of +0x18 set: this slot has no busy test
_SHORT16A = [("$bA", "$d0", 0, 0, "$t", 0), ("$bA", "$d0", 4, 2, "$t2", "$pal2"),
             ("$bA", "$d1", -3, -2, "$t", 0), ("$bB", "$d0", 1, 1, "$t", 0),
             ("$bA", "$e0", 0, 0, "$t", 0), ("$bA", "$d0", 100, 100, "$t", 0)]

HOOKS["Sprite::blt16a"] = dict(
    module="jgld.dll", addr=0x00019830, abi="thiscall", ret="int", args=_A_DST4,
    fixture="c3ci_s111", state=_BLIT, vectors=_BLT16A)
HOOKS["Sprite::blt16a_s223"] = dict(
    module="jgld.dll", addr=0x00019830, abi="thiscall", ret="int", args=_A_DST4,
    fixture="c3ci_s223", state=_BLIT, vectors=_SHORT16A)

# blt16b / blt16c: (dst, x, y, colour, key, flag). 0x12340a5b and 0x1ff prove that the callee reads only the
# low word of the colour and the low byte of the flag, which is what the two dirty pushes at VA 0x10019971 /
# 0x10019975 (and VA 0x10019a41 / 0x10019a45) carry.
_ABC = ["pointer", "pointer", "int", "int", "int", "pointer", "int"]
_BLT16B = [("$bA", "$d0", 0, 0, 0x7FFF, 0, 0),
           ("$bA", "$d0", 4, 2, 0x0421, "$pal2", 1),
           ("$bA", "$d1", -3, -2, 0x001F, 0, 0x5A),
           ("$bA", "$d2", 0, 0, 0x03E0, 0, 0),
           ("$bA", "$d3", 0, 0, 0x1234, 0, 0),
           ("$bA", "$d4", 0, 0, 0x0A5B, 0, 0),
           ("$bB", "$d0", 1, 1, 0x12340A5B, 0, 0x1FF),
           ("$bNP", "$d0", 0, 0, 0x0101, 0, 0),
           ("$bD", "$d0", 5, 5, 0x7FFF, 0, 0),
           ("$bA", "$d5", 0, 0, 0x0421, 0, 0),
           ("$bNB", "$d0", 0, 0, 0x7FFF, 0, 0),
           ("$bA", "$e0", 0, 0, 0x7FFF, 0, 0),
           ("$bA", "$x0", 0, 0, 0x7FFF, 0, 0),
           ("$b16", "$d0", 0, 0, 0x7FFF, 0, 0),
           ("$bX", "$d0", 0, 0, 0x7FFF, 0, 0),
           ("$bA", "$d0", 100, 100, 0x7FFF, 0, 0),
           ("$bA", "$d0", 0, 0, 0x03E0, "$pal", 1),
           ("$bBsy", "$d0", 0, 0, 0x001F, 0, 0)]
_SHORT16B = [("$bA", "$d0", 0, 0, 0x7FFF, 0, 0), ("$bA", "$d0", 4, 2, 0x0421, "$pal2", 1),
             ("$bA", "$d1", -3, -2, 0x001F, 0, 0), ("$bB", "$d0", 1, 1, 0x03E0, 0, 0),
             ("$bA", "$e0", 0, 0, 0x7FFF, 0, 0), ("$bA", "$d0", 100, 100, 0x7FFF, 0, 0)]

HOOKS["Sprite::blt16b"] = dict(
    module="jgld.dll", addr=0x00019900, abi="thiscall", ret="int", args=_ABC,
    fixture="c3ci_s111", state=_BLIT, vectors=_BLT16B)
HOOKS["Sprite::blt16b_s223"] = dict(
    module="jgld.dll", addr=0x00019900, abi="thiscall", ret="int", args=_ABC,
    fixture="c3ci_s223", state=_BLIT, vectors=_SHORT16B)
HOOKS["Sprite::blt16c"] = dict(
    module="jgld.dll", addr=0x000199D0, abi="thiscall", ret="int", args=_ABC,
    fixture="c3ci_s111", state=_BLIT, vectors=_BLT16B)
HOOKS["Sprite::blt16c_s223"] = dict(
    module="jgld.dll", addr=0x000199D0, abi="thiscall", ret="int", args=_ABC,
    fixture="c3ci_s223", state=_BLIT, vectors=_SHORT16B)

# bltS (other, dst, x, y, pal): the third guard is the OTHER sprite's bits per pixel, which $b16 and $bX
# fail. blitPair1 reads and writes the one Surface.
# NO VECTOR PASSES $d4 (a Surface with no pixel base) OR $bNB (a Sprite with no payload), through either
# sprite argument: blitPair1 0x10025dd0 and blitPair 0x10025900 test neither and fault in the ORIGINAL on
# all three (measured, log/c3/alt/c3ci_bisect.py: vectors 5, 10 and 11 of the first Sprite::bltS run read
# "access violation accessing 0x0"). c3bm, which took the two blitters to C3, passed neither either. The two
# guards bltS does make are still both-sided through the depth and bits-per-pixel vectors below.
_ABLTS = ["pointer", "pointer", "pointer", "int", "int", "pointer"]
_BLTS = [("$bA", "$bW", "$d0", 0, 0, 0),
         ("$bA", "$bW", "$d0", 4, 2, "$pal2"),
         ("$bA", "$bW", "$d1", -3, -2, 0),
         ("$bA", "$bW", "$d2", 0, 0, 0),
         ("$bA", "$bW", "$d3", 0, 0, 0),
         ("$bA", "$bW", "$d0", 24, 10, 0),
         ("$bB", "$bW", "$d0", 1, 1, 0),
         ("$bNP", "$bW", "$d0", 0, 0, 0),
         ("$bA", "$bB", "$d0", 0, 0, 0),
         ("$bD", "$bW", "$d0", 5, 5, 0),
         ("$bA", "$bW", "$d0", -20, 0, 0),
         ("$bA", "$bW", "$d0", 0, 15, 0),
         ("$bA", "$bW", "$e0", 0, 0, 0),
         ("$bA", "$bW", "$x0", 0, 0, 0),
         ("$b16", "$bW", "$d0", 0, 0, 0),
         ("$bA", "$b16", "$d0", 0, 0, 0),
         ("$bA", "$bX", "$d0", 0, 0, 0),
         ("$bA", "$bW", "$d0", 100, 100, 0),
         ("$bA", "$bW", "$d5", 0, 0, "$pal")]
_SHORTS = [("$bA", "$bW", "$d0", 0, 0, 0), ("$bA", "$bW", "$d0", 4, 2, "$pal2"),
           ("$bA", "$bW", "$d1", -3, -2, 0), ("$bB", "$bW", "$d0", 1, 1, 0),
           ("$bA", "$bW", "$e0", 0, 0, 0), ("$bA", "$bW", "$d0", 100, 100, 0)]

HOOKS["Sprite::bltS"] = dict(
    module="jgld.dll", addr=0x00019AA0, abi="thiscall", ret="int", args=_ABLTS,
    fixture="c3ci_s111", state=_BLIT, vectors=_BLTS)
HOOKS["Sprite::bltS_s223"] = dict(
    module="jgld.dll", addr=0x00019AA0, abi="thiscall", ret="int", args=_ABLTS,
    fixture="c3ci_s223", state=_BLIT, vectors=_SHORTS)

# bltS2 (other, dst, dst2, x, y, pal): the guard tests the FIRST Surface's depth, and blitPair writes the
# SECOND. The $d6 / $d0 / $d5 third arguments show that it is the second one the pixels appear in. $d4 and
# $bNB are left out for the reason above.
_ABLTS2 = ["pointer", "pointer", "pointer", "pointer", "int", "int", "pointer"]
_BLTS2 = [("$bA", "$bW", "$d0", "$d6", 0, 0, 0),
          ("$bA", "$bW", "$d0", "$d6", 4, 2, "$pal2"),
          ("$bA", "$bW", "$d1", "$d6", -3, -2, 0),
          ("$bA", "$bW", "$d2", "$d6", 0, 0, 0),
          ("$bA", "$bW", "$d3", "$d6", 0, 0, 0),
          ("$bA", "$bW", "$d0", "$d6", 24, 10, 0),
          ("$bA", "$bW", "$d0", "$d0", 0, 0, 0),
          ("$bB", "$bW", "$d0", "$d5", 1, 1, 0),
          ("$bNP", "$bW", "$d0", "$d6", 0, 0, 0),
          ("$bA", "$bB", "$d0", "$d6", 0, 0, 0),
          ("$bD", "$bW", "$d0", "$d6", 5, 5, 0),
          ("$bA", "$bW", "$d0", "$d6", -20, 0, 0),
          ("$bA", "$bW", "$d0", "$d6", 0, 15, 0),
          ("$bA", "$bW", "$e0", "$d6", 0, 0, 0),
          ("$bA", "$bW", "$x0", "$d6", 0, 0, 0),
          ("$b16", "$bW", "$d0", "$d6", 0, 0, 0),
          ("$bA", "$b16", "$d0", "$d6", 0, 0, 0),
          ("$bA", "$bX", "$d0", "$d6", 0, 0, 0),
          ("$bA", "$bW", "$d0", "$d6", 100, 100, 0),
          ("$bA", "$bW", "$d5", "$d6", 0, 0, "$pal")]
_SHORTS2 = [("$bA", "$bW", "$d0", "$d6", 0, 0, 0), ("$bA", "$bW", "$d0", "$d6", 4, 2, "$pal2"),
            ("$bA", "$bW", "$d1", "$d6", -3, -2, 0), ("$bB", "$bW", "$d0", "$d5", 1, 1, 0),
            ("$bA", "$bW", "$e0", "$d6", 0, 0, 0), ("$bA", "$bW", "$d0", "$d6", 100, 100, 0)]

HOOKS["Sprite::bltS2"] = dict(
    module="jgld.dll", addr=0x00019B90, abi="thiscall", ret="int", args=_ABLTS2,
    fixture="c3ci_s111", state=_BLIT, vectors=_BLTS2)
HOOKS["Sprite::bltS2_s223"] = dict(
    module="jgld.dll", addr=0x00019B90, abi="thiscall", ret="int", args=_ABLTS2,
    fixture="c3ci_s223", state=_BLIT, vectors=_SHORTS2)

# The two line slots, Sprite::lineTo 0x00018490 (slot 35) and Sprite::dashTo 0x00018510 (slot 34),
# were prepared and dropped: their callees blitMask 0x2cec0 and blitMaskScaled 0x2d390 hang the game.
# log/c3/c3ci_notes.txt has what was established about them, which is most of what a later batch needs.

# ======================================================================== the five draw dispatchers

# draw (dst, x, y, pal): the only dispatcher whose guards come BEFORE the Palette store, so its two early
# returns leave [sprite + 0x10] alone. The 8-bit destinations ($e0, $e1, $e2, $e3) take 0x5dde0 and the
# 16-bit ones 0x1cde0; $bBsy takes the two busy reports, whose result is the /GZ frame fill 0xcccccccc.
_ADRAW = ["pointer", "pointer", "int", "int", "pointer"]
HOOKS["Sprite::draw"] = dict(
    module="jgld.dll", addr=0x00015180, abi="thiscall", ret="int", args=_ADRAW,
    fixture="c3ci_s111", state=_BLIT,
    vectors=[("$bA", "$d0", 0, 0, 0), ("$bA", "$d0", 4, 2, "$pal2"), ("$bA", "$d1", -3, -2, 0),
             ("$bB", "$d0", 1, 1, 0), ("$bNP", "$d0", 0, 0, 0), ("$bD", "$d0", 5, 5, 0),
             ("$bA", "$d2", 0, 0, 0), ("$bA", "$d3", 0, 0, 0), ("$bA", "$d4", 0, 0, 0),
             ("$bA", "$e0", 0, 0, 0), ("$bA", "$e1", 2, 2, 0), ("$bA", "$e2", 0, 0, 0),
             ("$bA", "$e3", 0, 0, 0), ("$bBsy", "$d0", 0, 0, 0), ("$bBsy", "$e0", 0, 0, 0),
             ("$bA", "$x0", 0, 0, 0), ("$bA", "$x1", 0, 0, 0), ("$b16", "$d0", 0, 0, 0),
             ("$b16", "$e0", 0, 0, 0), ("$bX", "$d0", 0, 0, 0), ("$bNB", "$d0", 0, 0, 0),
             ("$bA", 0, 0, 0, 0), ("$bA", "$d0", 100, 100, 0), ("$bA", "$d5", 0, 0, "$pal")])
HOOKS["Sprite::draw_s223"] = dict(
    module="jgld.dll", addr=0x00015180, abi="thiscall", ret="int", args=_ADRAW,
    fixture="c3ci_s223", state=_BLIT,
    vectors=[("$bA", "$d0", 0, 0, 0), ("$bA", "$d0", 4, 2, "$pal2"), ("$bA", "$d1", -3, -2, 0),
             ("$bB", "$d0", 1, 1, 0), ("$bA", "$e0", 0, 0, 0), ("$bA", "$d0", 100, 100, 0)])

# drawF (dst, x, y, table, pal): one blitter, on an 8-bit destination. The Palette store comes FIRST, so the
# NULL-destination (0x10) and invalid-sprite (7) returns leave [sprite + 0x10] changed. Four of its six arms
# never store a result, so they return the /GZ frame fill.
HOOKS["Sprite::drawF"] = dict(
    module="jgld.dll", addr=0x00016270, abi="thiscall", ret="int", args=_A_DST4,
    fixture="c3ci_s111", state=_BLIT,
    vectors=[("$bA", "$e0", 0, 0, "$b", 0), ("$bA", "$e0", 4, 2, "$b2", "$pal2"),
             ("$bA", "$e1", -3, -2, "$b", 0), ("$bB", "$e0", 1, 1, "$b2", 0),
             ("$bD", "$e0", 5, 5, "$b", 0), ("$bA", "$e2", 0, 0, "$b", 0),
             ("$bA", "$e3", 0, 0, "$b", 0), ("$bA", "$e0", 100, 100, "$b", 0),
             ("$bBsy", "$e0", 0, 0, "$b", 0), ("$bA", "$d0", 0, 0, "$b", 0),
             ("$bA", "$x0", 0, 0, "$b", 0), ("$b16", "$e0", 0, 0, "$b", 0),
             ("$b16", "$d0", 0, 0, "$b", 0), ("$b16", "$x0", 0, 0, "$b", 0),
             ("$bX", "$e0", 0, 0, "$b", 0), ("$bNB", "$e0", 0, 0, "$b", 0),
             ("$bA", 0, 0, 0, "$b", 0), ("$bA", 0, 0, 0, "$b", "$pal"),
             ("$bA", "$e1", 0, 0, "$b2", 0), ("$bNP", "$e0", 0, 0, "$b", 0)])
HOOKS["Sprite::drawF_s223"] = dict(
    module="jgld.dll", addr=0x00016270, abi="thiscall", ret="int", args=_A_DST4,
    fixture="c3ci_s223", state=_BLIT,
    vectors=[("$bA", "$e0", 0, 0, "$b", 0), ("$bA", "$e0", 4, 2, "$b2", "$pal2"),
             ("$bA", "$e1", -3, -2, "$b", 0), ("$bB", "$e0", 1, 1, "$b", 0),
             ("$bA", "$d0", 0, 0, "$b", 0), ("$bA", "$e0", 100, 100, "$b", 0)])

# drawH (dst, x, y, table, key): two blitters, 0x61db0 on an 8-bit destination and 0x26240 on a 16-bit one.
# The table is the same argument for both: $m / $m2 are the 256-byte index remap 0x26240 reads and $b / $b2
# the 64 KB byte table 0x61db0 indexes with one byte.
HOOKS["Sprite::drawH"] = dict(
    module="jgld.dll", addr=0x000169A0, abi="thiscall", ret="int", args=_A_DST4,
    fixture="c3ci_s111", state=_BLIT,
    vectors=[("$bA", "$d0", 0, 0, "$m", 0), ("$bA", "$d0", 4, 2, "$m2", "$pal2"),
             ("$bA", "$d1", -3, -2, "$m", 0), ("$bA", "$d2", 0, 0, "$m", 0),
             ("$bA", "$d3", 0, 0, "$m", 0), ("$bA", "$d4", 0, 0, "$m", 0),
             ("$bB", "$d0", 1, 1, "$m2", 0), ("$bNP", "$d0", 0, 0, "$m", 0),
             ("$bA", "$e0", 0, 0, "$b", 0), ("$bA", "$e1", 2, 2, "$b2", 0),
             ("$bA", "$e2", 0, 0, "$b", 0), ("$bA", "$e3", 0, 0, "$b", 0),
             ("$bBsy", "$d0", 0, 0, "$m", 0), ("$bBsy", "$e0", 0, 0, "$b", 0),
             ("$bA", "$x0", 0, 0, "$m", 0), ("$b16", "$d0", 0, 0, "$m", 0),
             ("$bX", "$d0", 0, 0, "$m", 0), ("$bNB", "$d0", 0, 0, "$m", 0),
             ("$bA", 0, 0, 0, "$m", 0), ("$bD", "$d0", 5, 5, "$m", 0),
             ("$bA", "$d0", 100, 100, "$m", 0), ("$bA", "$d5", 0, 0, "$m2", "$pal")])
HOOKS["Sprite::drawH_s223"] = dict(
    module="jgld.dll", addr=0x000169A0, abi="thiscall", ret="int", args=_A_DST4,
    fixture="c3ci_s223", state=_BLIT,
    vectors=[("$bA", "$d0", 0, 0, "$m", 0), ("$bA", "$d0", 4, 2, "$m2", "$pal2"),
             ("$bA", "$d1", -3, -2, "$m", 0), ("$bB", "$d0", 1, 1, "$m", 0),
             ("$bA", "$e0", 0, 0, "$b", 0), ("$bA", "$d0", 100, 100, "$m", 0)])

# drawJ (dst, x, y, table, pixels, key): 0x62dd0 on an 8-bit destination, with both of its tables. Its
# result local is cleared to 0 first, so the report-only arms return 0 and not the frame fill.
_ADRAWJ = ["pointer", "pointer", "int", "int", "pointer", "pointer", "pointer"]
HOOKS["Sprite::drawJ"] = dict(
    module="jgld.dll", addr=0x00016FC0, abi="thiscall", ret="int", args=_ADRAWJ,
    fixture="c3ci_s111", state=_BLIT,
    vectors=[("$bA", "$e0", 0, 0, "$b", "$b2", 0), ("$bA", "$e0", 4, 2, "$b2", "$b", "$pal2"),
             ("$bA", "$e1", -3, -2, "$b", "$m", 0), ("$bB", "$e0", 1, 1, "$m", "$b", 0),
             ("$bD", "$e0", 5, 5, "$b", "$b2", 0), ("$bA", "$e2", 0, 0, "$b", "$b2", 0),
             ("$bA", "$e3", 0, 0, "$b", "$b2", 0), ("$bA", "$e0", 100, 100, "$b", "$b2", 0),
             ("$bBsy", "$e0", 0, 0, "$b", "$b2", 0), ("$bA", "$d0", 0, 0, "$b", "$b2", 0),
             ("$bA", "$x0", 0, 0, "$b", "$b2", 0), ("$b16", "$e0", 0, 0, "$b", "$b2", 0),
             ("$b16", "$d0", 0, 0, "$b", "$b2", 0), ("$bX", "$e0", 0, 0, "$b", "$b2", 0),
             ("$bNB", "$e0", 0, 0, "$b", "$b2", 0), ("$bA", 0, 0, 0, "$b", "$b2", 0),
             ("$bA", 0, 0, 0, "$b", "$b2", "$pal"), ("$bNP", "$e0", 0, 0, "$b2", "$m", 0),
             ("$bA", "$e1", 0, 0, "$m", "$b", 0), ("$bA", "$e0", 31, 0, "$b", "$b2", 0)])
HOOKS["Sprite::drawJ_s223"] = dict(
    module="jgld.dll", addr=0x00016FC0, abi="thiscall", ret="int", args=_ADRAWJ,
    fixture="c3ci_s223", state=_BLIT,
    vectors=[("$bA", "$e0", 0, 0, "$b", "$b2", 0), ("$bA", "$e0", 4, 2, "$b2", "$b", "$pal2"),
             ("$bA", "$e1", -3, -2, "$b", "$b2", 0), ("$bB", "$e0", 1, 1, "$b", "$b2", 0),
             ("$bA", "$d0", 0, 0, "$b", "$b2", 0), ("$bA", "$e0", 100, 100, "$b", "$b2", 0)])

# drawL (dst, x, y, table, key): 0x63f60 on an 8-bit destination. Its inner test is against 8 ONLY, so a
# 16-bit destination takes the same arm as a depth-24 one.
HOOKS["Sprite::drawL"] = dict(
    module="jgld.dll", addr=0x000178A0, abi="thiscall", ret="int", args=_A_DST4,
    fixture="c3ci_s111", state=_BLIT,
    vectors=[("$bA", "$e0", 0, 0, "$b", 0), ("$bA", "$e0", 4, 2, "$b2", "$pal2"),
             ("$bA", "$e1", -3, -2, "$b", 0), ("$bB", "$e0", 1, 1, "$b2", 0),
             ("$bD", "$e0", 5, 5, "$b", 0), ("$bA", "$e2", 0, 0, "$b", 0),
             ("$bA", "$e3", 0, 0, "$b", 0), ("$bA", "$e0", 100, 100, "$b", 0),
             ("$bBsy", "$e0", 0, 0, "$b", 0), ("$bA", "$d0", 0, 0, "$b", 0),
             ("$bA", "$x0", 0, 0, "$b", 0), ("$b16", "$e0", 0, 0, "$b", 0),
             ("$b16", "$d0", 0, 0, "$b", 0), ("$bX", "$e0", 0, 0, "$b", 0),
             ("$bNB", "$e0", 0, 0, "$b", 0), ("$bA", 0, 0, 0, "$b", 0),
             ("$bA", 0, 0, 0, "$b", "$pal"), ("$bNP", "$e0", 0, 0, "$b2", 0),
             ("$bA", "$e0", 0, 15, "$b", 0), ("$bA", "$e1", 0, 0, "$b2", 0)])
HOOKS["Sprite::drawL_s223"] = dict(
    module="jgld.dll", addr=0x000178A0, abi="thiscall", ret="int", args=_A_DST4,
    fixture="c3ci_s223", state=_BLIT,
    vectors=[("$bA", "$e0", 0, 0, "$b", 0), ("$bA", "$e0", 4, 2, "$b2", "$pal2"),
             ("$bA", "$e1", -3, -2, "$b", 0), ("$bB", "$e0", 1, 1, "$b", 0),
             ("$bA", "$d0", 0, 0, "$b", 0), ("$bA", "$e0", 100, 100, "$b", 0)])
