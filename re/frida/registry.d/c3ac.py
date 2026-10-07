# C3 batch c3ac (2026-10-08): large gameplay functions (over 2 KB). holeMagazineEvent 0x0042dea0 is __cdecl void with
# no arguments, so it gets one registry entry per fixture variant (c3ac.js); each is a single call whose rich global /
# hole-table writes are the compared state. Reimplemented in shim/src/re/c3ac.cpp. State regions cover: the hole table
# [0x00575ab0, +0x2698); the course output globals; the shared text buffer 0x0051a068 and the two itoa scratch buffers;
# the startMessage ticker globals (incl. the RNG seed 0x00822d9c and the 0x005a6d40 message copy); postEvent's event
# records; and logTick's 0x00568600 word log. Branch coverage per fixture: log/c3/c3ac_purpose.md.

# Hole table: par/yards/ad0/ad4/c08/histogram/flags/aa8/be2/be4/coords/rating/s8a8 for holes 1..18, stride 0x208.
_HOLE_TABLE = [(0x00575AB0, 0, 0x2698)]
# Course outputs holeMagazineEvent writes.
_OUT = [(0x0056D1B0, 0, 4), (0x0059AE78, 0, 4), (0x0059AAFC, 0, 4), (0x0058D36C, 0, 4), (0x005A636C, 0, 4),
        (0x0056949C, 0, 4), (0x005A882C, 0, 4), (0x00541CD8, 0, 4), (0x005685F8, 0, 4), (0x0056C7B4, 0, 4),
        (0x004C2850, 0, 4)]
# Shared text buffer 0x0051a068, the magazine itoa scratch 0x0058a528 and announceHoleType's itoa scratch 0x00824134.
_TEXT = [(0x0051A068, 0, 384), (0x0058A528, 0, 32), (0x00824134, 0, 16)]
# startMessage writers (as c3g _MSG_STATE), with the message copy widened to the longest magazine message.
_MSG = [(0x0053DF54, 0, 4), (0x00569498, 0, 1), (0x005A34EC, 0, 4), (0x005A7144, 0, 4), (0x004C2E08, 0, 4),
        (0x005694A4, 0, 4), (0x0056D1A8, 0, 8), (0x00822D9C, 0, 4), (0x005A6D40, 0, 384)]
# postEvent records (announceHoleType kinds 0..7 and magazine ids 7/10 -> 0x004c15a0 + id*0x30).
_EVENT = [(0x004C15A0, 0, 0x300), (0x004E3DB8, 0, 8), (0x00839338, 0, 4), (0x008392A4, 0, 4)]
# logTick 500-word log.
_LOG = [(0x00568600, 0, 1000)]

_STATE = _HOLE_TABLE + _OUT + _TEXT + _MSG + _EVENT + _LOG

_HOLE = dict(module="golf_clean.exe", addr=0x0042DEA0, abi="default", ret="void", args=[], state=_STATE,
             vectors=[()])

HOOKS = {
    "holeMagazineEvent": dict(fixture="c3ac_mag1", **_HOLE),
    "holeMagazineEvent_mag2": dict(fixture="c3ac_mag2", **_HOLE),
    "holeMagazineEvent_busy": dict(fixture="c3ac_busy", **_HOLE),
    "holeMagazineEvent_diff2": dict(fixture="c3ac_diff2", **_HOLE),
    "holeMagazineEvent_force": dict(fixture="c3ac_force", **_HOLE),
}
