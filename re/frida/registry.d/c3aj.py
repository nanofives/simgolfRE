# C3 batch c3aj (2026-10-08): matchUpdate 0x00427380, the end-of-hole narrator. __cdecl, one argument (the golfer
# index), void, so each registry key is a fixture variant (re/frida/js/fixtures.d/c3aj.js) and the vectors are the
# golfer indices it is called with; the compared evidence is the state the call writes. Reimplemented in
# shim/src/re/c3aj.cpp. Branch coverage per fixture: log/c3/c3aj_purpose.md.
#
# State regions cover everything matchUpdate and the callees it invokes through their original addresses write:
# the golfer records, the hole table, the per-type table and the weekly money tables, the course-record table and
# the string store its holders' names live in, the per-type stroke totals, the 0x4c-stride record sweep, the shared
# text buffer and the itoa scratch, the fee/cash/purse scalars, and the startMessage, pointsPopup, postEvent and
# logTick rings (including the RNG seed 0x00822d9c, which startMessage draws from).

# Golfer records 0..7, stride 0x100 (0x0042738b).
_GOLFERS = [(0x005794B8, 0, 0x800)]
# Hole table, stride 0x208: par bytes 0x00575ab0, waiting 0x00575c9c, takings 0x00575ca4, magazine flags
# 0x00575cb0, match/par bytes 0x00575cb8, the shot histogram 0x00575ad8.
_HOLES = [(0x00575AB0, 0, 0x2698)]
# Weekly income 0x00584210 and purses 0x0058421e (100 rows of 0x14) followed by the per-type table 0x005849e0
# (76 entries of 0x2c; +0 best, +1 average, +2 rank, +3+h the per-hole bits, +0x2a the par word).
_TYPES = [(0x00584210, 0, 0x14E2)]
# Course records 0x0056a524 (10 dwords) and the string store appendString / appendToBuffer45b8b0 move them in.
_RECORDS = [(0x0056A524, 0, 0x30), (0x0056FCB0, 0, 0x1002), (0x0059D81C, 0, 0x100), (0x005A46B8, 0, 0x100)]
# Per-type stroke totals 0x0056ae90 / 0x0056aedc and the 0x4c-stride records swept at 0x00427736.
_TABLES = [(0x0056AE90, 0, 0x1600), (0x00585862, 0, 0x1300)]
# Shared text buffer and the itoa scratch both narrators write through.
_TEXT = [(0x0051A068, 0, 512), (0x0058A528, 0, 32)]
# Fee, cash, purse and tutorial scalars (0x005a9cd8/0x005a9cdc are playSoundAt's).
_SCALARS = [(0x004C2850, 0, 4), (0x004C2E0C, 0, 4), (0x00571FD4, 0, 4), (0x005787CC, 0, 4), (0x0059B730, 0, 4),
            (0x005A59F8, 0, 4), (0x005A9CCC, 0, 4), (0x005A9CD8, 0, 8)]
# startMessage 0x0040cb00 writers (as c3ac's _MSG), with the message copy widened to the longest narration.
_MSG = [(0x0053DF54, 0, 4), (0x00569498, 0, 1), (0x005A34EC, 0, 4), (0x005A7144, 0, 4), (0x004C2E08, 0, 4),
        (0x005694A4, 0, 4), (0x0056D1A8, 0, 8), (0x00822D9C, 0, 4), (0x005A6D40, 0, 512)]
# pointsPopup 0x0040c890's eight-slot ring.
_POPUP = [(0x0059ABB0, 0, 4), (0x00542FD8, 0, 32), (0x00542FF8, 0, 32), (0x00542DD8, 0, 32), (0x00542F00, 0, 32)]
# postEvent 0x0046e7b0's records and logTick 0x0040c6f0's 500-word log.
_EVENT = [(0x004C15A0, 0, 0x300), (0x004E3DB8, 0, 8), (0x00839338, 0, 4), (0x008392A4, 0, 4), (0x00568600, 0, 1000)]

# patronEvent 0x004266b0's own globals, for the two keys whose tail reaches it (plus appendCents' 0x00569628).
_PATRON = [(0x0053A450, 0, 4), (0x00543CFC, 0, 4), (0x00567A1C, 0, 4), (0x00572CAC, 0, 4), (0x0059AAF8, 0, 4),
           (0x00822C70, 0, 4), (0x00569628, 0, 4), (0x0058BCB8, 0, 0x1000)]

_STATE = (_GOLFERS + _HOLES + _TYPES + _RECORDS + _TABLES + _TEXT + _SCALARS + _MSG + _POPUP + _EVENT
          + _PATRON)

_BASE = dict(module="golf_clean.exe", addr=0x00427380, abi="default", ret="void", args=["int"], state=_STATE)

# In the end-of-match fixtures the called golfer stands on the course's last hole; golfer 2 is the even partner
# of its pair, golfer 5 the odd one and golfer 6 a third even one, so one fixture drives both parities of the
# arms that test `golfer == even` / `golfer == odd` (0x00427de2, 0x00427f34).
_PAIR = [(2,), (5,), (6,)]

HOOKS = {
    "matchUpdate": dict(fixture="c3aj_fee", vectors=[(2,), (3,), (4,)], **_BASE),
    "matchUpdate_fee3": dict(fixture="c3aj_fee3", vectors=[(2,), (3,), (4,)], **_BASE),
    "matchUpdate_fee2": dict(fixture="c3aj_fee2", vectors=[(2,), (3,), (4,)], **_BASE),
    "matchUpdate_nofee": dict(fixture="c3aj_nofee", vectors=[(2,), (4,), (5,)], **_BASE),
    "matchUpdate_match_mid": dict(fixture="c3aj_match_mid", vectors=[(2,), (4,)], **_BASE),
    "matchUpdate_match_win": dict(fixture="c3aj_match_win", vectors=_PAIR, **_BASE),
    "matchUpdate_match_tie": dict(fixture="c3aj_match_tie", vectors=_PAIR, **_BASE),
    "matchUpdate_match_lose": dict(fixture="c3aj_match_lose", vectors=_PAIR, **_BASE),
    "matchUpdate_records": dict(fixture="c3aj_records", vectors=_PAIR, **_BASE),
    "matchUpdate_records_empty": dict(fixture="c3aj_records_empty", vectors=_PAIR, **_BASE),
    "matchUpdate_record_msg": dict(fixture="c3aj_record_msg", vectors=_PAIR, **_BASE),
    "matchUpdate_vip": dict(fixture="c3aj_vip", vectors=[(2,), (3,), (4,), (5,)], **_BASE),
    "matchUpdate_comeback": dict(fixture="c3aj_comeback", vectors=_PAIR, **_BASE),
    "matchUpdate_patron": dict(fixture="c3aj_patron", vectors=[(2,), (3,), (5,)], **_BASE),
    "matchUpdate_match_nopair": dict(fixture="c3aj_match_nopair", vectors=[(2,), (4,), (6,)], **_BASE),
    "matchUpdate_patron_wrap": dict(fixture="c3aj_patron_wrap", vectors=[(2,), (4,)], **_BASE),
}
