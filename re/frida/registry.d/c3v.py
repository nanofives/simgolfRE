# C3 batch c3v (2026-10-07): UI functions with static callers that the test scenarios never reach: three ListModel
# list walkers (findId / indexOfId / idAt over a circular doubly-linked list), scrollbarResetState, Window grow/shrink
# rect transforms, Widget::setValue, EditBox::setText and nearestCharControl. Reimplemented in shim/src/re/c3v.cpp;
# fixtures in re/frida/js/fixtures.d/c3v.js; which side of every jcc each vector takes is in log/c3/c3v_purpose.md.

# ListModel over c3v_list (6 nodes, data 100..600). The model fields that change are inside 0xc8..0xf8.
_LIST_STATE = [("$lm", 0xc8, 0x30)]
# findId / indexOfId keys: the six present ids plus absent ones (return/record count 6).
_FIND = [("$lm", k) for k in (100, 200, 300, 400, 500, 600, 0, -1, 101, 999, 12345, 50)]
# idAt indices: forward 0..5, negative -1/-2/-6, past the end 6/10 (seek skipped, head returned).
_IDAT = [("$lm", i) for i in (0, 1, 2, 3, 4, 5, -1, -2, -6, -7, 6, 10)]
# Empty-list probes (head +0xc8 = 0): index -1 enters the seek (abs 1 > count 0 -> skip, returns 0), 0 skips it.
_IDAT_EMPTY = [("$lm", i) for i in (-1, 0, 5)]
_FIND_EMPTY = [("$lm", k) for k in (100, 0, -1)]

# scrollbarResetState: the twelve c3v_scroll objects; each writes 0x5ac..0x5bc.
_SCROLL_STATE = [("$sb%d" % i, 0x5ac, 0x10) for i in range(12)]
_SCROLL = [("$sb%d" % i,) for i in range(12)]

# grow/shrinkRect over c3v_rect: the three rectangles are the state; vectors pair a window with a rectangle.
_RECT_STATE = [("$r%d" % j, 0, 0x10) for j in range(3)]
_RECT = [("$w%d" % i, "$r0") for i in range(12)] + [("$w5", "$r1"), ("$w11", "$r2"), ("$w3", "$r2"), ("$w0", 0)]

# Widget::setValue over c3v_setvalue: state is the recorder and each object's value slot +0x184.
_SV_STATE = [("$rec", 0, 0x40)] + [("$sv%d" % i, 0x184, 4) for i in range(4)]
_SV = [("$sv0", v) for v in (0, 1, 7, -3, 100, 0x7FFFFFFF)] + \
      [("$sv2", v) for v in (5, -10, 42)] + [("$sv3", 9)] + [("$sv1", 4), ("$sv1", 0)]

# EditBox::setText over c3v_settext: state is the 32-byte buffer, the length slot +0x5a4 and the recorder.
_ST_STATE = [("$buf", 0, 0x20), ("$st0", 0x5a4, 4), ("$rec", 0, 0x40)]
_ST = [("$st0", "$t%d" % i) for i in range(5)] + [("$st0", 0), ("$st1", "$t2"), ("$st1", 0), ("$st0", "$t1")]

# nearestCharControl: points near several table entries (take the d < best branch at various indices) and far points
# (never take it, index -1). Both the i < 8 (x/3) and i >= 8 (raw) table halves are walked on every call.
_NEAR = [(210, 150), (240, 185), (93, 127), (93, 239), (48, 22), (258, 22), (454, 30), (454, 230), (328, 107),
         (762, 33), (760, 220), (580, 98), (300, 300), (500, 500), (100, 50), (400, 150), (900, 100),
         (2000, 2000), (-500, -500), (0, 0), (210, 220), (328, 157)]

HOOKS = {
    "ListModel::findId": dict(module="golf_clean.exe", addr=0x004899D0, abi="thiscall", ret="int",
                              args=["pointer", "int"], fixture="c3v_list", state=_LIST_STATE, vectors=_FIND),
    "ListModel::indexOfId": dict(module="golf_clean.exe", addr=0x004898D0, abi="thiscall", ret="void",
                                 args=["pointer", "int"], fixture="c3v_list", state=_LIST_STATE, vectors=_FIND),
    "ListModel::idAt": dict(module="golf_clean.exe", addr=0x00489A30, abi="thiscall", ret="int",
                            args=["pointer", "int"], fixture="c3v_list", state=_LIST_STATE, vectors=_IDAT),
    "ListModel::findId_empty": dict(module="golf_clean.exe", addr=0x004899D0, abi="thiscall", ret="int",
                                    args=["pointer", "int"], fixture="c3v_list_empty", state=_LIST_STATE,
                                    vectors=_FIND_EMPTY),
    "ListModel::indexOfId_empty": dict(module="golf_clean.exe", addr=0x004898D0, abi="thiscall", ret="void",
                                       args=["pointer", "int"], fixture="c3v_list_empty", state=_LIST_STATE,
                                       vectors=_FIND_EMPTY),
    "ListModel::idAt_empty": dict(module="golf_clean.exe", addr=0x00489A30, abi="thiscall", ret="int",
                                  args=["pointer", "int"], fixture="c3v_list_empty", state=_LIST_STATE,
                                  vectors=_IDAT_EMPTY),
    "scrollbarResetState": dict(module="golf_clean.exe", addr=0x004979A0, abi="thiscall", ret="void",
                                args=["pointer"], fixture="c3v_scroll", state=_SCROLL_STATE, vectors=_SCROLL),
    "Window::growRect": dict(module="golf_clean.exe", addr=0x0047CC10, abi="thiscall", ret="void",
                             args=["pointer", "pointer"], fixture="c3v_rect", state=_RECT_STATE, vectors=_RECT),
    "Window::shrinkRect": dict(module="golf_clean.exe", addr=0x0047CCE0, abi="thiscall", ret="void",
                               args=["pointer", "pointer"], fixture="c3v_rect", state=_RECT_STATE, vectors=_RECT),
    "Widget::setValue": dict(module="golf_clean.exe", addr=0x0047D020, abi="thiscall", ret="void",
                             args=["pointer", "int"], fixture="c3v_setvalue", state=_SV_STATE, vectors=_SV),
    "EditBox::setText": dict(module="golf_clean.exe", addr=0x00486200, abi="thiscall", ret="void",
                             args=["pointer", "pointer"], fixture="c3v_settext", state=_ST_STATE, vectors=_ST),
    "nearestCharControl": dict(module="golf_clean.exe", addr=0x00438260, abi="default", ret="int",
                               args=["int", "int"], vectors=_NEAR),
}
