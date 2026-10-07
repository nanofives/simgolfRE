# C3 batch c3ae (revisit, 2026-10-08): A/B vectors for the eight functions in shim/src/re/c3ae.cpp.
# Keys are the hooks.csv names (variants add a suffix; the gate sums every log/diff/<addr>_*.path1.csv of an address).
# Callees are in other, already-promoted C3 batches or are CRT imports, so NO callee is hooked by THIS batch: on the
# instance those batches are unlinked (both arms run the originals); on the main build they are hooked (both arms run
# the same reimpl). No SIMGOLF_HOOKS_OFF is needed for any key.
#   rateLot            -> clearCost 0x0042ee80 (c3ab), distance 0x0040acd0 (c3b)
#   matchDirective     -> trimSpaces 0x004925b0 (c3ab), CRT __strnicmp 0x004ad580
#   sceneryLabel       -> decorationName 0x00407700 (c3m)
#   TextView_endSegment-> Widget_value 0x00477580 (c3c)
#   Button::setColor*  -> Widget_applyPalette 0x004789f0 (c3ab), Widget_setQuad 0x00476310 (c3c) / 74 0x00476370 (c3x)
#   Widget_fillRect / Widget_get -> none of our batches (indirect vtable slot / leaf getter)

# rateLot (x, y): 10 lot positions inside the seeded 50x50 tile field; each samples a different 12-cell ring and a
# different distance to the seeded records. Same positions under the aura variant (global 0x00543cd0 != 0).
_RATE_XY = [(5, 5), (5, 10), (10, 5), (12, 8), (20, 20), (25, 13), (30, 30), (40, 40), (8, 40), (45, 8)]

# TextView_endSegment and Widget_get take only `this` (thiscall); one call per fixture/this.
_SELF = [("$obj",)]

# Button color setters: four quad components; the active fixture writes them into the font object, the _off fixture
# (this+0x130 == 0) changes nothing.
_QUAD = [(0x10203040, 1, 2, 3), (0x00ff00ff, 4, 5, 6), (0x7fffffff, -1, -2, -3), (0, 0, 0, 0),
         (0x12345678, 10, 20, 30), (-1, 100, 200, 255), (0x00c08040, 7, 8, 9)]

