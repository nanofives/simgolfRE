# C3 batch c3y (2026-10-07): ui functions statically called but never reached by the test scenarios, reimplemented in
# shim/src/re/c3y.cpp. Fixtures in re/frida/js/fixtures.d/c3y.js; per-jcc coverage in log/c3/c3y_purpose.md. Keys are
# filename-safe underscore forms (c3_verify/diff_hook key off the address). Window::adjustSizeOuter and comboFindItem
# are leaves (>= 10 vectors each across their keys). The three constructors call ListModel::ctor (0x00489150), a
# batch-mate hook: the verifier must re-run ListBox_ctor/viewFieldCtor4a2250 with SIMGOLF_HOOKS_OFF=00489150 so their
# original arm uses the original ListModel::ctor (ListModel_ctor itself has its own keys).

# adjustSizeOuter 0x0047ca10: scratch holds (a, b) int cells; the function adds to *a/*b from the window's flags and
# border fields. The 32-byte scratch is the state region; each vector points at one (a, b) pair.
_ADJ_STATE = [("$scratch", 0, 0x20)]
_ADJ_VEC = [("$obj", "$a0", "$b0"), ("$obj", "$a1", "$b1"), ("$obj", "$a2", "$b2"), ("$obj", "$a3", "$b3")]

# comboFindItem 0x004940e0: keys match node ids 10, 20, 30, 40 and a miss (99); the whole 0x3000 object is the state
# region (the walk writes the cursor and index in the list base). thiscall: first arg is `this`.
_COMBO_STATE = [("$obj", 0, 0x3000)]
_COMBO_KEYS = [("$obj", k) for k in (10, 20, 30, 40, 99, 0, -1, 11)]

# ListModel::ctor 0x00489150: argument-free, so one registry entry per fixture variant (distinct pre-fill + cookie).
# The object (0x200) and the global list cookie at 0x00839650 (saved into +0xec, then zeroed) are the state region.
_LM_STATE = [("$obj", 0, 0x200), (0x00839650, 0, 4)]
# ListBox::ctor / viewFieldCtor: the object (0x800, holds the Widget and ListModel sub-objects) plus the cookie.
_LB_STATE = [("$obj", 0, 0x800), (0x00839650, 0, 4)]

