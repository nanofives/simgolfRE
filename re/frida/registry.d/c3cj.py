# Batch c3cj: the jgld.dll Display / DisplayBase vtable methods (module-aware hooks; addr is the RVA,
# VA = 0x10000000 + addr). Reimplementations and the instruction citations: shim/src/re/c3cj.cpp; branch
# tables and the proposed Purposes: log/c3/c3cj_purpose.md; fixtures: re/frida/js/fixtures.d/c3cj.js;
# what was refused and why: log/c3/c3cj_notes.txt.
#
# Nothing live is handed to these bodies: the module Display pointer 0x10128420 is never read, and every
# Display, Mode, Array<ModeInfo>, mode table and destroyable object is the fixture's own. The Display
# objects carry a SYNTHETIC vtable whose four mode slots are recording stubs, because the bodies the real
# table 0x1011d640 holds there (+0x58 pickMode, +0x60 setMode, +0x64 setModeIndex 0x100662a0,
# +0x68 enumModes 0x10065a90 - read out of the binary by log/c3/alt/c3cj_vt.py) call EnumDisplaySettingsA,
# ChangeDisplaySettingsA and MessageBoxA and refill the module arrays.
#
# The two scale keys swap the module globals 0x10122dc0 / dc4 / dc8 with the game's first thread suspended
# and put the game's values back before resuming it (about forty live sprite blitters read those three).
#
# Hooked callees to switch off when re-running (SIMGOLF_HOOKS_OFF):
#   Display::findMode / pickMode / pickModeFreq   jgld.dll:66a80   Array_Rec14::count (c3bb, at C3). It is
#     the ONE hooked callee any key of this batch reaches: the mode-slot dispatches all land on the
#     fixture's stubs, and the four Factory destroy slots call slot 0 of the fixture's own object.
#   Nothing else: the eight accessors and the two scale slots have no callee at all.
#
# Leaf rule: Display::getField138, getField12c130, getHwnd, getInstance, getWidth, getHeight, getBpp,
# setScale and getScale have no callee (not even __chkesp: their epilogues are `pop/pop/pop; mov esp,ebp;
# pop ebp; ret`), so each carries at least ten vectors - sixteen for the seven getters, twelve plus twelve
# for getScale, thirteen for setScale.

# the whole fixture block: the sixteen Display objects, the Mode objects, the lists, the record area, the
# out-parameter slots and the destroyable objects, so a store one field too wide changes the hash
_BLK = [("$blk", 0, 0x8000)]
# the three scale words plus the fixture's out-parameter block
_SC = [("$g", 0, 12), ("$blk", 0, 0x1000)]

_ALLD = [("$d%d" % i,) for i in range(16)]

# The pickMode / pickModeFreq vector lists are used twice: once against the fixture whose slot +0x64 stub
# returns the object's canned dword (so the return value proves the call happened and that nothing between
# the call and the epilogue touches eax), and once against c3cj_displays_idx, whose stub returns its own
# ARGUMENT, so the CSV's return column IS the index the scan chose.
_PICK = [("$d0", 640, 480, 8),            # empty: `jl 0x10065fb1` taken, returns 1
         ("$d1", 640, 480, 8),            # 60 <= 0x4b: no early exit, slot +0x64 with 0
         ("$d8", 640, 480, 8),            # 76 > 0x4b but no candidate yet: `jle 0x10065f73` taken
         ("$d9", 640, 480, 8),            # 76 then 80: the second ends the scan with index 0
         ("$d2", 800, 600, 16),           # 75 is exactly 0x4b: `jle 0x10065f6d` taken
         ("$d3", 640, 480, 8),            # 100 then 120: early exit with index 0
         ("$d4", 1024, 768, 32),          # equal frequencies: the second takes the `jle` at 0x10065f5a
         ("$d5", 640, 480, 8),            # negative frequencies
         ("$d6", 640, 480, 8),            # one mismatch per field
         ("$d7", 800, 600, 16),           # four matches under 0x4b
         ("$d11", -1, -1, -1),            # no candidate at all: returns 1
         ("$d12", 0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF),   # INT_MAX frequency, no candidate yet
         ("$d13", 640, 480, 8),           # INT_MAX then INT_MIN
         ("$d14", 640, 480, 8),           # eight rows: two updates then an early exit
         ("$d15", 1280, 1024, 32)]        # equal pair, then 100 ends the scan with index 0

