# C3 batch c3u (2026-10-07): UI widget helpers of golf_clean.exe the test scenarios never reach but that have static
# callers. List seek/find helpers (listFindById, ListModel::selectedId), a character-editor hit-test (charEditorZone),
# a combo index reader (comboCurrentIndex), a clamped scalar store (setWidgetScalar), a HUD text-slot writer
# (setHudTextSlot), and two button setters (Button::setMode, Button::setColorHover). Reimplementations in shim/src/re/c3u.cpp; fixtures in
# re/frida/js/fixtures.d/c3u.js; per-jcc branch coverage in log/c3/c3u_purpose.md. Registry keys are the hooks.csv names.

# listFindById(this, key): one list of five nodes with ids 10,20,30,40,50 (fixture c3u_list). Vectors cover each found
# position, a key absent from the list, and 0. The _empty variant (fixture c3u_list_empty, null head) covers the
# head == 0 guard, which returns the index field unchanged.
_FIND = [(10,), (20,), (30,), (40,), (50,), (99,), (0,), (25,), (-1,)]
_FIND = [("$obj", k) for (k,) in _FIND]

# ListModel::selectedId(this): eleven objects o0..o10 (fixture c3u_sel), each a 6-node circular doubly-linked list with
# ids 100..600 and a different selection index this+0xf0 reaching a distinct node or branch side: o0 sel 0, o1 sel 1,
# o2 sel 2, o3 sel 4, o4 sel 5 (forward walks to nodes 0,1,2,4,5), o5 sel 6 (> count-1: seek skipped, cursor stays at
# head), o6 sel -1, o7 sel -3, o8 sel -5 (backward walks to nodes 5,3,1), o9 sel -7 (abs > count: seek returns before
# walking), o10 null head (returns 0). Argument-free, so one object per vector (leaf rule: >= 10 vectors).
_SEL = [("$o%d" % i,) for i in range(11)]
_SEL_STATE = [("$o%d" % i, 0xc8, 0x30) for i in range(11)]

# charEditorZone(x, y): pure hit-test over the 10-centre table at 0x004c7be0 plus two fixed points. Vectors land on
# each zone centre (centre + 0x3c), on the two special points, and far outside (-1). No fixture, no state.
_ZONES = [(29, 305), (97, 425), (165, 305), (233, 425), (301, 305), (369, 425), (437, 305), (505, 425), (573, 305),
          (641, 425)]
_ZONE = [(cx + 0x3c, cy + 0x3c) for (cx, cy) in _ZONES]            # 0..9
_ZONE += [(0x30a, 0x16b), (0x12, 0x1ef)]                          # 10, 11
_ZONE += [(0, 0), (5000, 5000), (-1000, -1000), (1000, 1000), (400, 0)]  # clear misses -> -1

# comboCurrentIndex(this): eleven objects o0..o10 (fixture c3u_combo). Byte +4 bit 2 chooses the list; o1 is the set
# list empty and o3 the clear list empty (both return 0 but via the two different count==0 branches); the other nine hit
# with distinct ids (111,222,333,444,555,666,777,888,999) alternating between the set (+0x1488) and clear (+0x2d98)
# lists. Argument-free, so one object per vector (leaf rule: >= 10 vectors).
_COMBO = [("$o%d" % i,) for i in range(11)]

# setWidgetScalar(this, value): clamp range [10, 100] (fixture c3u_scalar); mirror variant sets this+0x588 (c3u_scalar_mirror).
_SCALAR = [("$obj", v) for v in (-5, 0, 9, 10, 11, 50, 99, 100, 101, 200, -0x80000000, 0x7FFFFFFF)]
_SCALAR_STATE = [("$obj", 0x580, 0x14), (0x0083ab2c, 0, 4), ("$rec", 0, 0x40)]

# setHudTextSlot(idx, text, x, y): writes colour arrays 0x0083e8b8/0x0083d3c8 and the 0x100-byte slot text buffers at
# 0x0083e8e0; fixture c3u_hud supplies the synthetic strings t0..t3. Vectors cover valid slots with positive and
# negative colours (the default-colour branches), the idx>9 guard and the null-text guard.
_HUD = [(0, "$t0", 0x101, 0x202), (1, "$t1", -1, 0x303), (2, "$t2", 0x404, -1), (3, "$t3", -1, -1),
        (9, "$t1", 0x505, 0x606), (5, "$t2", 0, 0), (0, "$t3", 0x111, 0x222), (10, "$t1", 1, 2),
        (0, 0, 1, 2), (7, "$t0", 0x7fffffff, 0x1234), (4, "$t2", -5, -9), (9, "$t0", 0, 0)]
