# C3 batch c3x (2026-10-07): functions the test scenarios never reach but with static callers. The flag-array helpers
# (flagIsSet / flagSet / flagToggle49ee90) over a synthetic virtually-inherited object; the three Widget quad setters;
# the polygon-edge DDA setup polyEdgeStep; and the nearest-palette-index search Palette::nearest through a one-slot
# fake palette vtable. Reimplemented in shim/src/re/c3x.cpp; fixtures in re/frida/js/fixtures.d/c3x.js; the side of
# every jcc each vector takes is in log/c3/c3x_purpose.md. flagToggle49ee90 calls flagIsSet / flagSet (same batch):
# re-run it with SIMGOLF_HOOKS_OFF=0049ef80,0049eef0 so the original arm uses the original callees.

# Flag list is at base+0xc0 (cur +0xcc, idx +0xd4), mask at base+0xf0; isSet/set walk and update them, so the state
# region spans 0xc0..0xf4.
_FLAG_STATE = [("$obj", 0xc0, 0x34)]
# ids present in the populated list are 10, 20, 30 (indices 0, 1, 2); 99 and the rest are not found (index -> count).
# The object ($obj) is passed as `this` (thiscall), so it is the first element of every vector.
_ISSET_POP = [("$obj", i) for i in (10, 20, 30, 99, 10, 20, 30, 0, -1, 1000)]
_SET_POP = [("$obj",) + v for v in
            ((10, 1), (20, 1), (30, 0), (99, 0), (10, 0), (20, 0), (30, 1), (0, 1), (-1, 0), (1000, 1), (10, 1), (20, 1))]
_TOGGLE_POP = [("$obj", i) for i in (10, 20, 30, 99, 10, 20, 30, 0, -1, 1000, 10, 20)]

# polyEdgeStep edges e0..e11, each with its own 0x24-byte record; dir (edge+0) is set by the fixture.
_POLY = [("$e0", 0), ("$e1", 2), ("$e2", 1), ("$e3", 1), ("$e4", 3), ("$e5", 4),
         ("$e6", 5), ("$e7", 0), ("$e8", 0), ("$e9", 2), ("$e10", 4), ("$e11", 1)]

# Palette queries (r, g, b, reserved); nearest index varies with the colour.
_PAL_R0 = [("$obj", 0, 0, 0, 0), ("$obj", 255, 255, 255, 0), ("$obj", 128, 64, 32, 0), ("$obj", 10, 200, 50, 0),
           ("$obj", 200, 10, 150, 0), ("$obj", 50, 50, 50, 0), ("$obj", 255, 0, 0, 0), ("$obj", 0, 255, 0, 0),
           ("$obj", 0, 0, 255, 0), ("$obj", 123, 45, 67, 0), ("$obj", 240, 240, 10, 0), ("$obj", 5, 5, 250, 0)]
_PAL_R1 = [("$obj", r, g, b, 1) for (_, r, g, b, _z) in _PAL_R0]
_PAL_ARGS = ["pointer", "uint32", "uint32", "uint32", "int"]

HOOKS = {
    "flagIsSet": dict(module="golf_clean.exe", addr=0x0049EF80, abi="thiscall", ret="int", args=["pointer", "int"],
                      fixture="c3x_flags_pop", state=_FLAG_STATE, vectors=_ISSET_POP),
    "flagIsSet_empty": dict(module="golf_clean.exe", addr=0x0049EF80, abi="thiscall", ret="int",
                            args=["pointer", "int"], fixture="c3x_flags_empty", state=_FLAG_STATE,
                            vectors=[("$obj", 5), ("$obj", 7)]),
    "flagIsSet_zcount": dict(module="golf_clean.exe", addr=0x0049EF80, abi="thiscall", ret="int",
                             args=["pointer", "int"], fixture="c3x_flags_zcount", state=_FLAG_STATE,
                             vectors=[("$obj", 5), ("$obj", 10)]),
    "flagSet": dict(module="golf_clean.exe", addr=0x0049EEF0, abi="thiscall", ret="void",
                    args=["pointer", "int", "int"], fixture="c3x_flags_pop", state=_FLAG_STATE, vectors=_SET_POP),
    "flagSet_empty": dict(module="golf_clean.exe", addr=0x0049EEF0, abi="thiscall", ret="void",
                          args=["pointer", "int", "int"], fixture="c3x_flags_empty", state=_FLAG_STATE,
                          vectors=[("$obj", 5, 1), ("$obj", 7, 0)]),
    "flagSet_zcount": dict(module="golf_clean.exe", addr=0x0049EEF0, abi="thiscall", ret="void",
                           args=["pointer", "int", "int"], fixture="c3x_flags_zcount", state=_FLAG_STATE,
                           vectors=[("$obj", 5, 1), ("$obj", 10, 0)]),
    "flagToggle49ee90": dict(module="golf_clean.exe", addr=0x0049EE90, abi="thiscall", ret="void",
                             args=["pointer", "int"], fixture="c3x_flags_pop", state=_FLAG_STATE, vectors=_TOGGLE_POP),
    "Widget_setQuad70": dict(module="golf_clean.exe", addr=0x00476340, abi="thiscall", ret="void",
                             args=["pointer", "uint32", "uint32", "uint32", "uint32"], fixture="c3x_widget",
                             state=[("$obj", 0x70, 0x34)],
                             vectors=[("$obj", i * 7 + 1, i * 13 + 2, i * 29 + 3, i * 101 + 4) for i in range(12)]),
    "Widget_setQuad74": dict(module="golf_clean.exe", addr=0x00476370, abi="thiscall", ret="void",
                             args=["pointer", "uint32", "uint32", "uint32", "uint32"], fixture="c3x_widget",
                             state=[("$obj", 0x74, 0x34)],
                             vectors=[("$obj", i * 11 + 5, i * 17 + 6, i * 31 + 7, i * 103 + 8) for i in range(12)]),
    "Widget_setQuad78": dict(module="golf_clean.exe", addr=0x004763A0, abi="thiscall", ret="void",
                             args=["pointer", "uint32", "uint32", "uint32", "uint32"], fixture="c3x_widget",
                             state=[("$obj", 0x78, 0x34)],
                             vectors=[("$obj", i * 19 + 9, i * 23 + 10, i * 37 + 11, i * 107 + 12) for i in range(12)]),
    "polyEdgeStep": dict(module="golf_clean.exe", addr=0x00492ED0, abi="default", ret="int", args=["pointer", "int"],
                         fixture="c3x_poly", state=[("$edges", 0, 12 * 0x24)], vectors=_POLY),
    "Palette::nearest": dict(module="golf_clean.exe", addr=0x00483420, abi="thiscall", ret="int", args=_PAL_ARGS,
                             fixture="c3x_pal_r0", vectors=_PAL_R0),
    "Palette::nearest_r1": dict(module="golf_clean.exe", addr=0x00483420, abi="thiscall", ret="int", args=_PAL_ARGS,
                                fixture="c3x_pal_r1", vectors=_PAL_R1),
    "Palette::nearest_null": dict(module="golf_clean.exe", addr=0x00483420, abi="thiscall", ret="int", args=_PAL_ARGS,
                                  fixture="c3x_pal_null", vectors=[("$obj", 10, 20, 30, 0), ("$obj", 1, 2, 3, 1)]),
}