HOOKS = {
    # --- Window::adjustSizeOuter 0x0047ca10 (writer of *a/*b; one variant per flag path) ---
    "Window_adjustSizeOuter": dict(module="golf_clean.exe", addr=0x0047CA10, abi="thiscall", ret="void",
                                   args=["pointer", "pointer", "pointer"], fixture="c3y_win_flat", state=_ADJ_STATE,
                                   vectors=_ADJ_VEC),
    "Window_adjustSizeOuter_b4": dict(module="golf_clean.exe", addr=0x0047CA10, abi="thiscall", ret="void",
                                      args=["pointer", "pointer", "pointer"], fixture="c3y_win_b4", state=_ADJ_STATE,
                                      vectors=_ADJ_VEC),
    "Window_adjustSizeOuter_b8": dict(module="golf_clean.exe", addr=0x0047CA10, abi="thiscall", ret="void",
                                      args=["pointer", "pointer", "pointer"], fixture="c3y_win_b8", state=_ADJ_STATE,
                                      vectors=_ADJ_VEC),
    "Window_adjustSizeOuter_b400": dict(module="golf_clean.exe", addr=0x0047CA10, abi="thiscall", ret="void",
                                        args=["pointer", "pointer", "pointer"], fixture="c3y_win_b400", state=_ADJ_STATE,
                                        vectors=_ADJ_VEC),
    "Window_adjustSizeOuter_b400h1": dict(module="golf_clean.exe", addr=0x0047CA10, abi="thiscall", ret="void",
                                          args=["pointer", "pointer", "pointer"], fixture="c3y_win_b400h1",
                                          state=_ADJ_STATE, vectors=_ADJ_VEC),
    "Window_adjustSizeOuter_b10": dict(module="golf_clean.exe", addr=0x0047CA10, abi="thiscall", ret="void",
                                       args=["pointer", "pointer", "pointer"], fixture="c3y_win_b10", state=_ADJ_STATE,
                                       vectors=_ADJ_VEC),
    "Window_adjustSizeOuter_child": dict(module="golf_clean.exe", addr=0x0047CA10, abi="thiscall", ret="void",
                                         args=["pointer", "pointer", "pointer"], fixture="c3y_win_child",
                                         state=_ADJ_STATE, vectors=_ADJ_VEC),
    "Window_adjustSizeOuter_childskip": dict(module="golf_clean.exe", addr=0x0047CA10, abi="thiscall", ret="void",
                                             args=["pointer", "pointer", "pointer"], fixture="c3y_win_childskip",
                                             state=_ADJ_STATE, vectors=_ADJ_VEC),
    # Null a / null b exercise the two early returns (0x0047ca19 / 0x0047ca25); nothing is written.
    "Window_adjustSizeOuter_null": dict(module="golf_clean.exe", addr=0x0047CA10, abi="thiscall", ret="void",
                                        args=["pointer", "pointer", "pointer"], fixture="c3y_win_b4", state=_ADJ_STATE,
                                        vectors=[("$obj", 0, "$b0"), ("$obj", "$a0", 0)]),

    # --- comboFindItem 0x004940e0 (returns a node value; mutates the list cursor/index) ---
    "comboFindItem": dict(module="golf_clean.exe", addr=0x004940E0, abi="thiscall", ret="int", args=["pointer", "int"],
                          fixture="c3y_combo_a", state=_COMBO_STATE, vectors=_COMBO_KEYS),
    "comboFindItem_b": dict(module="golf_clean.exe", addr=0x004940E0, abi="thiscall", ret="int", args=["pointer", "int"],
                            fixture="c3y_combo_b", state=_COMBO_STATE, vectors=_COMBO_KEYS),
    "comboFindItem_empty": dict(module="golf_clean.exe", addr=0x004940E0, abi="thiscall", ret="int", args=["pointer", "int"],
                                fixture="c3y_combo_empty", state=_COMBO_STATE, vectors=[("$obj", 10), ("$obj", 0)]),
    "comboFindItem_count0": dict(module="golf_clean.exe", addr=0x004940E0, abi="thiscall", ret="int", args=["pointer", "int"],
                                 fixture="c3y_combo_count0", state=_COMBO_STATE, vectors=[("$obj", 10), ("$obj", 0)]),

    # --- ListModel::ctor 0x00489150 (field-initialising ctor; argument-free, one entry per fixture) ---
    "ListModel_ctor": dict(module="golf_clean.exe", addr=0x00489150, abi="thiscall", ret="pointer", args=["pointer"],
                           fixture="c3y_lm_a", state=_LM_STATE, vectors=[("$obj",)]),
    "ListModel_ctor_b": dict(module="golf_clean.exe", addr=0x00489150, abi="thiscall", ret="pointer", args=["pointer"],
                             fixture="c3y_lm_b", state=_LM_STATE, vectors=[("$obj",)]),
    "ListModel_ctor_c": dict(module="golf_clean.exe", addr=0x00489150, abi="thiscall", ret="pointer", args=["pointer"],
                             fixture="c3y_lm_c", state=_LM_STATE, vectors=[("$obj",)]),

    # --- ListBox::ctor 0x00489cb0 (build=1 runs the sub-ctors, build=0 skips them) ---
    "ListBox_ctor": dict(module="golf_clean.exe", addr=0x00489CB0, abi="thiscall", ret="pointer", args=["pointer", "int"],
                         fixture="c3y_listbox", state=_LB_STATE, vectors=[("$obj", 1)]),
    "ListBox_ctor_b0": dict(module="golf_clean.exe", addr=0x00489CB0, abi="thiscall", ret="pointer", args=["pointer", "int"],
                            fixture="c3y_listbox_b0", state=_LB_STATE, vectors=[("$obj", 0)]),

    # --- viewFieldCtor4a2250 0x004a2250 (build=1 runs the sub-ctors, build=0 skips them) ---
    "viewFieldCtor4a2250": dict(module="golf_clean.exe", addr=0x004A2250, abi="thiscall", ret="pointer", args=["pointer", "int"],
                                fixture="c3y_viewfield", state=_LB_STATE, vectors=[("$obj", 1)]),
    "viewFieldCtor4a2250_b0": dict(module="golf_clean.exe", addr=0x004A2250, abi="thiscall", ret="pointer",
                                   args=["pointer", "int"], fixture="c3y_viewfield_b0", state=_LB_STATE,
                                   vectors=[("$obj", 0)]),
}