_HUD_STATE = [(0x0083e8e0, 0, 0xa00), (0x0083e8b8, 0, 0x28), (0x0083d3c8, 0, 0x28)]

# Button::setMode(this, mode): object o has a parent control (this+0x130), o2 does not (fixture c3u_setmode). Vectors
# change the mode, repeat the current mode (the no-op branch), and set a mode on the parent-less object.
_SETMODE = [("$o", 1), ("$o", 2), ("$o", 2), ("$o", 7), ("$o2", 1), ("$o2", 5), ("$o", 0), ("$o2", 0)]
_SETMODE_STATE = [("$o", 0x578, 4), ("$o2", 0x578, 4), ("$rec", 0, 0x40)]

# Button::setColorHover(this, c0..c3): object o is active (this+0x130 set, font this+0x274 with +4 null so the palette
# helper is a no-op), o2 is inactive (this+0x130 null). Fixture c3u_hover.
_HOVER = [("$o", 0x11111111, 0x22222222, 0x33333333, 0x44444444), ("$o", 1, 2, 3, 4),
          ("$o", 0xdeadbeef, 0, 0xffffffff, 0x80000000), ("$o2", 9, 9, 9, 9), ("$o2", 0, 0, 0, 0)]
_HOVER_STATE = [("$o", 0x274, 0xb0), ("$o2", 0x274, 0xb0)]

HOOKS = {
    "listFindById": dict(module="golf_clean.exe", addr=0x004A4890, abi="thiscall", ret="int",
                         args=["pointer", "int"], fixture="c3u_list", state=[("$obj", 0x80, 0x10)], vectors=_FIND),
    "listFindById_empty": dict(module="golf_clean.exe", addr=0x004A4890, abi="thiscall", ret="int",
                               args=["pointer", "int"], fixture="c3u_list_empty", state=[("$obj", 0x80, 0x10)],
                               vectors=[("$obj", 10), ("$obj", 99)]),
    "ListModel::selectedId": dict(module="golf_clean.exe", addr=0x00489950, abi="thiscall", ret="uint32",
                                  args=["pointer"], fixture="c3u_sel", state=_SEL_STATE, vectors=_SEL),
    "charEditorZone": dict(module="golf_clean.exe", addr=0x004382F0, abi="default", ret="int", args=["int", "int"],
                           vectors=_ZONE),
    "comboCurrentIndex": dict(module="golf_clean.exe", addr=0x004942A0, abi="thiscall", ret="int", args=["pointer"],
                              fixture="c3u_combo", vectors=_COMBO),
    "setWidgetScalar": dict(module="golf_clean.exe", addr=0x004967F0, abi="thiscall", ret="void", args=["pointer", "int"],
                            fixture="c3u_scalar", state=_SCALAR_STATE, vectors=_SCALAR),
    "setWidgetScalar_mirror": dict(module="golf_clean.exe", addr=0x004967F0, abi="thiscall", ret="void",
                                   args=["pointer", "int"], fixture="c3u_scalar_mirror", state=_SCALAR_STATE,
                                   vectors=_SCALAR),
    "setHudTextSlot": dict(module="golf_clean.exe", addr=0x00494CB0, abi="default", ret="int",
                           args=["int", "pointer", "int", "int"], fixture="c3u_hud", state=_HUD_STATE, vectors=_HUD),
    "Button::setMode": dict(module="golf_clean.exe", addr=0x004890E0, abi="thiscall", ret="void",
                            args=["pointer", "int"], fixture="c3u_setmode", state=_SETMODE_STATE, vectors=_SETMODE),
    "Button::setColorHover": dict(module="golf_clean.exe", addr=0x00488970, abi="thiscall", ret="void",
                                  args=["pointer", "uint32", "uint32", "uint32", "uint32"], fixture="c3u_hover",
                                  state=_HOVER_STATE, vectors=_HOVER),
}
