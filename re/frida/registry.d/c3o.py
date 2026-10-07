# C3 batch c3o (2026-10-07): ui/input functions reimplemented in shim/src/re/c3o.cpp, fixtures in
# re/frida/js/fixtures.d/c3o.js. Format documented at the top of re/frida/hooks_registry.py. Keys are the hooks.csv
# names (diff_hook writes `Window::key` as Window_key). Every stub the functions reach logs into the fixture's `$rec`,
# which is a state region together with the globals each function writes, so the A/B compares the call sequences.

_WIN_STATE = [("$rec", 0, 4 + 48 * 16), (0x0083AB2C, 0, 4)]   # call log + g_curWidget

# Window::dispatchCommand 0x0047c970 (thiscall, cmd): ignore flags, no child, child answers / does not answer, a
# child whose answer sums to 0 (leafZero base 0: cmd 0 with no callback), two levels, no callback / no panel.
_CMD = [0, 1, 7, -3, 1000]
_DISPATCH = ([("$ignA", c) for c in (0, 5)] + [("$ignB", c) for c in (0, 5)]
             + [("$plain", c) for c in _CMD] + [("$bare", c) for c in (0, 7, 2)]
             + [("$parentYes", c) for c in (0, 4)] + [("$parentNo", c) for c in (0, 4)]
             + [("$parentZero", c) for c in (0, 3)] + [("$grand", c) for c in (0, 9)])

# Window::key 0x0047c5d0 (thiscall, a, vk): Enter (0x0d, 0x1000d), Esc (0x1b), Tab (9), other keys, on every
# window shape (Tab only where the focus child exists or there is no child).
_KEYS = [0x0D, 0x1000D, 0x1B, 0x09, 0x41, 0]
_KEY = ([("$ignA", 1, k) for k in (0x0D, 0x41)] + [("$ignB", 1, k) for k in (0x1B, 0x41)]
        + [("$plain", a, k) for a in (0, 2) for k in _KEYS] + [("$bare", 3, k) for k in _KEYS]
        + [("$parentYes", 1, k) for k in _KEYS] + [("$parentNo", 2, k) for k in _KEYS]
        + [("$parentZero", 0, k) for k in (0, 0x09, 0x0D)] + [("$grand", 4, k) for k in (0x41, 0x09, 0x1B)])

# Window::mouseDispatch254 0x0047c430 (thiscall, x, y, release): hot spots e0 = [0,100)x[0,100) and
# e1 = [50,150)x[50,150) (e1 is tested first); hit/miss of slot 0x44 ($plain returns 1, $miss returns 0); release.
_PTS = [(10, 10), (60, 60), (120, 120), (200, 200), (100, 10), (-1, 5), (99, 99), (149, 50)]
_MOUSE = ([("$ignA", 10, 10, 0), ("$ignA", 10, 10, 1), ("$ignB", 60, 60, 0), ("$ignB", 60, 60, 1)]
          + [("$plain", x, y, 0) for x, y in _PTS] + [("$miss", x, y, 0) for x, y in _PTS]
          + [("$bare", x, y, 0) for x, y in _PTS[:3]]
          + [("$plain", 10, 10, 1), ("$plain", 7, 8, 2), ("$bare", 5, 6, 1), ("$miss", 60, 60, -1)])

# buildToolLabel 0x0040a160 (x, y): cells seeded by c3o_label (log/c3/c3o_purpose.md lists what each one targets).
_LABEL = [(-1, 0), (50, 7), (0, 50), (2, 2), (0, 0), (3, 3), (3, 4), (10, 10), (12, 20), (4, 4), (4, 5),
          (5, 0), (5, 1), (5, 2), (5, 3), (5, 4), (5, 5), (49, 49), (6, 0), (6, 1), (6, 2), (6, 3),
          (7, 0), (7, 1), (7, 2), (7, 3), (8, 0), (8, 1), (8, 2), (8, 3), (11, 11), (20, 30)]

