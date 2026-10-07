# C3 batch c3s (2026-10-07): ui functions with static callers that the test scenarios never reach, reimplemented in
# shim/src/re/c3s.cpp. Format documented at the top of re/frida/hooks_registry.py; fixtures in
# re/frida/js/fixtures.d/c3s.js. Which side of every jcc each vector takes is listed in log/c3/c3s_purpose.md.

_TEXT = [("$s%d" % i, "$l%d" % i) for i in range(16)]

_FLAGS = [("$o%d" % i, on) for i in range(12) for on in (0, 1)]
_FLAGS += [("$o0", -1), ("$o3", 2), ("$o4", 0x100), ("$o7", -0x80000000)]

_WIN = [("$w%d" % w, v) for w in range(5) for v in (0, 1, -1, 0x12345678)]

_SIZE = [(0x2000, 0x2000), (5, 0x2000), (0x2000, 7), (100, 200), (0x1FFF, 0x2001), (-1, 0), (0, -0x2000),
         (0x7FFFFFFF, -0x80000000), (0x4000, 0x2000), (0x2000, 0x20000), (1, 1), (-0x2000, 0x2000)]

_TOKENS = [("$t1", 100), ("$t1", 115), ("$t1", 217), ("$t1", 101), ("$t1", -1), ("$t1", 500), ("$t1", 0x1000 + 100),
           ("$t2", 1000), ("$t2", 1128), ("$t2", 1255), ("$t2", 1256), ("$t2", -1),
           ("$t3", 100), ("$t3", -1)]

_LINES = [("$e0", 0, "$A", 8), ("$e0", -10, "$A", 8), ("$e0", -11, "$A", 8), ("$e0", 30, "$A", 8),
          ("$e0", 31, "$A", 8), ("$e0", 1000, "$A", 8), ("$e0", 0, "$A", 0), ("$e0", 0, "$A", -3),
          ("$e0", 1000, "$A", 3), ("$e1", 0, "$A", 8), ("$e1", 0, "$B", 2), ("$e2", 0x10, "$A", 8),
          ("$e2", 0, "$A", 8), ("$e1", 0x7FFFFFFF, "$A", 8), ("$e1", 55, "$B", 2), ("$e1", 60, "$B", 2)]

_CLICK = [("$m0", 0, 1, 2), ("$m1", 0, 0, 0), ("$m2", "$hOn", 0x11, 0x22), ("$m3", "$hOn", 0, -1),
          ("$m4", "$hOff", 0x33, 0x44), ("$m5", "$hOff", -1, 0), ("$m6", "$hNeg", 0x55, 0x66),
          ("$m7", "$hNeg", 7, 8), ("$m8", 0, -1, -1), ("$m9", "$hOn", 0x12345678, 0x9ABCDEF),
          ("$m10", "$hOff", 9, 10), ("$m11", "$hNeg", 0, 0)]

_NS = (0, 1, 2, 5, 7, 8, 15, 31, 32, 33, -1)
_BIT_TEST = [("$o%d" % o, n) for o in range(4) for n in _NS]
_BIT_SET = [("$o%d" % o, n, on) for o in range(4) for n in (0, 1, 5, 31, 32, -1) for on in (0, 1, -1)]
_TOGGLE = [("$o%d" % o, n) for o in range(4) for n in (0, 1, 2, 5, 8, 15, 31, 32, -1)]
_BITS_STATE = [("$obj", 0, 0x800)]

HOOKS = {
    # --- markup token scanners (cdecl): the length cells are written ---
    "skipToken": dict(module="golf_clean.exe", addr=0x00476D80, abi="default", ret="pointer",
                      args=["pointer", "pointer"], fixture="c3s_text", state=[("$lens", 0, 0x40)], vectors=_TEXT),
    "scanToken": dict(module="golf_clean.exe", addr=0x00476DD0, abi="default", ret="pointer",
                      args=["pointer", "pointer"], fixture="c3s_text", state=[("$lens", 0, 0x40)], vectors=_TEXT),

    # --- flag-word setters (this+0x24) ---
    "Widget::setFlagBit0": dict(module="golf_clean.exe", addr=0x00491490, abi="thiscall", ret="uint",
                                args=["pointer", "int"], fixture="c3s_flags", state=[("$obj", 0, 0x240)],
                                vectors=_FLAGS),
    "Widget::setFlagBit1": dict(module="golf_clean.exe", addr=0x004914B0, abi="thiscall", ret="uint",
                                args=["pointer", "int"], fixture="c3s_flags", state=[("$obj", 0, 0x240)],
                                vectors=_FLAGS),

    # --- child-field forwarders (child+0x5a0 of this+0x26c / this+0x270) ---
    "Window::setField26c5a0": dict(module="golf_clean.exe", addr=0x0047BA70, abi="thiscall", ret="pointer",
                                   args=["pointer", "int"], fixture="c3s_win", state=[("$obj", 0, 3 * 0x5B0)],
                                   vectors=_WIN),
    "Window::setField2705a0": dict(module="golf_clean.exe", addr=0x0047BA90, abi="thiscall", ret="pointer",
                                   args=["pointer", "int"], fixture="c3s_win", state=[("$obj", 0, 3 * 0x5B0)],
                                   vectors=_WIN),

    # --- TextView size with the 0x2000 "keep" sentinel ---
    "TextView::setSize": dict(module="golf_clean.exe", addr=0x0048E190, abi="thiscall", ret="int",
                              args=["pointer", "int", "int"], fixture="c3s_textview", state=[("$tv", 0x1FB8, 8)],
                              vectors=[("$tv", w, h) for w, h in _SIZE]),

    # --- table searches (read-only) ---
    "findMarkupTokenIndex": dict(module="golf_clean.exe", addr=0x004A4AD0, abi="thiscall", ret="int",
                                 args=["pointer", "int"], fixture="c3s_tokens", vectors=_TOKENS),
    "EditBox::lineIndexAt": dict(module="golf_clean.exe", addr=0x00486330, abi="thiscall", ret="int",
                                 args=["pointer", "int", "pointer", "int"], fixture="c3s_lines", vectors=_LINES),

    # --- click handler setter ---
    "ListModel::setClickHandler": dict(module="golf_clean.exe", addr=0x00489AB0, abi="thiscall", ret="int",
                                       args=["pointer", "pointer", "int", "int"], fixture="c3s_click",
                                       state=[("$obj", 0, 12 * 0x80)], vectors=_CLICK),

    # --- bit array behind a virtual-base table (flag dword at this + vbase + 0xf0) ---
    "bitTest": dict(module="golf_clean.exe", addr=0x0049F030, abi="thiscall", ret="uint", args=["pointer", "int"],
                    fixture="c3s_bits", vectors=_BIT_TEST),
    "bitSet": dict(module="golf_clean.exe", addr=0x0049EFF0, abi="thiscall", ret="pointer",
                   args=["pointer", "int", "int"], fixture="c3s_bits", state=_BITS_STATE, vectors=_BIT_SET),
    "flagToggle49eec0": dict(module="golf_clean.exe", addr=0x0049EEC0, abi="thiscall", ret="pointer",
                             args=["pointer", "int"], fixture="c3s_bits", state=_BITS_STATE, vectors=_TOGGLE),

    # --- per-kind field getter (switch on this+0x1f4; read-only) ---
    "getFieldValueByKind": dict(module="golf_clean.exe", addr=0x004A1370, abi="thiscall", ret="uint",
                                args=["pointer"], fixture="c3s_kind", vectors=[("$k%d" % i,) for i in range(21)]),
}
