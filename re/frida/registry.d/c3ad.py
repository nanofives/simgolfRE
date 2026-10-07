# C3 batch c3ad (2026-10-08) of golf_clean.exe: one large (2 KB) pure-math render function. Reimplemented in
# shim/src/re/c3ad.cpp; fixtures in re/frida/js/fixtures.d/c3ad.js; the side of every data-dependent jcc each
# fixture takes is in log/c3/c3ad_purpose.md.
#
# build16BitColorTable 0x00461830 has callees (brightenComponent 0x00461810 and the virtual display cap), so it
# is not a leaf and does not need >= 10 vectors. It takes no arguments, so each registry entry is one call; the
# two entries drive the two sides of the per-pixel `cmp eax,1` format branch (RGB565 vs RGB555), giving two
# distinct LUT tables. brightenComponent (its callee) is C3 in batch c3h, not this batch; both arms call the
# same installed brightenComponent hook, so no SIMGOLF_HOOKS_OFF is needed.

# The whole observable effect is the 0x60000-byte LUT at DAT_00824148 (the fixture's $table buffer).
_TABLE = [("$table", 0, 0x60000)]

HOOKS = {
    "build16BitColorTable_565": dict(module="golf_clean.exe", addr=0x00461830, abi="default", ret="int",
                                     args=[], fixture="c3ad_build16_565", state=_TABLE, vectors=[[]]),
    "build16BitColorTable_555": dict(module="golf_clean.exe", addr=0x00461830, abi="default", ret="int",
                                     args=[], fixture="c3ad_build16_555", state=_TABLE, vectors=[[]]),
}
