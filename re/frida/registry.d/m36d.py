# C3 batch m36d (round 36, 2026-10-15): flagIsSet 0x0049ef80 and flagSet 0x0049eef0, re-implemented in
# shim/src/re/m36d.cpp (round 6 had promoted them with no hook). Fixtures in re/frida/js/fixtures.d/m36d.js put the
# ListModel base at obj+0x40 (vbtable+8 = 0x40); the state region obj+0x100..0x138 covers the list's cursor, count and
# index, the mask and the guard word after it. Key names carry the batch tag so they never collide with c3x's
# flagIsSet / flagSet keys. The jcc sides each vector takes are in log/c3/m36d_purpose.md.

_STATE = [("$obj", 0x100, 0x38)]

# pop: ids 10, 20, 30, 20 (count 4). 20 is found at index 1 (the duplicate at 3 is never reached), 99 / 0 / -1 are not
# found (index 4).
_ISSET_POP = [("$obj", i) for i in (10, 20, 30, 99, 0, -1, 10, 30)]
# long: ids 100..139. Found at 33, 39, 32, 31, 0, 5; 999 not found (index 40 -> shl by 8).
_ISSET_LONG = [("$obj", i) for i in (133, 139, 132, 131, 100, 105, 999, 137)]
_ISSET_SHORT = [("$obj", i) for i in (1, 3, 4, 6, 2)]
_ISSET_EDGE = [("$obj", i) for i in (10, 20, 99)]

_SET_POP = [("$obj",) + v for v in ((10, 1), (20, 0), (30, 1), (99, 1), (0, 0), (10, 2), (20, -1), (30, 0), (99, 0))]
_SET_LONG = [("$obj",) + v for v in ((133, 1), (133, 0), (139, 1), (132, 0), (131, 1), (100, 0), (999, 1), (999, 0),
                                      (105, 7), (117, 1), (118, 0))]
# (117, 1) / (118, 0): indices 17 / 18, where bit k and bit k-16 of 0xa5a5a5a5 differ in the way the write shows
# (a body taking the index mod 16 read GREEN on the first nine vectors; NEGSEL 4).
_SET_SHORT = [("$obj",) + v for v in ((1, 0), (3, 1), (4, 1), (6, 0), (2, 1))]
_SET_EDGE = [("$obj",) + v for v in ((10, 1), (20, 0), (99, 1))]


def _isset(fixture, vectors):
    return dict(module="golf_clean.exe", addr=0x0049EF80, abi="thiscall", ret="int", args=["pointer", "int"],
                fixture=fixture, state=_STATE, vectors=vectors)


def _set(fixture, vectors):
    return dict(module="golf_clean.exe", addr=0x0049EEF0, abi="thiscall", ret="void", args=["pointer", "int", "int"],
                fixture=fixture, state=_STATE, vectors=vectors)


HOOKS = {
    "flagIsSet_m36d_pop": _isset("m36d_flags_pop", _ISSET_POP),
    "flagIsSet_m36d_long": _isset("m36d_flags_long", _ISSET_LONG),
    "flagIsSet_m36d_short": _isset("m36d_flags_short", _ISSET_SHORT),
    "flagIsSet_m36d_empty": _isset("m36d_flags_empty", _ISSET_EDGE),
    "flagIsSet_m36d_zcount": _isset("m36d_flags_zcount", _ISSET_EDGE),
    "flagIsSet_m36d_negcount": _isset("m36d_flags_negcount", _ISSET_EDGE),
    "flagSet_m36d_pop": _set("m36d_flags_pop", _SET_POP),
    "flagSet_m36d_long": _set("m36d_flags_long", _SET_LONG),
    "flagSet_m36d_short": _set("m36d_flags_short", _SET_SHORT),
    "flagSet_m36d_empty": _set("m36d_flags_empty", _SET_EDGE),
    "flagSet_m36d_zcount": _set("m36d_flags_zcount", _SET_EDGE),
    "flagSet_m36d_negcount": _set("m36d_flags_negcount", _SET_EDGE),
}