_PICKF = [("$d1", 640, 480, 8, 1, 60),         # the +0x60 arm, low byte 1
          ("$d2", 800, 600, 16, 0x100, 60),    # low byte zero: the scan arm, equality at index 0
          ("$d2", 800, 600, 16, 0xFF, 70),     # the +0x60 arm, low byte 0xff
          ("$d3", 640, 480, 8, 0x12345678, 0),  # the +0x60 arm, low byte 0x78
          ("$d0", 640, 480, 8, 0, 60),         # empty list: returns 1
          ("$d7", 800, 600, 16, 0, 72),        # equality at index 2 ends the scan
          ("$d7", 800, 600, 16, 0, 99),        # no equality: the maximum, index 2
          ("$d4", 1024, 768, 32, 0, 85),       # equality at index 0
          ("$d4", 1024, 768, 32, 0, 99),       # equal pair: the second takes `jle 0x10066148`
          ("$d5", 640, 480, 8, 0, -5),         # equality on a negative frequency
          ("$d6", 640, 480, 8, 0, 65),         # one mismatch per field, then equality
          ("$d11", -1, -1, -1, 0, 0),          # no equality and no update: returns 1
          ("$d13", 640, 480, 8, 0, -0x80000000),   # update at index 0, equality at index 1
          ("$d14", 640, 480, 8, 0, 95),        # eight rows, equality at index 6
          ("$d15", 1280, 1024, 32, -0x80000000, 60),   # low byte zero: scan, equality at index 0
          ("$d12", 0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF, 0, 7)]   # no equality, one update