HOOKS = {
    "rateLot": dict(module="golf_clean.exe", addr=0x0042EF40, abi="default", ret="int", args=["int", "int"],
                    fixture="c3ae_ratelot", vectors=_RATE_XY),
    "rateLot_aura": dict(module="golf_clean.exe", addr=0x0042EF40, abi="default", ret="int", args=["int", "int"],
                         fixture="c3ae_ratelot_aura", vectors=_RATE_XY),

    # matchDirective(pp): each vector passes its own holder cell (char**); state lists every holder (the advanced *pp)
    # and every buffer (trimSpaces edits it in place). h0..h5 start with a real keyword copied from the binary table;
    # h6/h7 start with non-matching text (return -1, *pp unchanged).
    "matchDirective": dict(module="golf_clean.exe", addr=0x0048CD80, abi="default", ret="int", args=["pointer"],
                           fixture="c3ae_dir",
                           state=[("$h%d" % i, 0, 4) for i in range(8)] + [("$b%d" % i, 0, 64) for i in range(8)],
                           vectors=[("$h%d" % i,) for i in range(8)] + [(0,)]),  # (0,) = NULL pp -> returns -1

    # sceneryLabel(a, b, text): matches (5,7)->idx0, (8,9)->idx1, (10,11)->idx2 in the seeded 0x005689e8 table; the
    # scratch buffer 0x0051a068 is the state. Covers match+text (append name+sep+decorationName), match+no-text
    # (return 1), no-match+text (append "Scenic"), no-match+no-text (return 0), and a-matches/b-differs (no match).
    "sceneryLabel": dict(module="golf_clean.exe", addr=0x00407B60, abi="default", ret="int",
                         args=["int", "int", "int"], fixture="c3ae_scenery", state=[(0x0051A068, 0, 0x100)],
                         vectors=[(5, 7, 1), (5, 7, 0), (8, 9, 1), (10, 11, 1), (99, 99, 1), (99, 99, 0), (5, 99, 1)]),

    # TextView_endSegment(this): one call per global/this setting. _onpos adds Widget_value (its +8>=0 branch, returns
    # [+0x10]+[+8]); _onneg its +8<0 branch (returns [+0xc]); _off leaves the pen unchanged (flag 0x00839aa8 == 0).
    "TextView_endSegment": dict(module="golf_clean.exe", addr=0x00478700, abi="thiscall", ret="int",
                                args=["pointer"], fixture="c3ae_endseg_onpos",
                                state=[(0x00839AA0, 0, 0xC), ("$obj", 0x1C, 4), ("$obj", 0x30, 0x10)], vectors=_SELF),
    "TextView_endSegment_onneg": dict(module="golf_clean.exe", addr=0x00478700, abi="thiscall", ret="int",
                                      args=["pointer"], fixture="c3ae_endseg_onneg",
                                      state=[(0x00839AA0, 0, 0xC), ("$obj", 0x1C, 4), ("$obj", 0x30, 0x10)],
                                      vectors=_SELF),
    "TextView_endSegment_off": dict(module="golf_clean.exe", addr=0x00478700, abi="thiscall", ret="int",
                                    args=["pointer"], fixture="c3ae_endseg_off",
                                    state=[(0x00839AA0, 0, 0xC), ("$obj", 0x1C, 4), ("$obj", 0x30, 0x10)],
                                    vectors=_SELF),

    # Button::setColorNormal / setColorPressed: font quad at this+0x274 is the state. _off fixture (this+0x130 == 0)
    # does nothing. The font object this+0x274 has +4 == 0, so Widget_applyPalette returns 7 (no vtable) in both arms.
    "Button::setColorNormal": dict(module="golf_clean.exe", addr=0x00488930, abi="thiscall", ret="void",
                                   args=["pointer", "int", "int", "int", "int"], fixture="c3ae_btn",
                                   state=[("$obj", 0x2E0, 0x40)], vectors=[("$obj",) + q for q in _QUAD]),
    "Button::setColorNormal_off": dict(module="golf_clean.exe", addr=0x00488930, abi="thiscall", ret="void",
                                       args=["pointer", "int", "int", "int", "int"], fixture="c3ae_btn_off",
                                       state=[("$obj", 0x2E0, 0x40)], vectors=[("$obj", 1, 2, 3, 4)]),
    "Button::setColorPressed": dict(module="golf_clean.exe", addr=0x004889B0, abi="thiscall", ret="void",
                                    args=["pointer", "int", "int", "int", "int"], fixture="c3ae_btn",
                                    state=[("$obj", 0x2E0, 0x40)], vectors=[("$obj",) + q for q in _QUAD]),
    "Button::setColorPressed_off": dict(module="golf_clean.exe", addr=0x004889B0, abi="thiscall", ret="void",
                                        args=["pointer", "int", "int", "int", "int"], fixture="c3ae_btn_off",
                                        state=[("$obj", 0x2E0, 0x40)], vectors=[("$obj", 1, 2, 3, 4)]),

    # Widget_fillRect(this, p1..p5): the fake surface this+4 has a slot-25 callback that writes p1*7+p2*5+p3*3+p4+p5
    # into surface+0x80 (state). _nosurf (this+4 == 0) changes nothing.
    "Widget_fillRect": dict(module="golf_clean.exe", addr=0x00478B80, abi="thiscall", ret="void",
                            args=["pointer", "int", "int", "int", "int", "int"], fixture="c3ae_fr",
                            state=[("$surf", 0x80, 4)],
                            vectors=[("$obj", 1, 2, 3, 4, 5), ("$obj", 10, 0, 0, 0, 0), ("$obj", -1, -2, -3, -4, -5),
                                     ("$obj", 100, 200, 300, 400, 500), ("$obj", 0, 0, 0, 0, 0),
                                     ("$obj", 7, 7, 7, 7, 7), ("$obj", 0x1000, 1, 1, 1, 1),
                                     ("$obj", -100, 50, -25, 12, -6), ("$obj", 3, 6, 9, 12, 15),
                                     ("$obj", 42, 43, 44, 45, 46)]),
    "Widget_fillRect_nosurf": dict(module="golf_clean.exe", addr=0x00478B80, abi="thiscall", ret="void",
                                   args=["pointer", "int", "int", "int", "int", "int"], fixture="c3ae_fr_nosurf",
                                   state=[("$surf", 0x80, 4)], vectors=[("$obj", 1, 2, 3, 4, 5)]),

    # Widget_get(this): leaf (no callees) -> 11 vectors. g0..g9 return their seeded sub-object +0x10; gnull hits the
    # null branch (reads the live default font 0x0083ad44 and stores it at this+0x5c, a state region).
    "Widget_get": dict(module="golf_clean.exe", addr=0x00477560, abi="thiscall", ret="int", args=["pointer"],
                       fixture="c3ae_get", state=[("$gnull", 0x5C, 4)],
                       vectors=[("$g%d" % i,) for i in range(10)] + [("$gnull",)]),
}