# drawMarkupRun 0x00477280 (thiscall, text, len): lengths are the string lengths from c3o_markup unless a shorter
# run or a run past the NUL (into the 16 zero bytes after each string) is the point of the vector.
_MARKUP = [
    ("$tplain", "$s_braces", 8), ("$tplain", "$s_plain", 3), ("$tnofont", 0, 5),
    ("$tm", "$s_plain", 8), ("$tm", "$s_plain", 0), ("$tm", "$s_plain", 20),
    ("$tm", "$s_braces", 8), ("$tm", "$s_braces", 3), ("$tm", "$s_brackets", 8), ("$tm", "$s_mixed", 9),
    ("$tm", "$s_double", 12), ("$tm", "$s_close", 4), ("$tm", "$s_closeb", 4), ("$tm", "$s_dollar", 12),
    ("$tm", "$s_link", 16), ("$tm", "$s_linknogt", 15), ("$tm", "$s_dd", 17), ("$tm", "$s_ddend", 14),
    ("$tm", "$s_dl", 17), ("$tm", "$s_caret", 10), ("$tm", "$s_eq", 8),
    ("$tpart", "$s_mixed", 9), ("$tpart", "$s_braces", 3), ("$tpart", "$s_link", 16), ("$tpart", "$s_dl", 17),
    ("$tdrop", "$s_caret", 10), ("$tdrop", "$s_plain", 8), ("$tdrop", "$s_ddbrace", 17), ("$tm", "$s_ddbrace", 17),
    ("$tlink", "$s_eq", 8), ("$tlink", "$s_caret", 10), ("$tlink", "$s_plain", 8),
    ("$tnarrow", "$s_dd", 17), ("$tnarrow", "$s_braces", 8),
    ("$tpend", "$s_plain", 8), ("$tpend", "$s_plain", 0), ("$tpend", "$s_braces", 8),
]
_MARKUP_STATE = [("$rec", 0, 4 + 48 * 16)] + [(f"${t}", 0, 0x80) for t in
                 ("tm", "tpart", "tplain", "tnofont", "tdrop", "tlink", "tnarrow", "tpend")]

# hitTestTree 0x0047f340 (cdecl: window, int* px, int* py): vector i uses window w_i and the coordinate cells
# px_i/py_i defined by c3o_hittree (its list V has _HIT_N entries; log/c3/c3o_purpose.md maps them to branches).
_HIT_N = 43
_HIT = [(f"$w{i}", f"$px{i}", f"$py{i}") for i in range(_HIT_N)]

# Window::resized 0x0047bc60 (thiscall, a, b): every parent shape of c3o_resized with a few (a, b) pairs; the state is
# the call log, g_curWidget and the block holding every child window (moveTo shifts their rects).
_RS = ["R_none", "R_H", "R_Hn", "R_V", "R_Vn", "R_HV", "R_HnV", "R_HVn", "R_HfV", "R_40", "R_cb", "R_sb"]
_RESIZED = [(f"${r}", a, b) for r in _RS for a, b in ((0, 0), (100, 50))] + [("$R_HV", -5, 7), ("$R_cb", 640, 480)]

HOOKS = {
    "Window::dispatchCommand": dict(module="golf_clean.exe", addr=0x0047C970, abi="thiscall", ret="int",
                                    args=["pointer", "int"], fixture="c3o_windows", state=_WIN_STATE,
                                    vectors=_DISPATCH),
    "Window::key": dict(module="golf_clean.exe", addr=0x0047C5D0, abi="thiscall", ret="int",
                        args=["pointer", "int", "int"], fixture="c3o_windows", state=_WIN_STATE, vectors=_KEY),
    "Window::mouseDispatch254": dict(module="golf_clean.exe", addr=0x0047C430, abi="thiscall", ret="void",
                                     args=["pointer", "int", "int", "int"], fixture="c3o_windows",
                                     state=_WIN_STATE, vectors=_MOUSE),
    "setDragTarget": dict(module="golf_clean.exe", addr=0x0047D840, abi="default", ret="void", args=["pointer"],
                          fixture="c3o_drag", state=[(0x0083AB60, 0, 4)],
                          vectors=[(f"$b{i}",) for i in range(12)]),
    "buildToolLabel": dict(module="golf_clean.exe", addr=0x0040A160, abi="default", ret="int", args=["int", "int"],
                           fixture="c3o_label", state=[(0x0051A068, 0, 0x60)], vectors=_LABEL),
    "drawMarkupRun": dict(module="golf_clean.exe", addr=0x00477280, abi="thiscall", ret="int",
                          args=["pointer", "pointer", "int"], fixture="c3o_markup", state=_MARKUP_STATE,
                          vectors=_MARKUP),
    "Window::resized": dict(module="golf_clean.exe", addr=0x0047BC60, abi="thiscall", ret="void",
                            args=["pointer", "int", "int"], fixture="c3o_resized",
                            state=[("$rec", 0, 4 + 48 * 16), (0x0083AB2C, 0, 4), ("$kids", 0, 8 * 0x280)],
                            vectors=_RESIZED),
    "hitTestTree": dict(module="golf_clean.exe", addr=0x0047F340, abi="default", ret="pointer",
                        args=["pointer", "pointer", "pointer"], fixture="c3o_hittree",
                        state=[("$cells", 0, _HIT_N * 8), ("$rec", 0, 4 + 48 * 16), (0x0083AB18, 0, 4)],
                        vectors=_HIT),
}