HOOKS = {
    # ------------------------------------------------------------------ the four mode methods
    #
    # Each dispatches slot +0x68 for the mode list and slot +0x64 (or +0x58 / +0x60) with its result. The
    # fixture's stubs append {tag, this, arguments} to the record area and return the object's canned
    # dword, so the compared state proves WHICH slot ran, with which arguments, in which order, and how
    # many times - which is the whole behaviour of these four, since the scan's result leaves them only
    # through that call.
    #
    # The sixteen lists ($a0..$a15, one per Display) are in re/frida/js/fixtures.d/c3cj.js; the comment on
    # each vector below names the branch it is there for.

    # findMode 0x00065c30: __thiscall(w, h, bpp) -> slot +0x64 with the index of the highest-frequency
    # exact match, or -1. The return is always 0 (`xor eax,eax` at 0x10065d3b), so every vector's evidence
    # is the record.
    "Display::findMode": dict(
        module="jgld.dll", addr=0x00065C30, abi="thiscall", ret="int",
        args=["pointer", "int", "int", "int"], fixture="c3cj_displays", state=_BLK,
        vectors=[("$d0", 640, 480, 8),            # empty list: `jge 0x10065cb0` taken at once, index -1
                 ("$d1", 640, 480, 8),            # one exact match, index 0
                 ("$d1", 800, 600, 16),           # width differs: `jne 0x10065cc4` taken
                 ("$d2", 800, 600, 16),           # 60 -> 75 -> 70: the last takes `jle 0x10065d03`
                 ("$d3", 640, 480, 8),            # two matches, rising frequency
                 ("$d4", 1024, 768, 32),          # equal frequencies: the second takes the `jle`
                 ("$d5", 640, 480, 8),            # -5 <= -1 (jle), then 0 > -1 updates: signed compare
                 ("$d6", 640, 480, 8),            # one mismatch per field, then a match at index 4
                 ("$d7", 800, 600, 16),           # four matches, best at index 2
                 ("$d10", 0, 0, 0),               # all-zero row matches all-zero arguments
                 ("$d11", -1, -1, -1),            # frequency -1 is not > the initial -1: no candidate
                 ("$d12", 0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF),   # extremes match
                 ("$d13", 640, 480, 8),           # INT_MAX then INT_MIN frequency
                 ("$d14", 640, 480, 8),           # eight rows, four of them mismatching
                 ("$d15", 1280, 1024, 32)]),      # equal pair then a higher one

    # setMode 0x00065da0: __thiscall(w, h, bpp, apply). Low byte of `apply` zero -> slot +0x58 with
    # (w, h, [this+0x134]); non-zero -> write the three fields and call slot +0x00 of [this+0x148] with
    # them and a literal 1. No return value, so the evidence is the record plus the three fields.
    "Display::setMode": dict(
        module="jgld.dll", addr=0x00065DA0, abi="thiscall", ret="void",
        args=["pointer", "int", "int", "int", "int"], fixture="c3cj_displays", state=_BLK,
        vectors=[("$d0", 640, 480, 8, 0),             # zero: the +0x58 arm, bpp argument dropped
                 ("$d1", 800, 600, 16, 0x100),        # low byte zero although the dword is not
                 ("$d2", 1024, 768, 32, 1),           # the write arm
                 ("$d3", 320, 200, 4, 0xFF),          # write arm, every low-byte bit set
                 ("$d4", 0, 0, 0, 0x12345678),        # write arm (low byte 0x78), zeroes written
                 ("$d5", -1, -1, -1, 0),              # +0x58 arm with negative arguments
                 ("$d6", 0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF, 2),      # write arm, extremes
                 ("$d7", -0x80000000, 1, 24, 0),      # +0x58 arm, INT_MIN width
                 ("$d8", 1280, 1024, 32, 1),          # write arm
                 ("$d9", 1, 2, 3, -0x80000000),       # low byte zero: the +0x58 arm
                 ("$d10", 4, 5, 6, -0x33333334),      # 0xcccccccc: low byte 0xcc, the write arm
                 ("$d11", 7, 8, 9, 0),                # +0x58 arm on a Display whose depth is -4
                 ("$d12", 10, 11, 12, 0x101),         # write arm, low byte 1
                 ("$d13", 13, 14, 15, 0),             # +0x58 arm
                 ("$d15", 2048, 1536, 32, 0x80)]),    # write arm, low byte 0x80

    # pickMode 0x00065ea0: __thiscall(w, h, bpp) -> the slot +0x64 call's eax, or 1 when nothing matched.
    # Two extra branches over findMode: frequency > 0x4b (0x10065f68) and a candidate already held
    # (0x10065f6f), which together end the scan early with the PREVIOUS index.
    "Display::pickMode": dict(
        module="jgld.dll", addr=0x00065EA0, abi="thiscall", ret="int",
        args=["pointer", "int", "int", "int"], fixture="c3cj_displays", state=_BLK, vectors=_PICK),

    # The same fifteen vectors with the slot +0x64 stub returning its argument: the return column is then
    # the index the scan chose (-1 when nothing matched, and 1 on the two vectors that never call the slot).
    "Display::pickMode_idx": dict(
        module="jgld.dll", addr=0x00065EA0, abi="thiscall", ret="int",
        args=["pointer", "int", "int", "int"], fixture="c3cj_displays_idx", state=_BLK, vectors=_PICK),

    # pickModeFreq 0x00066040: __thiscall(w, h, bpp, apply, freq). Low byte of `apply` non-zero -> one
    # dispatch of slot +0x60 with (w, h, bpp, (this & 0xffffff00) | applyByte) and that call's eax;
    # otherwise the scan, in which an entry whose frequency EQUALS `freq` wins at once.
    "Display::pickModeFreq": dict(
        module="jgld.dll", addr=0x00066040, abi="thiscall", ret="int",
        args=["pointer", "int", "int", "int", "int", "int"], fixture="c3cj_displays", state=_BLK,
        vectors=_PICKF),

    # The same sixteen vectors with the slot +0x64 stub returning its argument. On the four `apply` vectors
    # the return still comes from slot +0x60, whose stub returns the object's canned dword, so those four
    # are unchanged - which is itself the evidence that the two arms use two different slots.
    "Display::pickModeFreq_idx": dict(
        module="jgld.dll", addr=0x00066040, abi="thiscall", ret="int",
        args=["pointer", "int", "int", "int", "int", "int"], fixture="c3cj_displays_idx", state=_BLK,
        vectors=_PICKF),

    # ------------------------------------------------------------------- the field accessors
    #
    # Seven one-instruction getters and one two-store writer. Sixteen Display objects whose every field
    # differs, so a body reading the neighbouring field would change the result on every vector.

    # getField138 0x00065bf0 (slot 15): the dword at this+0x138.
    "Display::getField138": dict(
        module="jgld.dll", addr=0x00065BF0, abi="thiscall", ret="uint32", args=["pointer"],
        fixture="c3cj_displays", state=_BLK, vectors=_ALLD),

    # getHwnd 0x00067560 (slot 8): the dword at this+0x144.
    "Display::getHwnd": dict(
        module="jgld.dll", addr=0x00067560, abi="thiscall", ret="uint32", args=["pointer"],
        fixture="c3cj_displays", state=_BLK, vectors=_ALLD),

    # getInstance 0x000675a0 (slot 9): the dword at this+0x140.
    "Display::getInstance": dict(
        module="jgld.dll", addr=0x000675A0, abi="thiscall", ret="uint32", args=["pointer"],
        fixture="c3cj_displays", state=_BLK, vectors=_ALLD),

    # getWidth 0x00067760 (slot 42): the int at this+0x12c.
    "Display::getWidth": dict(
        module="jgld.dll", addr=0x00067760, abi="thiscall", ret="int", args=["pointer"],
        fixture="c3cj_displays", state=_BLK, vectors=_ALLD),

    # getHeight 0x000677a0 (slot 43): the int at this+0x130.
    "Display::getHeight": dict(
        module="jgld.dll", addr=0x000677A0, abi="thiscall", ret="int", args=["pointer"],
        fixture="c3cj_displays", state=_BLK, vectors=_ALLD),

    # getBpp 0x000677e0 (slot 44): the int at this+0x134.
    "Display::getBpp": dict(
        module="jgld.dll", addr=0x000677E0, abi="thiscall", ret="int", args=["pointer"],
        fixture="c3cj_displays", state=_BLK, vectors=_ALLD),

    # getField12c130 0x000666b0 (slot 13): writes this+0x12c through the FIRST pointer and this+0x130
    # through the second. No return value, so the evidence is the twelve destination pairs; each vector
    # writes a different pair of the fixture's out slots, which is why the state differs per vector.
    "Display::getField12c130": dict(
        module="jgld.dll", addr=0x000666B0, abi="thiscall", ret="void",
        args=["pointer", "pointer", "pointer"], fixture="c3cj_displays", state=_BLK,
        vectors=[("$d%d" % i, "$p%d" % (2 * i), "$p%d" % (2 * i + 1)) for i in range(12)]),

    # ------------------------------------------------------- the two scale slots (module globals)
    #
    # setScale writes 0x10122dc0 / dc4 / dc8 in argument order and getScale reads them back through three
    # pointers. Both keys run with the game's first thread suspended and the game's values restored.

    # setScale 0x00067d50 (slot 39): thirteen different triples, so every vector changes the three words
    # to a different value. `this` is not read (spilled at 0x10067d6a): the block pointer stands in.
    "Display::setScale": dict(
        module="jgld.dll", addr=0x00067D50, abi="thiscall", ret="void",
        args=["pointer", "int", "int", "int"], fixture="c3cj_scale_a", state=_SC,
        vectors=[("$blk", 0, 0, 0),
                 ("$blk", 1, 2, 3),
                 ("$blk", -1, 0, 1),
                 ("$blk", 0x10000, 0x10000, 0x10000),
                 ("$blk", 0x7FFFFFFF, -0x80000000, 0),
                 ("$blk", -0x80000000, 0x7FFFFFFF, -1),
                 ("$blk", 0x20000, -0x20000, 0x10000),
                 ("$blk", -0x10000, -0x10000, -0x10000),
                 ("$blk", 100, 200, 300),
                 ("$blk", -0x33333334, 0x33333333, 0x55555555),
                 ("$blk", 7, -7, 7),
                 ("$blk", 0x8000, 0x8000, 0x4000),
                 ("$blk", 13, 17, 19)]),

    # getScale 0x00067da0 (slot 40), first triple (0x11111111, 0x22222222, 0x33333333): twelve vectors,
    # each with its own trio of out slots, so the state differs per vector and the mapping of global to
    # parameter is pinned by three different values.
    "Display::getScale": dict(
        module="jgld.dll", addr=0x00067DA0, abi="thiscall", ret="void",
        args=["pointer", "pointer", "pointer", "pointer"], fixture="c3cj_scale_a", state=_SC,
        vectors=[("$blk", "$q%d" % (3 * i), "$q%d" % (3 * i + 1), "$q%d" % (3 * i + 2)) for i in range(12)]),

    # getScale, second triple (-1, 0, INT_MAX): the same twelve destinations, so the result cannot be a
    # constant of the fixture's and the zero and negative cases are covered.
    "Display::getScale_b": dict(
        module="jgld.dll", addr=0x00067DA0, abi="thiscall", ret="void",
        args=["pointer", "pointer", "pointer", "pointer"], fixture="c3cj_scale_b", state=_SC,
        vectors=[("$blk", "$q%d" % (3 * i), "$q%d" % (3 * i + 1), "$q%d" % (3 * i + 2)) for i in range(12)]),

    # ------------------------------------------------------------ the four Factory destroy slots
    #
    # `delete p`: when the argument is not NULL, slot 0 of ITS OWN vtable is called with a literal 1. The
    # fixture's objects carry a recording stub in that slot, so nothing is freed and the record shows the
    # object and the flag. No return value. Twelve objects plus the NULL argument, which is the other side
    # of the `je` each body starts with.
    "Factory::destroyFont": dict(
        module="jgld.dll", addr=0x00067B50, abi="thiscall", ret="void", args=["pointer", "pointer"],
        fixture="c3cj_displays", state=_BLK,
        vectors=[("$blk", "$z%d" % i) for i in range(12)] + [("$blk", 0)]),

    "Factory::destroyPalette": dict(
        module="jgld.dll", addr=0x00067BD0, abi="thiscall", ret="void", args=["pointer", "pointer"],
        fixture="c3cj_displays", state=_BLK,
        vectors=[("$blk", "$z%d" % i) for i in range(2, 14)] + [("$blk", 0)]),

    "Factory::destroySurface": dict(
        module="jgld.dll", addr=0x00067C50, abi="thiscall", ret="void", args=["pointer", "pointer"],
        fixture="c3cj_displays", state=_BLK,
        vectors=[("$blk", "$z%d" % i) for i in range(4, 16)] + [("$blk", 0)]),

    "Factory::destroySprite": dict(
        module="jgld.dll", addr=0x00067CD0, abi="thiscall", ret="void", args=["pointer", "pointer"],
        fixture="c3cj_displays", state=_BLK,
        vectors=[("$blk", "$z%d" % i) for i in (1, 3, 5, 7, 9, 11, 13, 15, 0, 2, 4, 6)] + [("$blk", 0)]),
}
