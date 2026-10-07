# C3 batch c3ab (revisit, 2026-10-07): functions offered in earlier rounds and left at C2 for time/scope.
# clearCost (table-driven cost of clearing a tile; callee tileBlocked 0x0040bf60 is C3, not in this batch -> both
# arms call it identically, no SIMGOLF_HOOKS_OFF); landmarkName (strcat a landmark name into the scratch buffer
# 0x0051a068); stripNewline / trimSpaces (in-place string edits, callees not hooked); measureTextWidth and the three
# Widget forwarders (Widget_applyPalette / Widget_fillRectR / Widget_fillArea) through a one-vtable fake surface.
# Reimplemented in shim/src/re/c3ab.cpp; fixtures in re/frida/js/fixtures.d/c3ab.js; per-jcc coverage in
# log/c3/c3ab_purpose.md.

# stripNewline / trimSpaces each modify the one buffer a vector passes; list every buffer as a state region.
_STR_STATE = [("$b%d" % i, 0, 64) for i in range(8)]
_STR_VEC = [("$b%d" % i,) for i in range(8)]

# landmarkName appends into 0x0051a068; it is a leaf (no callees) so it carries >= 10 vectors.
_LAND_VEC = [(0, 1), (0, 0), (1, 1), (1, 0), (2, 1), (2, 0), (5, 1), (5, 0), (9, 1), (9, 0),
             (10, 1), (10, 0), (12, 1), (12, 0), (15, 0), (16, 1), (18, 1), (18, 0), (19, 1), (100, 0)]

# clearCost cells: (1,1)->12 (1,2)->-16 (1,3)->-8 (1,4)->32 (1,5)->16/14 (1,6)->10 (1,7)->15 (5,5)->0 (60,0)->0 (-1,5)->0
_COST_VEC = [(1, 1), (1, 2), (1, 3), (1, 4), (1, 5), (1, 6), (1, 7), (5, 5), (60, 0), (-1, 5)]

HOOKS = {
    "clearCost": dict(module="golf_clean.exe", addr=0x0042EE80, abi="default", ret="int", args=["int", "int"],
                      fixture="c3ab_cost", vectors=_COST_VEC),
    "clearCost_ct0": dict(module="golf_clean.exe", addr=0x0042EE80, abi="default", ret="int", args=["int", "int"],
                          fixture="c3ab_cost_ct0", vectors=_COST_VEC),

    "landmarkName": dict(module="golf_clean.exe", addr=0x004074A0, abi="default", ret="void", args=["int", "int"],
                         fixture="c3ab_landmark", state=[(0x0051A068, 0, 0x80)], vectors=_LAND_VEC),

    "stripNewline": dict(module="golf_clean.exe", addr=0x004925D0, abi="default", ret="void", args=["pointer"],
                         fixture="c3ab_strnl", state=_STR_STATE, vectors=_STR_VEC),
    "trimSpaces": dict(module="golf_clean.exe", addr=0x004925B0, abi="default", ret="void", args=["pointer"],
                       fixture="c3ab_trim", state=_STR_STATE, vectors=_STR_VEC),

    "measureTextWidth": dict(module="golf_clean.exe", addr=0x00483930, abi="thiscall", ret="int",
                             args=["pointer", "pointer", "int"], fixture="c3ab_wsurf",
                             vectors=[("$obj", "$s3", 3), ("$obj", "$s3", 10), ("$obj", "$s3", -1), ("$obj", "$s0", 0),
                                      ("$obj", "$s1", 5), ("$obj", "$s2", 1), ("$obj", "$s4", 4), ("$obj", "$s4", 100),
                                      ("$obj", 0, 5), ("$obj", "$s4", -5)]),
    "measureTextWidth_nofont": dict(module="golf_clean.exe", addr=0x00483930, abi="thiscall", ret="int",
                                    args=["pointer", "pointer", "int"], fixture="c3ab_wsurf_nofont",
                                    vectors=[("$obj", "$s3", 3), ("$obj", "$s1", 2)]),

    "Widget_applyPalette": dict(module="golf_clean.exe", addr=0x004789F0, abi="thiscall", ret="int",
                                args=["pointer", "pointer"], fixture="c3ab_wsurf", state=[("$obj", 0xF0, 4)],
                                vectors=[("$obj", "$arg0"), ("$obj", "$arg1"), ("$obj", "$arg2"), ("$obj", "$arg3"),
                                         ("$obj", "$arg4"), ("$obj", "$arg5"), ("$obj", "$arg6"), ("$obj", "$arg7"),
                                         ("$obj", "$arg8"), ("$obj", 0)]),
    "Widget_applyPalette_nofont": dict(module="golf_clean.exe", addr=0x004789F0, abi="thiscall", ret="int",
                                       args=["pointer", "pointer"], fixture="c3ab_wsurf_nofont",
                                       state=[("$obj", 0xF0, 4)], vectors=[("$obj", "$arg0")]),

    "Widget_fillRectR": dict(module="golf_clean.exe", addr=0x00475B00, abi="thiscall", ret="int",
                             args=["pointer", "int"], fixture="c3ab_wsurf",
                             vectors=[("$obj", 1), ("$obj", 2), ("$obj", 5), ("$obj", -3), ("$obj", 100),
                                      ("$obj", 0), ("$obj", 0x1000), ("$obj", -1), ("$obj", 7), ("$obj", 42)]),
    "Widget_fillRectR_nofont": dict(module="golf_clean.exe", addr=0x00475B00, abi="thiscall", ret="int",
                                    args=["pointer", "int"], fixture="c3ab_wsurf_nofont", vectors=[("$obj", 5)]),

    "Widget_fillArea": dict(module="golf_clean.exe", addr=0x00478B50, abi="thiscall", ret="int",
                            args=["pointer", "int", "int"], fixture="c3ab_wsurf",
                            vectors=[("$obj", 1, 2), ("$obj", 3, 4), ("$obj", 5, 6), ("$obj", -1, 10), ("$obj", 100, 0),
                                     ("$obj", 0, 0), ("$obj", 7, 7), ("$obj", 2, -5), ("$obj", 50, 50), ("$obj", -3, -4)]),
    "Widget_fillArea_nofont": dict(module="golf_clean.exe", addr=0x00478B50, abi="thiscall", ret="int",
                                   args=["pointer", "int", "int"], fixture="c3ab_wsurf_nofont", vectors=[("$obj", 1, 2)]),
}
